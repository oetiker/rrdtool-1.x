# Microsoft Developer Studio Project File - Name="gd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=gd - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "gd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "gd.mak" CFG="gd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "gd - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "gd - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "gd - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "." /I "..\libpng-1.0.3" /I "..\zlib-1.1.3" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_CTYPE_DISABLE_MACROS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x100c
# ADD RSC /l 0x100c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "gd - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\libpng-1.0.3" /I "..\zlib-1.1.3" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "_CTYPE_DISABLE_MACROS" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x100c
# ADD RSC /l 0x100c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "gd - Win32 Release"
# Name "gd - Win32 Debug"
# Begin Source File

SOURCE=.\GD.C
# End Source File
# Begin Source File

SOURCE=.\gdfontg.c
# End Source File
# Begin Source File

SOURCE=.\gdfontl.c
# End Source File
# Begin Source File

SOURCE=.\GDFONTMB.C
# End Source File
# Begin Source File

SOURCE=.\GDFONTS.C
# End Source File
# Begin Source File

SOURCE=.\gdfontt.c
# End Source File
# Begin Source File

SOURCE=.\gdlucidab10.c
# End Source File
# Begin Source File

SOURCE=.\gdlucidab12.c
# End Source File
# Begin Source File

SOURCE=.\gdlucidab14.c
# End Source File
# Begin Source File

SOURCE=.\gdlucidan10.c
# End Source File
# Begin Source File

SOURCE=.\gdlucidan12.c
# End Source File
# Begin Source File

SOURCE=.\gdlucidan14.c
# End Source File
# End Target
# End Project
