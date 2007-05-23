# Microsoft Developer Studio Project File - Name="rrdtool" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=rrdtool - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "rrdtool.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "rrdtool.mak" CFG="rrdtool - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "rrdtool - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "rrdtool - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "rrdtool - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "rrdtool_"
# PROP BASE Intermediate_Dir "rrdtool_"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "toolrelease"
# PROP Intermediate_Dir "toolrelease"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /I "../src" /I "../../zlib-1.2.3" /I "../../libpng-1.2.16" /I "../../libart_lgpl-2.3.17" /I "../../freetype-2.3.1/include" /D "HAVE_CONFIG_H" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_CTYPE_DISABLE_MACROS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x100c /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 libpng.lib zlib.lib libart.lib freetype231MT.lib kernel32.lib user32.lib /nologo /subsystem:console /incremental:yes /debug /machine:I386 /libpath:"../../libpng-1.2.16/projects/visualc6/Win32_LIB_Release" /libpath:"../../zlib-1.2.3" /libpath:"../../libart_lgpl-2.3.17/win32/release" /libpath:"../../freetype-2.3.1/objs"

!ELSEIF  "$(CFG)" == "rrdtool - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "rrdtool0"
# PROP BASE Intermediate_Dir "rrdtool0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "tooldebug"
# PROP Intermediate_Dir "tooldebug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /Gm /GX /ZI /Od /I "../src" /I "../../zlib-1.2.3" /I "../../libpng-1.2.16" /I "../../libart_lgpl-2.3.17" /I "../../freetype-2.3.1/include" /D "HAVE_CONFIG_H" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "_CTYPE_DISABLE_MACROS" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x100c /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo /o"rrdtool.bsc"
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 libpng.lib zlib.lib libart.lib freetype231MT.lib kernel32.lib user32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept /libpath:"../../libpng-1.2.16/projects/visualc6/Win32_LIB_Release" /libpath:"../../zlib-1.2.3" /libpath:"../../libart_lgpl-2.3.17/win32/release" /libpath:"../../freetype-2.3.1/objs"

!ENDIF 

# Begin Target

# Name "rrdtool - Win32 Release"
# Name "rrdtool - Win32 Debug"
# Begin Source File

SOURCE=..\src\rrd_tool.c
# End Source File
# End Target
# End Project
