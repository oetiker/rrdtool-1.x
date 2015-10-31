#! /usr/bin/perl 
use Data::Dumper;

use FindBin;

BEGIN { $| = 1; print "1..2\n"; }
END {
  print "not ok 1\n" unless $loaded;
  unlink "demo.rrd";
}

sub ok
{
    my($what, $result) = @_ ;
    $ok_count++;
    if (not $result){
      warn "failed $what\n";
      print "not ";
    }
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
      step=>100,
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
  "--start", "1424540800",
  "--end", "start+24h",
  "--lower-limit=0",
  "--interlaced",
  "--imgformat","PNG",
  "--width=450",
  "DEF:a=cb//extrainfo:a:AVERAGE",
  "DEF:b=cb//:b:AVERAGE",
  "DEF:c=cb//:c:AVERAGE",
  "LINE:a#00b6e4:a",
  "LINE:b#10b634:b",
  "LINE:c#503d14:c",
  "VDEF:av=a,AVERAGE",
  "PRINT:av:%8.6lf";
  
if (my $ERROR = RRDs::error) {
   die "RRD ERROR: $ERROR\n";
}

my $a = $result->{'print[0]'};
my $b = '0.722767';
ok("$a eq $b",$a eq $b);
