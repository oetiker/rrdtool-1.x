#! /bin/perl

#makes things work when run without install
use lib qw( ../bindings/perl-shared/blib/lib ../bindings/perl-shared/blib/arch );

#makes programm work AFTER install
use lib qw( /usr/local/rrdtool-1.1.0/lib/perl ../lib/perl );

use strict;
use vars qw(@ISA $loaded);

use RRDs;
my $start=time;
my $rrd="randome.rrd";
RRDs::create ($rrd, "--start",$start-1, "--step",300,
	      "DS:a:GAUGE:600:U:U",
	      "DS:b:GAUGE:600:U:U",
	      "RRA:AVERAGE:0.5:1:200");
my $ERROR = RRDs::error;
die "$0: unable to create `$rrd': $ERROR\n" if $ERROR;
my $t;
for ($t=$start; $t<$start+200*300; $t+=300){
  RRDs::update $rrd, "$t:".rand(100).":".(sin($t/800)*50+50);
  if ($ERROR = RRDs::error) {
    die "$0: unable to update `$rrd': $ERROR\n";
  }
}
RRDs::graph "stripes.png",
  "--title", 'Stripes Demo', 
  "--start", $start,
  "--end", "start + 400 min",
  "--interlace", 
  "--imgformat","PNG",
  "--width=450",
  "DEF:a=$rrd:a:AVERAGE",
  "DEF:b=$rrd:b:AVERAGE",
  "CDEF:alpha=TIME,1200,%,600,LT,a,UNKN,IF",
  "CDEF:beta=TIME,1200,%,600,GE,b,UNKN,IF",
  "AREA:alpha#0022e9:alpha",
  "AREA:beta#00b674:beta",
  "LINE1:b#ff4400:beta envelope\\c",
  "COMMENT:\\s",
  "COMMENT:CDEF\:alpha=TIME,1200,%,600,LT,a,UNKN,IF",
  "COMMENT:CDEF\:beta=TIME,1200,%,600,GE,b,UNKN,IF\\j";
if ($ERROR = RRDs::error) {
  print "ERROR: $ERROR\n";
};


print "This script has created stripes.png in the current directory\n";
print "This demonstrates the use of the TIME and % RPN operators\n";
