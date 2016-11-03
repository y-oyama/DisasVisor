#!/usr/bin/python

import sys
import re
import urllib2
import time
import socket

from HTMLParser import HTMLParser, HTMLParseError

class MyHTMLParser(HTMLParser):
    def __init__(self):
        HTMLParser.__init__(self)
        self.scan_mode = False
        self.result = []
    
    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'table' == tag and 'class' in attrs and attrs['class'] == 'top_rtmap':
            self.scan_mode = True

    def handle_endtag(self, tag):
        if tag == 'table':
            self.scan_mode = False

    def handle_data(self, data):
        if (self.scan_mode):
            m = re.match('\s+$', data)
            if (m == None):
                self.result.append(data)

argv = sys.argv
argc = len(argv)
if (argc != 3):
    sys.stderr.write('Usage: %s disasvisor-client-ipaddr server-check-interval-in-second\n' % argv[0])
    quit()
client_ipaddr = argv[1]
period = int(argv[2])
client_port = 26666 # DisasVisor receives messages from UDP/26666
URL = 'http://www.hinet.bosai.go.jp/?LANG=en'

parser = MyHTMLParser()

prev_earthquake_info = ''

while True:

    response = urllib2.urlopen(URL)
    htmlbody = response.read()

    parser.feed(htmlbody)
    parser.close()
    result = parser.result
    result.pop(0)
    result.pop(0)

    curr_earthquake_info = ''
    toggle = 0
    for i in range(len(result)):
        x = result.pop(0)
        if (toggle == 0):
            # sys.stdout.write(x)
            # sys.stdout.write(": ")
            curr_earthquake_info = curr_earthquake_info + x
            curr_earthquake_info = curr_earthquake_info + ': '
        else:
            # sys.stdout.write(x)
            # sys.stdout.write("\n")
            m = re.match('(\D{3} \d{2}, \d{4} \d{2}:\d{2}:\d{2})\.\d{2}$', x)
            if m:
                x = m.group(1)
            curr_earthquake_info = curr_earthquake_info + x
            curr_earthquake_info = curr_earthquake_info + '\n'
        toggle = 1 - toggle

    if (prev_earthquake_info != curr_earthquake_info):
        print "==== Latest Earthquake ===="
        sys.stdout.write(curr_earthquake_info)
        print "========"
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        message = 'DISASVISOR 1.0 1 0080 0040     ' + curr_earthquake_info
        print "message is ", message
        print "client_ipaddr is ", client_ipaddr
        print "client_port is ", client_port
        sock.sendto(message, (client_ipaddr, client_port))
        prev_earthquake_info = curr_earthquake_info
    else:
        sys.stdout.write('No new earthquake.\n')

    time.sleep(period)

