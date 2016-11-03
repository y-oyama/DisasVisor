/*
 * Copyright (c) 2012 Igel Co., Ltd
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

#include "initfunc.h"
#include "list.h"
#include "mm.h"
#include "spinlock.h"
#include "vga.h"

#if USE_ADVISOR
#include "printf.h"
#endif /* USE_ADVISOR */

struct vga_data {
	LIST1_DEFINE (struct vga_data);
	struct vga_func *func;
	struct vga_func_data *data;
};

static LIST1_DEFINE_HEAD (struct vga_data, vga_list);
static rw_spinlock_t vga_lock;
static struct vga_data *vga_active;

void
vga_register (struct vga_func *func, struct vga_func_data *data)
{
	struct vga_data *p;

	p = alloc (sizeof *p);
	p->func = func;
	p->data = data;
	rw_spinlock_lock_ex (&vga_lock);
	LIST1_ADD (vga_list, p);
	rw_spinlock_unlock_ex (&vga_lock);
}

int
vga_is_ready (void)
{
	struct vga_data *p;

	rw_spinlock_lock_sh (&vga_lock);
	LIST1_FOREACH (vga_list, p)
		if (p->func->is_ready (p->data))
			break;
	rw_spinlock_unlock_sh (&vga_lock);
	vga_active = p;
	return p ? 1 : 0;
}

int
vga_transfer_image (enum vga_func_transfer_dir dir, void *image,
		    enum vga_func_image_type type, int stride,
		    unsigned int width, unsigned int lines,
		    unsigned int x, unsigned int y)
{
	struct vga_data *p;

	p = vga_active;
	if (!p)
		return 0;
	return p->func->transfer_image (p->data, dir, image, type, stride,
					width, lines, x, y);
}

int
vga_fill_rect (void *image, enum vga_func_image_type type, unsigned int x,
	       unsigned int y, unsigned int width, unsigned int height)
{
	struct vga_data *p;

	p = vga_active;
	if (!p)
		return 0;
	return p->func->fill_rect (p->data, image, type, x, y, width, height);
}

int
vga_get_screen_size (unsigned int *width, unsigned int *height)
{
	struct vga_data *p;

	p = vga_active;
	if (!p)
		return 0;
	return p->func->get_screen_size (p->data, width, height);
}


#if USE_ADVISOR
int vga_display_picture(u8 *image, unsigned int x, unsigned int y,
                        unsigned int width, unsigned int height)
{
#define NBYTES_IN_IMGDATA_PIXEL 3

	struct vga_data *p;
	unsigned int xx, yy;

	p = vga_active;
	if (!p) {
		return 0;
	}

	for (yy = 0; yy < height; yy++) {
		for (xx = 0; xx < width; xx++) {
			u8 pixel[4];
			pixel[0] = image[yy * width * NBYTES_IN_IMGDATA_PIXEL + xx * NBYTES_IN_IMGDATA_PIXEL + 2];
			pixel[1] = image[yy * width * NBYTES_IN_IMGDATA_PIXEL + xx * NBYTES_IN_IMGDATA_PIXEL + 1];
			pixel[2] = image[yy * width * NBYTES_IN_IMGDATA_PIXEL + xx * NBYTES_IN_IMGDATA_PIXEL + 0];
			pixel[3] = 0;
			p->func->fill_rect (p->data, pixel, VGA_FUNC_IMAGE_TYPE_BGRX_8888, x + xx, y + yy, /*width=*/1, /*height=*/1);
		}
	}
	return 1;
}

static u8 advisor_picture0_rgb[] = {
#include "pictures/tsukuba_logo_black.dat"
};
static unsigned int advisor_picture0_widthheight[] = { 200, 200 };

static u8 advisor_picture1_rgb[] = {
#include "pictures/dialog_plain1.dat"
};
static unsigned int advisor_picture1_widthheight[] = { 800, 600 };

static u8 advisor_picture2_rgb[] = {
#include "pictures/dialog_plain2.dat"
};
static unsigned int advisor_picture2_widthheight[] = { 800, 600 };

int vga_display_stored_picture(unsigned int picture_number, unsigned int x, unsigned int y)
{
  unsigned int width = 0;
  unsigned int height = 0;
  u8 *rgb_data = NULL;

  switch (picture_number) {
  case 0:
    width  = advisor_picture0_widthheight[0];
    height = advisor_picture0_widthheight[1];
    rgb_data = advisor_picture0_rgb;
    break;
  case 1:
    width  = advisor_picture1_widthheight[0];
    height = advisor_picture1_widthheight[1];
    rgb_data = advisor_picture1_rgb;
    break;
  case 2:
    width  = advisor_picture2_widthheight[0];
    height = advisor_picture2_widthheight[1];
    rgb_data = advisor_picture2_rgb;
    break;
  default:
    printf("Invalid picture number.\n");
    return -1;
  }

  if (!vga_is_ready()) {
    return -1;
  }

  return vga_display_picture(rgb_data, x, y, width, height);
}

#include "dialog_fonts/letter_size.h"

static u8 advisor_letter_rgb[95][LETTER_WIDTH*LETTER_HEIGHT*3] = {
  {
#include "dialog_fonts/letter_20.dat"
  },
  {
#include "dialog_fonts/letter_21.dat"
  },
  {
#include "dialog_fonts/letter_22.dat"
  },
  {
#include "dialog_fonts/letter_23.dat"
  },
  {
#include "dialog_fonts/letter_24.dat"
  },
  {
#include "dialog_fonts/letter_25.dat"
  },
  {
#include "dialog_fonts/letter_26.dat"
  },
  {
#include "dialog_fonts/letter_27.dat"
  },
  {
#include "dialog_fonts/letter_28.dat"
  },
  {
#include "dialog_fonts/letter_29.dat"
  },
  {
#include "dialog_fonts/letter_2a.dat"
  },
  {
#include "dialog_fonts/letter_2b.dat"
  },
  {
#include "dialog_fonts/letter_2c.dat"
  },
  {
#include "dialog_fonts/letter_2d.dat"
  },
  {
#include "dialog_fonts/letter_2e.dat"
  },
  {
#include "dialog_fonts/letter_2f.dat"
  },
  {
#include "dialog_fonts/letter_30.dat"
  },
  {
#include "dialog_fonts/letter_31.dat"
  },
  {
#include "dialog_fonts/letter_32.dat"
  },
  {
#include "dialog_fonts/letter_33.dat"
  },
  {
#include "dialog_fonts/letter_34.dat"
  },
  {
#include "dialog_fonts/letter_35.dat"
  },
  {
#include "dialog_fonts/letter_36.dat"
  },
  {
#include "dialog_fonts/letter_37.dat"
  },
  {
#include "dialog_fonts/letter_38.dat"
  },
  {
#include "dialog_fonts/letter_39.dat"
  },
  {
#include "dialog_fonts/letter_3a.dat"
  },
  {
#include "dialog_fonts/letter_3b.dat"
  },
  {
#include "dialog_fonts/letter_3c.dat"
  },
  {
#include "dialog_fonts/letter_3d.dat"
  },
  {
#include "dialog_fonts/letter_3e.dat"
  },
  {
#include "dialog_fonts/letter_3f.dat"
  },
  {
#include "dialog_fonts/letter_40.dat"
  },
  {
#include "dialog_fonts/letter_41.dat"
  },
  {
#include "dialog_fonts/letter_42.dat"
  },
  {
#include "dialog_fonts/letter_43.dat"
  },
  {
#include "dialog_fonts/letter_44.dat"
  },
  {
#include "dialog_fonts/letter_45.dat"
  },
  {
#include "dialog_fonts/letter_46.dat"
  },
  {
#include "dialog_fonts/letter_47.dat"
  },
  {
#include "dialog_fonts/letter_48.dat"
  },
  {
#include "dialog_fonts/letter_49.dat"
  },
  {
#include "dialog_fonts/letter_4a.dat"
  },
  {
#include "dialog_fonts/letter_4b.dat"
  },
  {
#include "dialog_fonts/letter_4c.dat"
  },
  {
#include "dialog_fonts/letter_4d.dat"
  },
  {
#include "dialog_fonts/letter_4e.dat"
  },
  {
#include "dialog_fonts/letter_4f.dat"
  },
  {
#include "dialog_fonts/letter_50.dat"
  },
  {
#include "dialog_fonts/letter_51.dat"
  },
  {
#include "dialog_fonts/letter_52.dat"
  },
  {
#include "dialog_fonts/letter_53.dat"
  },
  {
#include "dialog_fonts/letter_54.dat"
  },
  {
#include "dialog_fonts/letter_55.dat"
  },
  {
#include "dialog_fonts/letter_56.dat"
  },
  {
#include "dialog_fonts/letter_57.dat"
  },
  {
#include "dialog_fonts/letter_58.dat"
  },
  {
#include "dialog_fonts/letter_59.dat"
  },
  {
#include "dialog_fonts/letter_5a.dat"
  },
  {
#include "dialog_fonts/letter_5b.dat"
  },
  {
#include "dialog_fonts/letter_5c.dat"
  },
  {
#include "dialog_fonts/letter_5d.dat"
  },
  {
#include "dialog_fonts/letter_5e.dat"
  },
  {
#include "dialog_fonts/letter_5f.dat"
  },
  {
#include "dialog_fonts/letter_60.dat"
  },
  {
#include "dialog_fonts/letter_61.dat"
  },
  {
#include "dialog_fonts/letter_62.dat"
  },
  {
#include "dialog_fonts/letter_63.dat"
  },
  {
#include "dialog_fonts/letter_64.dat"
  },
  {
#include "dialog_fonts/letter_65.dat"
  },
  {
#include "dialog_fonts/letter_66.dat"
  },
  {
#include "dialog_fonts/letter_67.dat"
  },
  {
#include "dialog_fonts/letter_68.dat"
  },
  {
#include "dialog_fonts/letter_69.dat"
  },
  {
#include "dialog_fonts/letter_6a.dat"
  },
  {
#include "dialog_fonts/letter_6b.dat"
  },
  {
#include "dialog_fonts/letter_6c.dat"
  },
  {
#include "dialog_fonts/letter_6d.dat"
  },
  {
#include "dialog_fonts/letter_6e.dat"
  },
  {
#include "dialog_fonts/letter_6f.dat"
  },
  {
#include "dialog_fonts/letter_70.dat"
  },
  {
#include "dialog_fonts/letter_71.dat"
  },
  {
#include "dialog_fonts/letter_72.dat"
  },
  {
#include "dialog_fonts/letter_73.dat"
  },
  {
#include "dialog_fonts/letter_74.dat"
  },
  {
#include "dialog_fonts/letter_75.dat"
  },
  {
#include "dialog_fonts/letter_76.dat"
  },
  {
#include "dialog_fonts/letter_77.dat"
  },
  {
#include "dialog_fonts/letter_78.dat"
  },
  {
#include "dialog_fonts/letter_79.dat"
  },
  {
#include "dialog_fonts/letter_7a.dat"
  },
  {
#include "dialog_fonts/letter_7b.dat"
  },
  {
#include "dialog_fonts/letter_7c.dat"
  },
  {
#include "dialog_fonts/letter_7d.dat"
  },
  {
#include "dialog_fonts/letter_7e.dat"
  },
};

int vga_display_stored_letter(u8 ascii_code, unsigned int x, unsigned y)
{
  u8 *rgb_data = NULL;
  if ((ascii_code < ' ') || (ascii_code > '~')) {
    printf("WARNING: vga_display_stored_letter: invalid ASCII code: 0x%02x\n", ascii_code);
    return -1;
  }
  rgb_data = advisor_letter_rgb[ascii_code - ' '];
  return vga_display_picture(rgb_data, x, y, LETTER_WIDTH, LETTER_HEIGHT);
}

#if USE_DISASVISOR
enum disaster_type { disaster_type_earthquake = 1, disaster_type_flood = 2 };

int vga_display_disaster_warning(unsigned int dt, unsigned int x, unsigned int y,
                                 unsigned char *msg, unsigned int msg_len)
{
  int offs_x_inside_dialog = 26, offs_y_inside_dialog = 154;
  int idx_x = 0, idx_y = 0;
  int nth;

#define MAX_NLETTERS_IN_ROW 41
#define MAX_NROWS_IN_DIALOG 12

  if (dt == disaster_type_earthquake) {
    vga_display_stored_picture(1, x, y);
  } else if (dt == disaster_type_flood) {
    vga_display_stored_picture(2, x, y);
  } else {
    printf("Error: unknown disaster type: %d\n", dt);
    return -1;
  }

  for (nth = 0; nth < msg_len; nth++) {
    unsigned char c = msg[nth];
    if ((' ' <= c) && (c <= '~')) {
      int loc_x = x + offs_x_inside_dialog + LETTER_WIDTH * idx_x;
      int loc_y = y + offs_y_inside_dialog + LETTER_HEIGHT * idx_y;
      vga_display_stored_letter(/*ascii_code=*/c, /*x=*/loc_x, /*y=*/loc_y);
      idx_x++;
      if (idx_x >= MAX_NLETTERS_IN_ROW) {
        idx_x = 0;
        idx_y++;
      }
    } else if (c == '\n') {
      idx_x = 0;
      idx_y++;
    }
    if (idx_y >= MAX_NROWS_IN_DIALOG) {
      printf("WARNING: Too many lines are provided in the disasvisor message.\n");
      break;
    }
  }
  return 0;
}
#endif /* USE_DISASVISOR */

int
vga_blackout_screen(int use_semitrans_blackout)
{
  unsigned int width, height;
  unsigned int x, y;
  unsigned int const screen_edge_pixels_left = 20;
  struct vga_data *p = vga_active;
  u8 black_pixel[4] = { 0, 0, 0, 0 };
  u32 pixel[1];
  u8 *pixelb = (u8*)pixel;
  if (!p) {
    return 0;
  }
  if (!vga_get_screen_size(&width, &height)) {
    return 0;
  }
  for (y = screen_edge_pixels_left; y < height - screen_edge_pixels_left; y++) {
    for (x = screen_edge_pixels_left; x < width - screen_edge_pixels_left; x++) {
      if (use_semitrans_blackout) {
	if (vga_transfer_image(VGA_FUNC_TRANSFER_DIR_GET, pixel,
			       VGA_FUNC_IMAGE_TYPE_BGRX_8888,
			       sizeof pixel[0], 1, 1, x, y) < 0) {
	  return 0;
	}
	pixelb[0] = pixelb[0] / 8;
	pixelb[1] = pixelb[1] / 8;
	pixelb[2] = pixelb[2] / 8;
	pixelb[3] = pixelb[3] / 8;
	if (vga_transfer_image(VGA_FUNC_TRANSFER_DIR_PUT, pixel,
			       VGA_FUNC_IMAGE_TYPE_BGRX_8888,
			       sizeof pixel[0], 1, 1, x, y) < 0) {
	  return 0;
	}
      } else {
        p->func->fill_rect (p->data, black_pixel, VGA_FUNC_IMAGE_TYPE_BGRX_8888, x, y, /*width=*/1, /*height=*/1);
      }
    }
  }
  return 1;
}

int vga_tessellate_screen(unsigned int tile_size)
{
  unsigned int width, height;
  unsigned int x, y;
  u32 *tile;
  if (!vga_get_screen_size(&width, &height)) {
    return 0;
  }

  tile = alloc(tile_size * tile_size * sizeof(u32));
  for (y = 0; y < height; y += tile_size) {
    for (x = 0; x < width; x += tile_size) {
      int j;
      if ((x + tile_size > width) || (y + tile_size > height)) {
        continue;
      }
      if (vga_transfer_image(VGA_FUNC_TRANSFER_DIR_GET, tile,
                             VGA_FUNC_IMAGE_TYPE_BGRX_8888,
                             sizeof tile[0] * tile_size,
                             tile_size, tile_size,
                             x, y) < 0) {
        goto out;
      }
      for (j = 1; j < tile_size * tile_size; j++) {
        tile[j] = tile[0];
      }
      if (vga_transfer_image(VGA_FUNC_TRANSFER_DIR_PUT, tile,
                             VGA_FUNC_IMAGE_TYPE_BGRX_8888,
                             sizeof tile[0] * tile_size,
                             tile_size, tile_size,
                             x, y) < 0) {
        goto out;
      }
    }
  }

 out:
  free(tile);
  return 1;
}
#endif /* USE_ADVISOR */


static void
vga_init (void)
{
	rw_spinlock_init (&vga_lock);
	LIST1_HEAD_INIT (vga_list);
	vga_active = NULL;
}

INITFUNC ("driver0", vga_init);
