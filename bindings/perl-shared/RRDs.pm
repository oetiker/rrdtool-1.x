package RRDs;

use strict;
use vars qw(@ISA $VERSION);

@ISA = qw(DynaLoader);

require DynaLoader;

$VERSION=1.2012;

bootstrap RRDs $VERSION;

1;
__END__

=head1 NAME

RRDs - Access RRDtool as a shared module

=head1 SYNOPSIS

  use RRDs;
  RRDs::error
  RRDs::last ...
  RRDs::info ...
  RRDs::create ...
  RRDs::update ...
  RRDs::updatev ...
  RRDs::graph ...
  RRDs::fetch ...
  RRDs::tune ...
  RRDs::times(start, end)

=head1 DESCRIPTION

=head2 Calling Sequence

This module accesses RRDtool functionality directly from within perl. The
arguments to the functions listed in the SYNOPSIS are explained in the regular
RRDtool documentation. The commandline call

 rrdtool update mydemo.rrd --template in:out N:12:13

gets turned into

 RRDs::update ("mydemo.rrd", "--template", "in:out", "N:12:13");

Note that

 --template=in:out

is also valid.

The RRDs::times function takes two parameters:  a "start" and "end" time.
These should be specified in the B<AT-STYLE TIME SPECIFICATION> format
used by RRDtool.  See the B<rrdfetch> documentation for a detailed
explanation on how to specify time.

=head2 Error Handling

The RRD functions will not abort your program even when they can not make
sense out of the arguments you fed them.

The function RRDs::error should be called to get the error status
after each function call. If RRDs::error does not return anything
then the previous function has completed its task successfully.

 use RRDs;
 RRDs::update ("mydemo.rrd","N:12:13");
 my $ERR=RRDs::error;
 die "ERROR while updating mydemo.rrd: $ERR\n" if $ERR;

=head2 Return Values

The functions RRDs::last, RRDs::graph, RRDs::info, RRDs::fetch and RRDs::times
return their findings.

B<RRDs::last> returns a single INTEGER representing the last update time.

 $lastupdate = RRDs::last ...

B<RRDs::graph> returns an pointer to an ARRAY containing the x-size and y-size of the
created image and results of the PRINT arguments.

 ($averages,$xsize,$ysize) = RRDs::graph ...
 print "Imagesize: ${xsize}x${ysize}\n";
 print "Averages: ", (join ", ", @$averages);

B<RRDs::info> returns a pointer to a hash. The keys of the hash
represent the property names of the RRD and the values of the hash are
the values of the properties.  

 $hash = RRDs::info "example.rrd";
 foreach my $key (keys %$hash){
   print "$key = $$hash{$key}\n";
 }

B<RRDs::updatev> also returns a pointer to hash. The keys of the hash
are concatenated strings of a timestamp, RRA index, and data source name for
each consolidated data point (CDP) written to disk as a result of the
current update call. The hash values are CDP values.

B<RRDs::fetch> is the most complex of
the pack regarding return values. There are 4 values. Two normal
integers, a pointer to an array and a pointer to a array of pointers.

  my ($start,$step,$names,$data) = RRDs::fetch ... 
  print "Start:       ", scalar localtime($start), " ($start)\n";
  print "Step size:   $step seconds\n";
  print "DS names:    ", join (", ", @$names)."\n";
  print "Data points: ", $#$data + 1, "\n";
  print "Data:\n";
  foreach my $line (@$data) {
    print "  ", scalar localtime($start), " ($start) ";
    $start += $step;
    foreach my $val (@$line) {
      printf "%12.1f ", $val;
    }
    print "\n";
  }

B<RRDs::times> returns two integers which are the number of seconds since
epoch (1970-01-01) for the supplied "start" and "end" arguments, respectively.

See the examples directory for more ways to use this extension.

=head1 NOTE

If you are manipulating the TZ variable you should also call the posixs
function tzset to initialize all internal state of the library for properly
operating in the timezone of your choice.

 use POSIX qw(tzset);
 $ENV{TZ} = 'CET';   
 POSIX::tzset();     


=head1 AUTHOR

Tobias Oetiker E<lt>oetiker@ee.ethz.chE<gt>

=cut
