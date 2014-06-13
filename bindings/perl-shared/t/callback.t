#! /usr/bin/perl 
use Data::Dumper;

use FindBin;

BEGIN { $| = 1; print "1..1\n"; }
END {
  print "not ok 1\n" unless $loaded;
  unlink "demo.rrd";
}

sub ok
{
    my($what, $result) = @_ ;
    $ok_count++;
    print "not " unless $result;
    print "ok $ok_count $what\n";
}

use strict;
use vars qw(@ISA $loaded);

use RRDs;
$loaded = 1;
my $ok_count = 1;

ok("loading",1);


RRDs::fetch_cb_register(sub{
    my $request = shift;
    my $items = ($request->{end}-$request->{start})/$request->{step};
    return {
      step=>200,
      start=>$request->{start},
      data => {
        a=>[ map{ sin($_/200) } (0..$items) ],
        b=>[ map{ cos($_/200) } (10..$items) ],
        c=>[ map{ sin($_/100) } (100..$items) ],
      }
    };
});

my $result = RRDs::graphv "callback.png",
  "--title", "Callback Demo", 
  "--start", "now",
  "--end", "start+1d",
  "--lower-limit=0",
  "--interlace", 
  "--imgformat","PNG",
  "--width=450",
  "DEF:a=cb//extrainfo:a:AVERAGE",
  "DEF:b=cb//:b:AVERAGE",
  "DEF:c=cb//:c:AVERAGE",
  "LINE:a#00b6e4:a",
  "LINE:b#10b634:b",
  "LINE:c#503d14:c";

if (my $ERROR = RRDs::error) {
   die "RRD ERROR: $ERROR\n";
}

