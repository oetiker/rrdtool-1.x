use ExtUtils::MakeMaker;
use Config;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

# Specify the location of the archive containing PIC compiled object files.
my $R = "";


for ($^O){
        /linux/   && do{ $R = "-Wl,--rpath -Wl,\$(RPATH)"};
        /hpux/    && do{ $R = "+b\$(RPATH)"};
        /solaris/ && do{ $R = "-R\$(RPATH)"};
}

# darwin works without this because librrd contains its
# install_name which will includes the final location of the
# library after it is installed. This install_name gets transfered
# to the perl shared object.

my $librrd = "-L../../src/.libs/ $R -lrrd";

WriteMakefile(
    'NAME'         => 'RRDs',
    'VERSION_FROM' => 'RRDs.pm', # finds $VERSION
    'DEFINE'	   => "-DPERLPATCHLEVEL=$Config{PATCHLEVEL}",
    'INC'          => '-I../../src',
    # Perl will figure out which one is valid
    'dynamic_lib'  => {'OTHERLDFLAGS' => "$librrd -lm"},
    'realclean'    => {FILES => 't/demo?.rrd t/demo?.png' }
);
