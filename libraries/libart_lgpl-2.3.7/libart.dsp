# Microsoft Developer Studio Project File - Name="libart" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libart - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libart.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libart.mak" CFG="libart - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libart - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libart - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libart - Win32 Release"

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
# ADD CPP /nologo /W3 /GX /I "..\..\confignt" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "LIBART_COMPILATION" /YX /FD /c
# SUBTRACT CPP /O<none>
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libart - Win32 Debug"

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
# ADD CPP /nologo /ML /W3 /Gm /GX /ZI /Od /I "..\..\confignt" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "LIBART_COMPILATION" /YX /FD /GZ /c
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

# Name "libart - Win32 Release"
# Name "libart - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\art_affine.c
# End Source File
# Begin Source File

SOURCE=.\art_alphagamma.c
# End Source File
# Begin Source File

SOURCE=.\art_bpath.c
# End Source File
# Begin Source File

SOURCE=.\art_gray_svp.c
# End Source File
# Begin Source File

SOURCE=.\art_misc.c
# End Source File
# Begin Source File

SOURCE=.\art_pixbuf.c
# End Source File
# Begin Source File

SOURCE=.\art_rect.c
# End Source File
# Begin Source File

SOURCE=.\art_rect_svp.c
# End Source File
# Begin Source File

SOURCE=.\art_rect_uta.c
# End Source File
# Begin Source File

SOURCE=.\art_render.c
# End Source File
# Begin Source File

SOURCE=.\art_render_gradient.c
# End Source File
# Begin Source File

SOURCE=.\art_render_svp.c
# End Source File
# Begin Source File

SOURCE=.\art_rgb.c
# End Source File
# Begin Source File

SOURCE=.\art_rgb_a_affine.c
# End Source File
# Begin Source File

SOURCE=.\art_rgb_affine.c
# End Source File
# Begin Source File

SOURCE=.\art_rgb_affine_private.c
# End Source File
# Begin Source File

SOURCE=.\art_rgb_bitmap_affine.c
# End Source File
# Begin Source File

SOURCE=.\art_rgb_pixbuf_affine.c
# End Source File
# Begin Source File

SOURCE=.\art_rgb_rgba_affine.c
# End Source File
# Begin Source File

SOURCE=.\art_rgb_svp.c
# End Source File
# Begin Source File

SOURCE=.\art_rgba.c
# End Source File
# Begin Source File

SOURCE=.\art_svp.c
# End Source File
# Begin Source File

SOURCE=.\art_svp_intersect.c
# End Source File
# Begin Source File

SOURCE=.\art_svp_ops.c
# End Source File
# Begin Source File

SOURCE=.\art_svp_point.c
# End Source File
# Begin Source File

SOURCE=.\art_svp_render_aa.c
# End Source File
# Begin Source File

SOURCE=.\art_svp_vpath.c
# End Source File
# Begin Source File

SOURCE=.\art_svp_vpath_stroke.c
# End Source File
# Begin Source File

SOURCE=.\art_svp_wind.c
# End Source File
# Begin Source File

SOURCE=.\art_uta.c
# End Source File
# Begin Source File

SOURCE=.\art_uta_ops.c
# End Source File
# Begin Source File

SOURCE=.\art_uta_rect.c
# End Source File
# Begin Source File

SOURCE=.\art_uta_svp.c
# End Source File
# Begin Source File

SOURCE=.\art_uta_vpath.c
# End Source File
# Begin Source File

SOURCE=.\art_vpath.c
# End Source File
# Begin Source File

SOURCE=.\art_vpath_bpath.c
# End Source File
# Begin Source File

SOURCE=.\art_vpath_dash.c
# End Source File
# Begin Source File

SOURCE=.\art_vpath_svp.c
# End Source File
# Begin Source File

SOURCE=".\libart-features.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
