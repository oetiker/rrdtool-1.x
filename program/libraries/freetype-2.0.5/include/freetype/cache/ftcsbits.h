/***************************************************************************/
/*                                                                         */
/*  ftcsbits.h                                                             */
/*                                                                         */
/*    A small-bitmap cache (specification).                                */
/*                                                                         */
/*  Copyright 2000-2001 by                                                 */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#ifndef __FTCSBITS_H__
#define __FTCSBITS_H__


#include <ft2build.h>
#include FT_CACHE_H
#include FT_CACHE_INTERNAL_CHUNK_H
#include FT_CACHE_IMAGE_H


FT_BEGIN_HEADER


  /*************************************************************************/
  /*                                                                       */
  /* <Section>                                                             */
  /*    cache_subsystem                                                    */
  /*                                                                       */
  /*************************************************************************/


  /*************************************************************************/
  /*                                                                       */
  /* <Type>                                                                */
  /*    FTC_SBit                                                           */
  /*                                                                       */
  /* <Description>                                                         */
  /*    A handle to a small bitmap descriptor.  See the FTC_SBitRec        */
  /*    structure for details.                                             */
  /*                                                                       */
  typedef struct FTC_SBitRec_*  FTC_SBit;


  /*************************************************************************/
  /*                                                                       */
  /* <Type>                                                                */
  /*    FTC_SBit_Cache                                                     */
  /*                                                                       */
  /* <Description>                                                         */
  /*    A handle to a small bitmap cache.  These are special cache objects */
  /*    used to store small glyph bitmaps (and anti-aliased pixmaps) in a  */
  /*    much more efficient way than the traditional glyph image cache     */
  /*    implemented by FTC_Image_Cache.                                    */
  /*                                                                       */
  typedef struct FTC_SBit_CacheRec_*  FTC_SBit_Cache;


  /*************************************************************************/
  /*                                                                       */
  /* <Struct>                                                              */
  /*    FTC_SBitRec                                                        */
  /*                                                                       */
  /* <Description>                                                         */
  /*    A very compact structure used to describe a small glyph bitmap.    */
  /*                                                                       */
  /* <Fields>                                                              */
  /*    width    :: The bitmap width in pixels.                            */
  /*                                                                       */
  /*    height   :: The bitmap height in pixels.                           */
  /*                                                                       */
  /*    left     :: The horizontal distance from the pen position to the   */
  /*                left bitmap border (a.k.a. `left side bearing', or     */
  /*                `lsb').                                                */
  /*                                                                       */
  /*    top      :: The vertical distance from the pen position (on the    */
  /*                baseline) to the upper bitmap border (a.k.a. `top side */
  /*                bearing').  The distance is positive for upwards       */
  /*                Y coordinates.                                         */
  /*                                                                       */
  /*    format   :: The format of the glyph bitmap (monochrome or gray).   */
  /*                                                                       */
  /*    pitch    :: The number of bytes per bitmap line.  May be positive  */
  /*                or negative.                                           */
  /*                                                                       */
  /*    xadvance :: The horizontal advance width in pixels.                */
  /*                                                                       */
  /*    yadvance :: The vertical advance height in pixels.                 */
  /*                                                                       */
  /*    buffer   :: A pointer to the bitmap pixels.                        */
  /*                                                                       */
  typedef struct  FTC_SBitRec_
  {
    FT_Byte   width;
    FT_Byte   height;
    FT_Char   left;
    FT_Char   top;

    FT_Byte   format;
    FT_Char   pitch;
    FT_Char   xadvance;
    FT_Char   yadvance;

    FT_Byte*  buffer;

  } FTC_SBitRec;


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    FTC_SBit_Cache_New                                                 */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Creates a new cache to store small glyph bitmaps.                  */
  /*                                                                       */
  /* <Input>                                                               */
  /*    manager :: A handle to the source cache manager.                   */
  /*                                                                       */
  /* <Output>                                                              */
  /*    acache  :: A handle to the new sbit cache.  NULL in case of error. */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  FT_EXPORT( FT_Error )
  FTC_SBit_Cache_New( FTC_Manager      manager,
                      FTC_SBit_Cache  *acache );


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    FTC_SBit_Cache_Lookup                                              */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Looks up a given small glyph bitmap in a given sbit cache.         */
  /*                                                                       */
  /* <Input>                                                               */
  /*    cache  :: A handle to the source sbit cache.                       */
  /*    desc   :: A pointer to the glyph image descriptor.                 */
  /*    gindex :: The glyph index.                                         */
  /*                                                                       */
  /* <Output>                                                              */
  /*    sbit   :: A handle to a small bitmap descriptor.                   */
  /*                                                                       */
  /* <Return>                                                              */
  /*    FreeType error code.  0 means success.                             */
  /*                                                                       */
  /* <Note>                                                                */
  /*    The small bitmap descriptor and its bit buffer are owned by the    */
  /*    cache and should never be freed by the application.  They might    */
  /*    as well disappear from memory on the next cache lookup, so don't   */
  /*    treat them as persistent data.                                     */
  /*                                                                       */
  /*    The descriptor's `buffer' field is set to 0 to indicate a missing  */
  /*    glyph bitmap.                                                      */
  /*                                                                       */
  FT_EXPORT( FT_Error )
  FTC_SBit_Cache_Lookup( FTC_SBit_Cache   cache,
                         FTC_Image_Desc*  desc,
                         FT_UInt          gindex,
                         FTC_SBit        *sbit );


  /* */


FT_END_HEADER

#endif /* __FTCSBITS_H__ */


/* END */
