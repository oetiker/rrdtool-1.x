use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'	=> 'RRDs',
    'VERSION_FROM' => 'RRDs.pm', 
    #'DEFINE'	=> '-D_DEBUG -DWIN32 -D_CONSOLE',     
    'OPTIMIZE' => '-O2',
    'INC'	=> '-I../src/ -I../gd1.3',    
    #'LIBS' => ['-L../src/debug -lrrd.lib  -L../gd1.3/debug -lgd.lib'],
	#'LIBC' => 'libc.lib',
    'MYEXTLIB'  => '../src/release/rrd.lib ../gd1.3/release/gd.lib ..\zlib-1.1.3\Release\zlib.lib ..\libpng-1.0.3\Release\png.lib', 
    'realclean'    => {FILES => 't/demo?.rrd t/demo?.gif' }
);
