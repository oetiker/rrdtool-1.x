# Microsoft Developer Studio Project File - Name="freetype" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=freetype - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "freetype.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "freetype.mak" CFG="freetype - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "freetype - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "freetype - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "freetype - Win32 Release"

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
# ADD CPP /nologo /MT /W3 /GX /I "include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /i "include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "freetype - Win32 Debug"

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
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FD /GZ /c
# SUBTRACT CPP /YX
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

# Name "freetype - Win32 Release"
# Name "freetype - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\ahangles.c
# End Source File
# Begin Source File

SOURCE=.\ahglobal.c
# End Source File
# Begin Source File

SOURCE=.\ahglyph.c
# End Source File
# Begin Source File

SOURCE=.\ahhint.c
# End Source File
# Begin Source File

SOURCE=.\ahmodule.c
# End Source File
# Begin Source File

SOURCE=.\cff.c
# End Source File
# Begin Source File

SOURCE=.\cidgload.c
# End Source File
# Begin Source File

SOURCE=.\cidload.c
# End Source File
# Begin Source File

SOURCE=.\cidobjs.c
# End Source File
# Begin Source File

SOURCE=.\cidparse.c
# End Source File
# Begin Source File

SOURCE=.\cidriver.c
# End Source File
# Begin Source File

SOURCE=.\ftcalc.c
# End Source File
# Begin Source File

SOURCE=.\ftcchunk.c
# End Source File
# Begin Source File

SOURCE=.\ftcglyph.c
# End Source File
# Begin Source File

SOURCE=.\ftcimage.c
# End Source File
# Begin Source File

SOURCE=.\ftcmanag.c
# End Source File
# Begin Source File

SOURCE=.\ftcsbits.c
# End Source File
# Begin Source File

SOURCE=.\ftextend.c
# End Source File
# Begin Source File

SOURCE=.\ftglyph.c
# End Source File
# Begin Source File

SOURCE=.\ftgrays.c
# End Source File
# Begin Source File

SOURCE=.\ftinit.c
# End Source File
# Begin Source File

SOURCE=.\ftlist.c
# End Source File
# Begin Source File

SOURCE=.\ftlru.c
# End Source File
# Begin Source File

SOURCE=.\ftnames.c
# End Source File
# Begin Source File

SOURCE=.\ftobjs.c
# End Source File
# Begin Source File

SOURCE=.\ftoutln.c
# End Source File
# Begin Source File

SOURCE=.\ftraster.c
# End Source File
# Begin Source File

SOURCE=.\ftrend1.c
# End Source File
# Begin Source File

SOURCE=.\ftsmooth.c
# End Source File
# Begin Source File

SOURCE=.\ftstream.c
# End Source File
# Begin Source File

SOURCE=.\ftsystem.c
# End Source File
# Begin Source File

SOURCE=.\fttrigon.c
# End Source File
# Begin Source File

SOURCE=.\pcfdriver.c
# End Source File
# Begin Source File

SOURCE=.\pcfread.c
# End Source File
# Begin Source File

SOURCE=.\pcfutil.c
# End Source File
# Begin Source File

SOURCE=.\psauxmod.c
# End Source File
# Begin Source File

SOURCE=.\psmodule.c
# End Source File
# Begin Source File

SOURCE=.\psobjs.c
# End Source File
# Begin Source File

SOURCE=.\sfdriver.c
# End Source File
# Begin Source File

SOURCE=.\sfobjs.c
# End Source File
# Begin Source File

SOURCE=.\t1afm.c
# End Source File
# Begin Source File

SOURCE=.\t1decode.c
# End Source File
# Begin Source File

SOURCE=.\t1driver.c
# End Source File
# Begin Source File

SOURCE=.\t1gload.c
# End Source File
# Begin Source File

SOURCE=.\t1load.c
# End Source File
# Begin Source File

SOURCE=.\t1objs.c
# End Source File
# Begin Source File

SOURCE=.\t1parse.c
# End Source File
# Begin Source File

SOURCE=.\ttcmap.c
# End Source File
# Begin Source File

SOURCE=.\ttdriver.c
# End Source File
# Begin Source File

SOURCE=.\ttgload.c
# End Source File
# Begin Source File

SOURCE=.\ttinterp.c
# End Source File
# Begin Source File

SOURCE=.\ttload.c
# End Source File
# Begin Source File

SOURCE=.\ttobjs.c
# End Source File
# Begin Source File

SOURCE=.\ttpload.c
# End Source File
# Begin Source File

SOURCE=.\ttpost.c
# End Source File
# Begin Source File

SOURCE=.\ttsbit.c
# End Source File
# Begin Source File

SOURCE=.\winfnt.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
