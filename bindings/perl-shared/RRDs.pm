package RRDs;

use strict;
use vars qw(@ISA $VERSION);

@ISA = qw(DynaLoader);

require DynaLoader;

$VERSION=1.7002;

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
  RRDs::dump ...
  RRDs::restore ...
  RRDs::flushcached ...
  RRDs::register_fetch_cb ...
  $RRDs::VERSION

=head1 DESCRIPTION

=head2 Calling Sequence

This module accesses RRDtool functionality directly from within Perl. The
arguments to the functions listed in the SYNOPSIS are explained in the regular
RRDtool documentation. The command line call

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

The RRD functions will not abort your program even when they cannot make
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

B<RRDs::graph> returns an ARRAY containing the x-size and y-size of the
created image and a pointer to an array with the results of the PRINT arguments.

 ($result_arr,$xsize,$ysize) = RRDs::graph ...
 print "Imagesize: ${xsize}x${ysize}\n";
 print "Averages: ", (join ", ", @$averages);

B<RRDs::info> returns a pointer to a hash. The keys of the hash
represent the property names of the RRD and the values of the hash are
the values of the properties.  

 $hash = RRDs::info "example.rrd";
 foreach my $key (keys %$hash){
   print "$key = $$hash{$key}\n";
 }

B<RRDs::graphv> takes the same parameters as B<RRDs::graph> but it returns a
pointer to hash. The hash returned contains meta information about the
graph. Like its size as well as the position of the graph area on the image.
When calling with '-' as the filename then the contents of the graph will be
returned in the hash as well (key 'image').

B<RRDs::updatev> also returns a pointer to hash. The keys of the hash
are concatenated strings of a timestamp, RRA index, and data source name for
each consolidated data point (CDP) written to disk as a result of the
current update call. The hash values are CDP values.

B<RRDs::fetch> is the most complex of
the pack regarding return values. There are 4 values. Two normal
integers, a pointer to an array and a pointer to an array of pointers.

  my ($start,$step,$names,$data) = RRDs::fetch ... 
  print "Start:       ", scalar localtime($start), " ($start)\n";
  print "Step size:   $step seconds\n";
  print "DS names:    ", join (", ", @$names)."\n";
  print "Data points: ", $#$data + 1, "\n";
  print "Data:\n";
  for my $line (@$data) {
    print "  ", scalar localtime($start), " ($start) ";
    $start += $step;
    for my $val (@$line) {
      printf "%12.1f ", $val;
    }
    print "\n";
  }

B<RRDs::xport> exposes the L<rrdxport|rrdxport> functionality and returns data
with the following structure:

  my ($start,$end,$step,$cols,$names,$data) = RRDs::xport ...
  
  # $start : timestamp
  # $end   : timestamp
  # $step  : seconds
  # $cols  : number of returned columns
  # $names : arrayref with the names of the columns
  # $data  : arrayref of arrayrefs with the data (first index is time, second is column)

B<RRDs::times> returns two integers which are the number of seconds since
epoch (1970-01-01) for the supplied "start" and "end" arguments, respectively.

See the examples directory for more ways to use this extension.

=head2 Fetch Callback Function

Normally when using graph, xport or fetch the data you see will come from an
actual rrd file.  Some people who like the look of rrd charts, therefore
export their data from a database and then load it into an rrd file just to
be able to call rrdgraph on it. Using a custom callback, you can supply your own
code for handling the data requests from graph, xport and fetch.

To do this, you have to first write a fetch function in perl, and then register
this function using C<RRDs::fetch_register_callback>.

Finally you can use the pseudo path name B<cb//>[I<filename>] to tell
rrdtool to use your callback routine instead of the normal rrdtool fetch function
to organize the data required.

The callback function must look like this:

  sub fetch_callback {
      my $args_hash = shift;
      # {
      #  filename => 'cb//somefilename',
      #  cd => 'AVERAGE',
      #  start => 1401295291,   
      #  end => 1401295591,
      #  step => 300 }

      # do some clever thing to get that data ready    

      return {
          start => $unix_timestamp,
          step => $step_width,
          data => {
              dsName1 => [ value1, value2, ... ],
              dsName2 => [ value1, value2, ... ],
              dsName3 => [ value1, value2, ... ],
          }
     };
  }

=head1 NOTE

If you are manipulating the TZ variable you should also call the POSIX
function L<tzset(3)> to initialize all internal states of the library for properly
operating in the timezone of your choice.

 use POSIX qw(tzset);
 $ENV{TZ} = 'CET';   
 POSIX::tzset();     


=head1 AUTHOR

Tobias Oetiker E<lt>tobi@oetiker.chE<gt>

=cut
