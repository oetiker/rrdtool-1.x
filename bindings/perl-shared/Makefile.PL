use ExtUtils::MakeMaker;
use Config;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

# Specify the location of the archive containing PIC compiled object files.
my $R = $^O eq 'Linux' ? "-Wl,--rpath -Wl," : "-R" ;
my $librrd = "-L../../src/.libs/ $R\$(RPATH) -lrrd";

WriteMakefile(
    'NAME'         => 'RRDs',
    'VERSION_FROM' => 'RRDs.pm', # finds $VERSION
    'DEFINE'	   => "-DPERLPATCHLEVEL=$Config{PATCHLEVEL}",
    'INC'          => '-I../../src',
    # where to look for the necessary libraries 
    # Perl will figure out which one is valid
    'depend'	   => {'RRDs.c' => "../../src/.libs/librrd.so"},
    'dynamic_lib'  => {'OTHERLDFLAGS' => "$librrd -lm"},
    'realclean'    => {FILES => 't/demo?.rrd t/demo?.png' }
);
