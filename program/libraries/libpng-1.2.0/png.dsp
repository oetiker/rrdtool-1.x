# Microsoft Developer Studio Project File - Name="png" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=png - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "png.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "png.mak" CFG="png - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "png - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "png - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "png - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /I "..\zlib-1.1.3" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# SUBTRACT CPP /O<none>
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "png - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /ML /W3 /Gm /GX /ZI /Od /I "..\zlib-1.1.3" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "png - Win32 Release"
# Name "png - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\png.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngerror.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngget.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngmem.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngpread.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngread.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngrio.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngrtran.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngrutil.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngset.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngtrans.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngwio.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngwrite.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngwtran.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# Begin Source File

SOURCE=.\pngwutil.c
# ADD CPP /I "..\zlib-1.1.4"
# SUBTRACT CPP /I "..\zlib-1.1.3"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
