#!/usr/bin/ruby

STDIN.gets # skip the PPM header
STDIN.gets # skip the picture size info
STDIN.gets # skip the color depth info

while (line = STDIN.gets)
  while (line =~ /^\s*(\d+)(.*)$/)
    num = $1
    print num, ", "
    line = $2
  end
  print "\n"
end

