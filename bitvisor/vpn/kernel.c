/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <core.h>
#include <core/arith.h>
#include <core/cpu.h>
#include <core/iccard.h>
#include <core/process.h>
#include <core/time.h>
#include <core/timer.h>
#include <net/netapi.h>
#include <IDMan.h>
#include <Se/Se.h>
#include "vpn_msg.h"

#if USE_ADVISOR
#include <stdint.h>
#include "vga.h"
#endif

#define NUM_OF_HANDLE 32

struct vpncallback {
	SE_HANDLE nic_handle;
	SE_SYS_CALLBACK_RECV_NIC *callback;
	void *param;
};

struct vpndata {
	SE_HANDLE vpn_handle;
	void *ph, *vh;
	struct nicfunc pf, vf;
	struct vpncallback pc, vc;
};

#ifdef VPN_PD
static struct mempool *mp;
static int vpnkernel_desc, desc;
static void *handle[NUM_OF_HANDLE];
static spinlock_t handle_lock;	/* new only */
static SE_HANDLE vpn_timer_handle;

static void
callsub (int c, struct msgbuf *buf, int bufcnt)
{
	int r;

	for (;;) {
		r = msgsendbuf (desc, c, buf, bufcnt);
		if (r == 0)
			break;
		panic ("vpn msgsendbuf failed (%d)", c);
	}
}

void
vpn_user_init (struct config_data_vpn *vpn, char *seed, int len)
{
	int d, d1;
	struct config_data_vpn *p;
	char *q;
	struct msgbuf buf[2];

	mp = mempool_new (0, 1, true);
	d = newprocess ("vpn");
	if (d < 0)
		panic ("newprocess vpn");
	d1 = msgopen ("ttyout");
	if (d1 < 0)
		panic ("msgopen ttyout");
	msgsenddesc (d, d1);
	msgsenddesc (d, d1);
	msgclose (d1);
	p = mempool_allocmem (mp, sizeof *p);
	memcpy (p, vpn, sizeof *p);
	q = mempool_allocmem (mp, len);
	memcpy (q, seed, len);
	setmsgbuf (&buf[0], p, sizeof *p, 0);
	setmsgbuf (&buf[1], q, sizeof *q, 0);
	if (msgsendbuf (d, 0, buf, 2))
		panic ("vpn init failed");
	mempool_freemem (mp, q);
	mempool_freemem (mp, p);
	msgclose (d);
}

SE_HANDLE
vpn_start (void *data)
{
	struct vpn_msg_start *arg;
	struct msgbuf buf[1];
	SE_HANDLE ret;
	int i;

	spinlock_lock (&handle_lock);
	for (i = 0; i < NUM_OF_HANDLE; i++)
		if (handle[i] == NULL)
			goto found;
	panic ("vpn_start: handle full");
found:
	handle[i] = data;
	spinlock_unlock (&handle_lock);
	arg = mempool_allocmem (mp, sizeof *arg);
	arg->handle = i;
	arg->cpu = get_cpu_id ();
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	callsub (VPN_MSG_START, buf, 1);
	ret = arg->retval;
	mempool_freemem (mp, arg);
	return ret;
}

static void
sendphysicalnicrecv_premap (SE_HANDLE nic_handle, UINT num_packets,
			    void **packets, UINT *packet_sizes, void *param,
			    long *premap)
{
	struct vpn_msg_physicalnicrecv *arg;
	struct msgbuf *buf;
	UINT i;

	arg = mempool_allocmem (mp, sizeof *arg);
	arg->nic_handle = nic_handle;
	arg->param = param;
	arg->num_packets = num_packets;
	arg->cpu = get_cpu_id ();
	buf = alloc (sizeof *buf * (1 + num_packets));
	setmsgbuf (&buf[0], arg, sizeof *arg, 0);
	if (premap) {
		for (i = 0; i < num_packets; i++)
			setmsgbuf_premap (&buf[1 + i], packets[i],
					  packet_sizes[i], 0, premap[i]);
	} else {
		for (i = 0; i < num_packets; i++)
			setmsgbuf (&buf[1 + i], packets[i], packet_sizes[i],
				   0);
	}
	callsub (VPN_MSG_PHYSICALNICRECV, buf, num_packets + 1);
	free (buf);
	mempool_freemem (mp, arg);
}

static void
sendphysicalnicrecv_wrapper (void *handle, unsigned int num_packets,
			     void **packets, unsigned int *packet_sizes,
			     void *param, long *premap)
{
	struct vpncallback *c = param;

	sendphysicalnicrecv_premap (c->nic_handle, num_packets, packets,
				    packet_sizes, c->param, premap);
}

static void
sendphysicalnicrecv (SE_HANDLE nic_handle, UINT num_packets, void **packets,
		     UINT *packet_sizes, void *param)
{
	sendphysicalnicrecv_premap (nic_handle, num_packets, packets,
				    packet_sizes, param, NULL);
}

static void
sendvirtualnicrecv_premap (SE_HANDLE nic_handle, UINT num_packets,
			   void **packets, UINT *packet_sizes, void *param,
			   long *premap)
{
	struct vpn_msg_virtualnicrecv *arg;
	struct msgbuf *buf;
	UINT i;

	arg = mempool_allocmem (mp, sizeof *arg);
	arg->nic_handle = nic_handle;
	arg->param = param;
	arg->num_packets = num_packets;
	arg->cpu = get_cpu_id ();
	buf = alloc (sizeof *buf * (1 + num_packets));
	setmsgbuf (&buf[0], arg, sizeof *arg, 0);
	if (premap) {
		for (i = 0; i < num_packets; i++)
			setmsgbuf_premap (&buf[1 + i], packets[i],
					  packet_sizes[i], 0, premap[i]);
	} else {
		for (i = 0; i < num_packets; i++)
			setmsgbuf (&buf[1 + i], packets[i], packet_sizes[i],
				   0);
	}
	callsub (VPN_MSG_VIRTUALNICRECV, buf, num_packets + 1);
	free (buf);
	mempool_freemem (mp, arg);
}

static void
sendvirtualnicrecv_wrapper (void *handle, unsigned int num_packets,
			    void **packets, unsigned int *packet_sizes,
			    void *param, long *premap)
{
	struct vpncallback *c = param;

	sendvirtualnicrecv_premap (c->nic_handle, num_packets, packets,
				   packet_sizes, c->param, premap);
}

static void
sendvirtualnicrecv (SE_HANDLE nic_handle, UINT num_packets, void **packets,
		    UINT *packet_sizes, void *param)
{
	sendvirtualnicrecv_premap (nic_handle, num_packets, packets,
				   packet_sizes, param, NULL);
}

static void
vpn_timer_callback (void *handle, void *data)
{
	struct vpn_msg_timer *arg;
	struct msgbuf buf[1];

	arg = mempool_allocmem (mp, sizeof *arg);
	arg->now = vpn_GetTickCount ();
	arg->cpu = get_cpu_id ();
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	callsub (VPN_MSG_TIMER, buf, 1);
	mempool_freemem (mp, arg);
}

static int
vpnkernel_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if (m != MSG_BUF)
		return -1;
	if (c == VPNKERNEL_MSG_IC_RSA_SIGN) {
		struct vpnkernel_msg_ic_rsa_sign *arg;

		if (bufcnt != 4)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		if (buf[2].len > 65536)
			return -1;
		arg = buf[0].base;
		arg->retval = vpn_ic_rsa_sign (buf[1].base, buf[2].base,
					       buf[2].len, buf[3].base,
					       &arg->sign_buf_size);
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_GETPHYSICALNICINFO) {
		struct vpnkernel_msg_getphysicalnicinfo *arg;
		int h;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		h = arg->handle;
		if (h < 0 || h >= NUM_OF_HANDLE)
			return -1;
		vpn_GetPhysicalNicInfo (handle[h], &arg->info);
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_GETVIRTUALNICINFO) {
		struct vpnkernel_msg_getvirtualnicinfo *arg;
		int h;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		h = arg->handle;
		if (h < 0 || h >= NUM_OF_HANDLE)
			return -1;
		vpn_GetVirtualNicInfo (handle[h], &arg->info);
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_SENDPHYSICALNIC) {
		struct vpnkernel_msg_sendphysicalnic *arg;
		int h;
		UINT num_packets;
		void **packets;
		UINT *packet_sizes;
		int i;

		if (bufcnt < 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		h = arg->handle;
		if (h < 0 || h >= NUM_OF_HANDLE)
			return -1;
		num_packets = arg->num_packets;
		if (bufcnt != 1 + num_packets)
			return -1;
		if (num_packets > 255) { /* maximum number of packets */
			printf ("sending too many packets (physical)\n");
			goto skip1;
		}
		if (num_packets == 0)
			goto skip1;
		packets = alloc (sizeof *packets * num_packets);
		packet_sizes = alloc (sizeof *packet_sizes * num_packets);
		for (i = 0; i < num_packets; i++) {
			packets[i] = buf[i + 1].base;
			packet_sizes[i] = buf[i + 1].len;
		}
		vpn_SendPhysicalNic (handle[h], num_packets, packets,
				     packet_sizes);
		free (packets);
		free (packet_sizes);
	skip1:
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_SENDVIRTUALNIC) {
		struct vpnkernel_msg_sendvirtualnic *arg;
		int h;
		UINT num_packets;
		void **packets;
		UINT *packet_sizes;
		int i;

		if (bufcnt < 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		h = arg->handle;
		if (h < 0 || h >= NUM_OF_HANDLE)
			return -1;
		num_packets = arg->num_packets;
		if (bufcnt != 1 + num_packets)
			return -1;
		if (num_packets > 255) { /* maximum number of packets */
			printf ("sending too many packets (virtual)\n");
			goto skip2;
		}
		if (num_packets == 0)
			goto skip2;
		packets = alloc (sizeof *packets * num_packets);
		packet_sizes = alloc (sizeof *packet_sizes * num_packets);
		for (i = 0; i < num_packets; i++) {
			packets[i] = buf[i + 1].base;
			packet_sizes[i] = buf[i + 1].len;
		}
		vpn_SendVirtualNic (handle[h], num_packets, packets,
				     packet_sizes);
		free (packets);
		free (packet_sizes);
	skip2:
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_SETPHYSICALNICRECVCALLBACK) {
		struct vpnkernel_msg_setphysicalnicrecvcallback *arg;
		int h;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		h = arg->handle;
		if (h < 0 || h >= NUM_OF_HANDLE)
			return -1;
		vpn_SetPhysicalNicRecvCallback (handle[h], sendphysicalnicrecv,
						arg->param);
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_SETVIRTUALNICRECVCALLBACK) {
		struct vpnkernel_msg_setvirtualnicrecvcallback *arg;
		int h;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		h = arg->handle;
		if (h < 0 || h >= NUM_OF_HANDLE)
			return -1;
		vpn_SetVirtualNicRecvCallback (handle[h], sendvirtualnicrecv,
					       arg->param);
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_SET_TIMER) {
		struct vpnkernel_msg_set_timer *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		vpn_SetTimer (vpn_timer_handle, arg->interval);
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_GETTICKCOUNT) {
		struct vpnkernel_msg_gettickcount *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		arg->retval = vpn_GetTickCount ();
		arg->cpu = get_cpu_id ();
		return 0;
	} else {
		return -1;
	}
}
#endif /* VPN_PD */

UINT
vpn_GetCurrentCpuId (void)
{
	return get_cpu_id ();
}

UINT
vpn_GetTickCount (void)
{
	u64 d[2], q[2];

	d[0] = get_time ();
	d[1] = 0;
	mpudiv_128_32 (d, 1000, q);
	return (UINT)q[0];
}

SE_HANDLE
vpn_NewTimer (SE_SYS_CALLBACK_TIMER *callback, void *param)
{
	void *p;

	p = timer_new (callback, param);
	if (!p)
		panic ("NewTimer failed");
	return p;
}

void
vpn_SetTimer (SE_HANDLE timer_handle, UINT interval)
{
	u64 usec;

	usec = interval;
	usec *= 1000;
	timer_set (timer_handle, usec);
}

void
vpn_FreeTimer (SE_HANDLE timer_handle)
{
	timer_free (timer_handle);
}

void
vpn_GetPhysicalNicInfo (SE_HANDLE nic_handle, SE_NICINFO *info)
{
	struct vpndata *vpn = nic_handle;
	struct nicinfo nic;
	int i;

	vpn->pf.get_nic_info (vpn->ph, &nic);
	for (i = 0; i < 6; i++)
		info->MacAddress[i] = nic.mac_address[i];
	info->Mtu = nic.mtu;
	info->MediaType = SE_MEDIA_TYPE_ETHERNET;
	info->MediaSpeed = nic.media_speed;
}

void
vpn_SendPhysicalNic (SE_HANDLE nic_handle, UINT num_packets, void **packets,
		     UINT *packet_sizes)
{
	struct vpndata *vpn = nic_handle;

	vpn->pf.send (vpn->ph, num_packets, packets, packet_sizes, true);
}

#if USE_DISASVISOR
uint16_t ntohs(uint16_t x)
{
  return ((x & 0x00ff) << 8) | ((x & 0xff00) >> 8);
}
#endif /* USE_DISASVISOR */

#if USE_DISASVISOR
static int is_delimiter(int c)
{
  return ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'));
}

#define SKIP_DELIMITERS(m)         \
  do {                             \
    while (1) {                    \
      uint8_t c = *(m);            \
      if (c == '\0') return;       \
      if (!is_delimiter(c)) break; \
      (m)++;                       \
    }                              \
  } while (0)
  
#define READ_INTEGER(m, result)             \
  do {                                      \
    (result) = 0;                           \
    while (1) {                             \
      uint8_t c = *(m);                     \
      if (c == '\0') return;                \
      if (is_delimiter(c)) break;           \
      if ((c < '0') || (c > '9')) return;   \
      (result) = (result) * 10 + (c - '0'); \
      (m)++;                                \
    }                                       \
  } while (0)     

/* Sample disasvisor message sent from client:
 *
 * DISASVISOR 1.0
 * 1
 * 80 40
 * Magnitude 5.5
 * Tokyo:   intensity 5 upper
 * Ibaraki: intensity 5 lower
 * Saitama: intensity 4
 *
 * "DISASVISOR 1.0 ": header
 * 1: disaster type (dialog picture number)
 * 80: location of dialog window (x)
 * 40: location of dialog window (y)
 * Byte sequence following them: detailed info shown in the dialog
 */

static void handle_disasvisor_message(uint8_t *udp_payload, size_t udp_payload_len)
{
#define DISASVISOR_KEYWORD "DISASVISOR"
  char *disasvisor_keyword = DISASVISOR_KEYWORD;
  uint8_t c;

#if 0
  printf("UDP payload:\n");
  {
    int i;
    for (i = 0; i < udp_payload_len; i++) {
      c = udp_payload[i];
      if ((32 <= c) && (c <= 126)) {
        printf("%c", c);
      } else {
        printf("(%02x)", c);
      }
    }
    printf("\n");
  }
#endif

  if ((udp_payload_len >= strlen(disasvisor_keyword))
      && (strncmp((char *)udp_payload, disasvisor_keyword, strlen(disasvisor_keyword)) == 0)) {
    uint8_t *m = udp_payload + strlen(disasvisor_keyword);
    uint8_t disaster_type;
    uint32_t dialog_offset_x, dialog_offset_y;
    
    /* skip delimiter(s) */
    if (!is_delimiter(*m))
      return;
    SKIP_DELIMITERS(m);

    /* read the version number (at present, simply skip it) */
    while (1) {
      c = *m;
      if (c == '\0')
        return;
      if (is_delimiter(c))
        break;
      m++;
    }
    
    SKIP_DELIMITERS(m);

    /* read the disaster type */
    READ_INTEGER(m, disaster_type);

    SKIP_DELIMITERS(m);

    /* read the x-offset of the displayed dialog image */
    READ_INTEGER(m, dialog_offset_x);
    
    SKIP_DELIMITERS(m);

    /* read the y-offset of the displayed dialog image */
    READ_INTEGER(m, dialog_offset_y);
    
    SKIP_DELIMITERS(m);

    {
      uint8_t *detailed_info = m;
      size_t detailed_info_len = udp_payload_len - (m - udp_payload);
      vga_display_disaster_warning(disaster_type, dialog_offset_x, dialog_offset_y,
                                   detailed_info, detailed_info_len);
    }
  }
}
#endif /* USE_DISASVISOR */

static void
vpn_callback_wrapper (void *handle, unsigned int num_packets, void **packets,
		      unsigned int *packet_sizes, void *param, long *premap)
{
	struct vpncallback *c = param;

	c->callback (c->nic_handle, num_packets, packets, packet_sizes,
		     c->param);

#if USE_DISASVISOR
  {
    int i;
    //int j, k;
    //uint8_t c;
    for (i = 0; i < num_packets; i++) {
      uint8_t *packet = packets[i];
      uint32_t packet_size = packet_sizes[i];
      uint32_t packet_len = (packet_size - 14 < 0 ? 0 : packet_size - 14);

      uint8_t *ip_packet = packet + 14;
      uint32_t ip_packet_len;

      uint8_t *tcp_or_udp_packet = NULL;
      uint8_t *udp_packet = NULL;
      uint32_t udp_packet_len;
      //uint16_t src_port;
      uint16_t dst_port;

#define DISASVISOR_PORT 26666
      uint16_t disasvisor_port = DISASVISOR_PORT;

#if 0
      printf("packet %d\n", i);
      printf("packet_len = %d\n", packet_len);
#endif
      if (packet_len < 20)
        continue;

      ip_packet_len = ntohs(*((uint16_t *)(ip_packet + 2)));

#if 0
      printf("IP version: %d\n", (ip_packet[0] & 0xf0) >> 4);
      printf("IP header length: %d octets\n", ip_packet[0] & 0x0f);
      printf("IP packet length: %d bytes\n", ip_packet_len);
      printf("IP packet TTL: %d\n", ip_packet[8]);
      printf("IP packet protocol: %d", ip_packet[9]);
      if (ip_packet[9] == 0x6) printf(" (TCP) ");
      if (ip_packet[9] == 0x11) printf(" (UDP) ");
      if (ip_packet[9] == 0x29) printf(" (IPv6) ");
      printf("\n");
      printf("src ip -> dst ip: %d.%d.%d.%d -> %d.%d.%d.%d\n",
             ip_packet[12], ip_packet[13], ip_packet[14], ip_packet[15],
             ip_packet[16], ip_packet[17], ip_packet[18], ip_packet[19]);
#endif

      tcp_or_udp_packet = ip_packet + 20;

      //src_port = ntohs(*((uint16_t *)(tcp_or_udp_packet + 0)));
      dst_port = ntohs(*((uint16_t *)(tcp_or_udp_packet + 2)));

      //printf("src port -> dst port: %d -> %d\n", src_port, dst_port);

      if (ip_packet[9] == 0x11) {
        // it is a UDP packet
        udp_packet = tcp_or_udp_packet;
      } else {
        // it is not a UDP packet
        continue;
      }

      if (ip_packet_len < 28)
        continue;
      
      udp_packet_len = ntohs(*((uint16_t *)(udp_packet + 4)));

      //printf("UDP packet length: %d\n", udp_packet_len);
      //printf("UDP checksum: %d\n", ntohs(*((UINT16 *)(udp_packet + 6))));

      if (dst_port == disasvisor_port) {
        //printf("Received packets to the DisasVisor port!\n");
        uint8_t *udp_payload = udp_packet + 8;
        uint16_t udp_payload_len = udp_packet_len - 8;
        if (udp_payload_len <= 0) {
          return;
        }
        handle_disasvisor_message(udp_payload, udp_payload_len);
      }
    }
  }
#endif /* USE_DISASVISOR */
}

void
vpn_SetPhysicalNicRecvCallback (SE_HANDLE nic_handle,
				SE_SYS_CALLBACK_RECV_NIC *callback,
				void *param)
{
	struct vpndata *vpn = nic_handle;

	vpn->pc.nic_handle = nic_handle;
	vpn->pc.callback = callback;
	vpn->pc.param = param;
#ifdef VPN_PD
	if (callback == sendphysicalnicrecv) {
		vpn->pf.set_recv_callback (vpn->ph,
					   sendphysicalnicrecv_wrapper,
					   &vpn->pc);
		return;
	}
#endif /* VPN_PD */
	vpn->pf.set_recv_callback (vpn->ph, vpn_callback_wrapper, &vpn->pc);
}

void
vpn_GetVirtualNicInfo (SE_HANDLE nic_handle, SE_NICINFO *info)
{
	struct vpndata *vpn = nic_handle;
	struct nicinfo nic;
	int i;

	vpn->vf.get_nic_info (vpn->vh, &nic);
	for (i = 0; i < 6; i++)
		info->MacAddress[i] = nic.mac_address[i];
	info->Mtu = nic.mtu;
	info->MediaType = SE_MEDIA_TYPE_ETHERNET;
	info->MediaSpeed = nic.media_speed;
}

void
vpn_SendVirtualNic (SE_HANDLE nic_handle, UINT num_packets, void **packets,
		    UINT *packet_sizes)
{
	struct vpndata *vpn = nic_handle;

	vpn->vf.send (vpn->vh, num_packets, packets, packet_sizes, true);
}

void
vpn_SetVirtualNicRecvCallback (SE_HANDLE nic_handle,
			       SE_SYS_CALLBACK_RECV_NIC *callback, void *param)
{
	struct vpndata *vpn = nic_handle;

	vpn->vc.nic_handle = nic_handle;
	vpn->vc.callback = callback;
	vpn->vc.param = param;
#ifdef VPN_PD
	if (callback == sendvirtualnicrecv) {
		vpn->vf.set_recv_callback (vpn->vh,
					   sendvirtualnicrecv_wrapper,
					   &vpn->vc);
		return;
	}
#endif /* VPN_PD */
	vpn->vf.set_recv_callback (vpn->vh, vpn_callback_wrapper, &vpn->vc);
}

static long
vpn_premap_recvbuf (void *handle, void *buf, unsigned int len)
{
#ifdef VPN_PD
	struct msgbuf mbuf;

	setmsgbuf (&mbuf, buf, len, 0);
	return msgpremapbuf (desc, &mbuf);
#endif /* VPN_PD */
	return 0;
}

bool
vpn_ic_rsa_sign (char *key_name, void *data, UINT data_size, void *sign,
		 UINT *sign_buf_size)
{
	unsigned long idman_session;
	unsigned short int signlen;
	int idman_ret;

#ifdef IDMAN
	if (!get_idman_session (&idman_session)) {
		printf ("RsaSign: card reader not ready\n");
		return false;
	}
	signlen = *sign_buf_size;
	idman_ret = IDMan_EncryptByIndex (idman_session, 2, data, data_size,
					  sign, &signlen, 0x220);
	if (!idman_ret) {
		*sign_buf_size = signlen;
		return true;
	}
#endif /* IDMAN */
	printf ("RsaSign: IDMan_EncryptByIndex()"
		" ret=%d data_size=%d\n", idman_ret, data_size);
	return false;
}

static void *
vpn_new_nic (char *arg, void *param)
{
	struct vpndata *p;

	p = alloc (sizeof *p);
	return p;
}

static bool
vpn_init (void *handle, void *phys_handle, struct nicfunc *phys_func,
	  void *virt_handle, struct nicfunc *virt_func)
{
	struct vpndata *p = handle;

	if (!virt_func)
		return false;
	p->ph = phys_handle;
	p->pf = *phys_func;
	p->vh = virt_handle;
	p->vf = *virt_func;
	return true;
}

static void
vpn_net_start (void *handle)
{
	struct vpndata *p = handle;

	p->vpn_handle = vpn_start (p);
}

static struct netfunc vpn_func = {
	.new_nic = vpn_new_nic,
	.init = vpn_init,
	.start = vpn_net_start,
	.premap_recvbuf = vpn_premap_recvbuf,
};

static void
vpn_kernel_init (void)
{
	void vpn_user_init (struct config_data_vpn *vpn, char *seed, int len);

#ifdef VPN_PD
	int i;

	spinlock_init (&handle_lock);
	for (i = 0; i < NUM_OF_HANDLE; i++)
		handle[i] = NULL;
	vpn_timer_handle = vpn_NewTimer (vpn_timer_callback, NULL);
	vpnkernel_desc = msgregister ("vpnkernel", vpnkernel_msghandler);
	if (vpnkernel_desc < 0)
		panic ("register vpnkernel");
#endif /* VPN_PD */
	vpn_user_init (&config.vpn, config.vmm.randomSeed,
		       sizeof config.vmm.randomSeed);
#ifdef VPN_PD
	desc = msgopen ("vpn");
	if (desc < 0)
		panic ("open vpn");
#endif
	net_register ("vpn", &vpn_func, NULL);
}

INITFUNC ("driver1", vpn_kernel_init);
