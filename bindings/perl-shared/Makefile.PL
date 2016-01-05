use ExtUtils::MakeMaker;
use Config;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

if (($Config{'osname'} eq 'MSWin32' && $ENV{'OSTYPE'} eq '')) {
	my ($perlver) = ($] =~ /(\d+\.\d{3})/);
	$perlver =~ s/[.0]//g;

	my $perl_core_dir = "$Config{privlib}/CORE";
	my $vc_dir = $ENV{'VSINSTALLDIR'} || 'C:/Program Files (x86)/Microsoft Visual Studio 9.0/VC';
	my $sdk_dir = $ENV{'WindowsSdkDir'} || 'C:/Program Files/Microsoft SDKs/Windows/v6.0A';

	WriteMakefile(
		'NAME'         => 'RRDs',
		'VERSION_FROM' => 'RRDs.pm',
		'DEFINE'       => "-DPERLPATCHLEVEL=$Config{PATCHLEVEL} -D_CRT_SECURE_NO_WARNINGS -DWIN32",
		'INC'          => '-I../../src -I../../win32 "-I$perl_core_dir" -I"$vc_dir/include" -I"$sdk_dir/Include"',
		'LDDLFLAGS'    => '-dll -nologo -opt:ref,icf -ltcg -libpath:"$perl_core_dir" -machine:X86',
		'LDFLAGS'      => '-nologo -opt:ref,icf -ltcg -libpath:"$perl_core_dir" -machine:X86',
		'OPTIMIZE'     => '-O2 -MD',
		'LIBS'         => qq{"../../win32/librrd-4.lib" "perl$perlver.lib" -L../../contrib/lib -L../../win32 -L"$sdk_dir/lib" -L"$vc_dir/lib" -L"$perl_core_dir"},
		'realclean'    => {FILES => 't/demo?.rrd t/demo?.png' },
		($] ge '5.005') ? (
			'AUTHOR'   => 'Tobias Oetiker (tobi@oetiker.ch)',
			'ABSTRACT' => 'Round Robin Database Tool',
		) : ()
	);
}else{
	my $TOP_SRCDIR = $ENV{'ABS_TOP_SRCDIR'} || '../..';
	my $TOP_BUILDDIR = $ENV{'ABS_TOP_BUILDDIR'} || '../..';
	my $SRCDIR = $ENV{'ABS_SRCDIR'} || '.';
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
			/bsd/     && do{ $R = "-Wl,--rpath -Wl,$rp"};
			/aix/     && do{ $R = "-blibpath:$rp"};
		}
	}

	# darwin works without this because librrd contains its
	# install_name which will includes the final location of the
	# library after it is installed. This install_name gets transferred
	# to the perl shared object.
	my $librrd;
	if ($^O eq 'darwin'){
        $librrd = '-lrrd';
	} else {
		$librrd = "-L${TOP_BUILDDIR}/src/.libs/ $R -lrrd";
	}

	WriteMakefile(
		'NAME'         => 'RRDs',
		'VERSION_FROM' => 'RRDs.pm', # finds $VERSION
		'DEFINE'	   => "-DPERLPATCHLEVEL=$Config{PATCHLEVEL}",
		'INC'          => "-I${TOP_BUILDDIR}/src -I${TOP_SRCDIR}/src",
		# Perl will figure out which one is valid
		#'dynamic_lib'  => {'OTHERLDFLAGS' => "$librrd -lm"},
		'depend'       => {'RRDs.c' => "${TOP_BUILDDIR}/src/librrd.la"},
		'LDFROM'       => '$(OBJECT) '.$librrd, 
		'realclean'    => {FILES => 't/demo?.rrd t/demo?.png' },
		($^O eq 'darwin') ? ( 'LDDLFLAGS'    => "-L${TOP_BUILDDIR}/src/.libs/ $Config{lddlflags}" ) : ()
	);
}

