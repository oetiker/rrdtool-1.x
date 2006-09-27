#!/usr/bin/env ruby
# $Id: test.rb,v 1.2 2002/10/22 17:34:00 miles Exp $
# Driver does not carry cash.

require "RRD"

name = "test"
rrd = "#{name}.rrd"
start = Time.now.to_i

puts "creating #{rrd}"
RRD.create(
    rrd,
    "--start", "#{start - 1}",
    "--step", "300",
	"DS:a:GAUGE:600:U:U",
    "DS:b:GAUGE:600:U:U",
    "RRA:AVERAGE:0.5:1:300")
puts

puts "updating #{rrd}"
start.to_i.step(start.to_i + 300 * 300, 300) { |i|
    RRD.update(rrd, "#{i}:#{rand(100)}:#{Math.sin(i / 800) * 50 + 50}")
}
puts

puts "fetching data from #{rrd}"
(fstart, fend, data) = RRD.fetch(rrd, "--start", start.to_s, "--end", (start + 300 * 300).to_s, "AVERAGE")
puts "got #{data.length} data points from #{fstart} to #{fend}"
puts

puts "generating graph #{name}.png"
RRD.graph(
   "#{name}.png",
    "--title", " RubyRRD Demo", 
    "--start", "#{start} + 1 h",
    "--end", "#{start} + 1000 min",
    "--interlace", 
    "--imgformat", "PNG",
    "--width=450",
    "DEF:a=#{rrd}:a:AVERAGE",
    "DEF:b=#{rrd}:b:AVERAGE",
    "CDEF:line=TIME,2400,%,300,LT,a,UNKN,IF",
    "AREA:b#00b6e4:beta",
    "AREA:line#0022e9:alpha",
    "LINE3:line#ff0000")
puts

print "This script has created #{name}.png in the current directory\n";
print "This demonstrates the use of the TIME and % RPN operators\n";
