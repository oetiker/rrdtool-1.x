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
# ADD CPP /nologo /MT /W3 /GX /O2 /I "." /I "..\gd1.3" /I "..\libpng-1.0.3" /I "..\zlib-1.1.3" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_CTYPE_DISABLE_MACROS" /FD /c
# SUBTRACT CPP /X /YX
# ADD BASE RSC /l 0x100c
# ADD RSC /l 0x100c
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
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "." /I "..\gd1.3" /I "..\libpng-1.0.3" /I "..\zlib-1.1.3" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "_CTYPE_DISABLE_MACROS" /FR /FD /c
# SUBTRACT CPP /X /YX
# ADD BASE RSC /l 0x100c
# ADD RSC /l 0x100c
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

SOURCE=.\gdpng.c
# End Source File
# Begin Source File

SOURCE=.\getopt.c
# End Source File
# Begin Source File

SOURCE=.\getopt1.c
# End Source File
# Begin Source File

SOURCE=.\gifsize.c
# End Source File
# Begin Source File

SOURCE=.\parsetime.c
# End Source File
# Begin Source File

SOURCE=.\pngsize.c
# End Source File
# Begin Source File

SOURCE=.\rrd_create.c
# End Source File
# Begin Source File

SOURCE=.\rrd_diff.c
# End Source File
# Begin Source File

SOURCE=.\rrd_dump.c
# End Source File
# Begin Source File

SOURCE=.\rrd_error.c
# End Source File
# Begin Source File

SOURCE=.\rrd_fetch.c
# End Source File
# Begin Source File

SOURCE=.\rrd_format.c
# End Source File
# Begin Source File

SOURCE=.\rrd_graph.c
# End Source File
# Begin Source File

SOURCE=.\rrd_last.c
# End Source File
# Begin Source File

SOURCE=.\rrd_open.c
# End Source File
# Begin Source File

SOURCE=.\rrd_resize.c
# End Source File
# Begin Source File

SOURCE=.\rrd_restore.c
# End Source File
# Begin Source File

SOURCE=.\rrd_tune.c
# End Source File
# Begin Source File

SOURCE=.\rrd_update.c
# End Source File
# End Target
# End Project
