use ExtUtils::MakeMaker;
use Config;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

# if the last argument when calling Makefile.PL is RPATH=/... and ... is the
# path to librrd.so then the Makefile will be written such that RRDs.so knows
# where to find librrd.so later on ... 
my $R="";
if ($ARGV[-1] =~ /RPATH=(\S+)/){
	pop @ARGV;
	my $rp = $1;
	for ($^O){
	        /linux/   && do{ $R = "-Wl,--rpath -Wl,$rp"};
	        /hpux/    && do{ $R = "+b$rp"};
	        /solaris/ && do{ $R = "-R$rp"};
	        /bsd/     && do{ $R = "-R$rp"};
                /aix/     && do{ $R = "-Wl,-blibpath:$rp"};
	}
}

# darwin works without this because librrd contains its
# install_name which will includes the final location of the
# library after it is installed. This install_name gets transfered
# to the perl shared object.

my $librrd;
if ($^O eq 'darwin'){
        $librrd = '-lrrd';
}
else {
        $librrd = "-L../../src/.libs/ $R -lrrd";
}

WriteMakefile(
    'NAME'         => 'RRDs',
    'VERSION_FROM' => 'RRDs.pm', # finds $VERSION
    'DEFINE'	   => "-DPERLPATCHLEVEL=$Config{PATCHLEVEL}",
    'INC'          => '-I../../src',
    # Perl will figure out which one is valid
    #'dynamic_lib'  => {'OTHERLDFLAGS' => "$librrd -lm"},
    'depend'       => {'RRDs.c' => "../../src/librrd.la"},
    'LDFROM'       => '$(OBJECT) '.$librrd, 
    'realclean'    => {FILES => 't/demo?.rrd t/demo?.png' },
    ($^O eq 'darwin') ? ( 'LDDLFLAGS'    => "-L../../src/.libs/ $Config{lddlflags}" ) : ()
);

