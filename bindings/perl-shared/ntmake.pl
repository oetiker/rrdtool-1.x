use ExtUtils::MakeMaker;
use Config;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
# This file is current set to compile with ActiveState 5xx builds
# (perl 5.005_03). Hopefully this lowest common denominator
# approach will work with newer ActiveState builds (i.e. 6xx).
WriteMakefile(
    'NAME'	=> 'RRDs',
    'VERSION_FROM' => 'RRDs.pm',
#    'DEFINE'	   => "-DPERLPATCHLEVEL=$Config{PATCHLEVEL}",
    'DEFINE'	   => "-DPERLPATCHLEVEL=5",

   'INC'	=> '-I../../src/ -I../../libraries/freetype-2.0.5/include -I ../../libraries/libart_lgpl-2.3.7 -I ../../libraries/zlib-1.1.4 -I ../../libraries/libpng-1.2.0',
   'OPTIMIZE' => '-O2 -MT',
# change this path to refer to your libc.lib
    'MYEXTLIB'  => 'c:/vc98/lib/libcmt.lib ../../src/release/rrd.lib ../../libraries/libart_lgpl-2.3.7/release/libart.lib ../../libraries/zlib-1.1.4/release/zlib.lib ../../libraries/libpng-1.2.0\release\png.lib ../../libraries/freetype-2.0.5/release/freetype.lib', 
    'realclean'    => {FILES => 't/demo?.rrd t/demo?.png' },
    ($] ge '5.005') ? (
        'AUTHOR' => 'Tobias Oetiker (oetiker@ee.ethz.ch)',
        'ABSTRACT' => 'Round Robin Database Tool',
    ) : ()


);
