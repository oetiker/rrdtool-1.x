# Microsoft Developer Studio Project File - Name="rrd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=rrd - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "rrd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "rrd.mak" CFG="rrd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "rrd - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "rrd - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "rrd - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "rrd___Wi"
# PROP BASE Intermediate_Dir "rrd___Wi"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /I "../src" /I "../../zlib-1.2.3" /I "../../libpng-1.2.16" /I "../../libart_lgpl-2.3.17" /I "../../freetype-2.3.1/include" /D "HAVE_CONFIG_H" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_CTYPE_DISABLE_MACROS" /FD /c
# SUBTRACT CPP /X /YX
# ADD BASE RSC /l 0x100c
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "rrd - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "rrd___W0"
# PROP BASE Intermediate_Dir "rrd___W0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /Gm /GX /ZI /Od /I "../src" /I "../../zlib-1.2.3" /I "../../libpng-1.2.16" /I "../../libart_lgpl-2.3.17" /I "../../freetype-2.3.1/include" /D "HAVE_CONFIG_H" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "_CTYPE_DISABLE_MACROS" /FR /FD /c
# SUBTRACT CPP /X /YX
# ADD BASE RSC /l 0x100c
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo /o"rrd.bsc"
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "rrd - Win32 Release"
# Name "rrd - Win32 Debug"
# Begin Source File

SOURCE="..\src\get_ver.awk"

!IF  "$(CFG)" == "rrd - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Creating ..\rrd_config.h
InputPath="..\src\get_ver.awk"

"..\rrd_config.h" : $(SOURCE) "..\configure.ac" "..\win32\rrd_config.h.msvc"
	awk -f ..\src\get_ver.awk ..\configure.ac ..\win32\rrd_config.h.msvc > ..\rrd_config.h

# End Custom Build

!ELSEIF  "$(CFG)" == "rrd - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Creating ..\rrd_config.h
InputPath="..\src\get_ver.awk"

"..\rrd_config.h" : $(SOURCE) "..\configure.ac" "..\win32\rrd_config.h.msvc"
	awk -f ..\src\get_ver.awk ..\configure.ac ..\win32\rrd_config.h.msvc > ..\rrd_config.h

# End Custom Build

!ENDIF

# End Source File
# Begin Source File

SOURCE=..\src\rrd_afm.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_afm_data.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_create.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_diff.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_dump.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_error.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_fetch.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_first.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_format.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_gfx.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_graph.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_graph_helper.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_hw.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_info.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_last.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_lastupdate.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_nan_inf.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_open.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_resize.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_restore.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_rpncalc.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_thread_safe_nt.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_tune.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_update.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_version.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_xport.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_getopt.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_getopt1.c
# End Source File
# Begin Source File

SOURCE=..\src\art_rgba_svp.c
# End Source File
# Begin Source File

SOURCE=..\src\hash_32.c
# End Source File
# Begin Source File

SOURCE=..\src\rrd_parsetime.c
# End Source File
# Begin Source File

SOURCE=..\src\pngsize.c
# End Source File
# Begin Source File

SOURCE=..\src\strftime.c
# End Source File
# End Target
# End Project
