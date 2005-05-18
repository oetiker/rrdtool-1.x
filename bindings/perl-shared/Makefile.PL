use ExtUtils::MakeMaker;
use Config;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

# Specify the location of the archive containing PIC compiled object files.
my $R = "-R" ;

for ($^O){
	/linux/ && do{ $R = "-Wl,--rpath -Wl,"};
	/hpux/  && do{ $R = "+b"};
}

my $librrd = "-L../../src/.libs/ $R\$(RPATH) -lrrd";

WriteMakefile(
    'NAME'         => 'RRDs',
    'VERSION_FROM' => 'RRDs.pm', # finds $VERSION
    'DEFINE'	   => "-DPERLPATCHLEVEL=$Config{PATCHLEVEL}",
    'INC'          => '-I../../src',
    # Perl will figure out which one is valid
    'dynamic_lib'  => {'OTHERLDFLAGS' => "$librrd -lm"},
    'realclean'    => {FILES => 't/demo?.rrd t/demo?.png' }
);
