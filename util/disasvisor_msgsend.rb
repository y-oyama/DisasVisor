#!/usr/bin/ruby

require "socket"

if ARGV.length == 0
  STDERR.print "Usage: ", $0, " disasvisor-client-ipaddr [message]\n"
  abort
else
  ip = ARGV[0]
  if ARGV.length == 1
    msg = STDIN.read
  else
    msg = ARGV[1]
  end
end

udp = UDPSocket.open()
port = 26666
sockaddr = Socket.pack_sockaddr_in(port, ip)
udp.send(msg, 0, sockaddr)
udp.close

