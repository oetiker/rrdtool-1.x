/***************************************************************************/
/*                                                                         */
/*  ftcsbits.c                                                             */
/*                                                                         */
/*    FreeType sbits manager (body).                                       */
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


#include <ft2build.h>
#include FT_CACHE_H
#include FT_CACHE_SMALL_BITMAPS_H
#include FT_INTERNAL_OBJECTS_H
#include FT_INTERNAL_DEBUG_H
#include FT_ERRORS_H

#include "ftcerror.h"

#include <string.h>         /* memcmp() */


#define FTC_SBITSET_ELEMENT_COUNT  16


  typedef struct  FTC_SBitSetRec_
  {
    FTC_ChunkSetRec  root;
    FTC_Image_Desc   desc;

  } FTC_SBitSetRec, *FTC_SBitSet;


  typedef struct  FTC_SBit_CacheRec_
  {
    FTC_Chunk_CacheRec  root;

  } FTC_SBit_CacheRec;



  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                     SBIT CACHE NODES                          *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_CALLBACK_DEF( void )
  ftc_sbit_chunk_node_destroy( FTC_ChunkNode  node )
  {
    FTC_ChunkSet  cset   = node->cset;
    FT_Memory     memory = cset->memory;
    FT_UInt       count  = node->num_elements;
    FTC_SBit      sbit   = (FTC_SBit)node->elements;


    for ( ; count > 0; sbit++, count-- )
      FREE( sbit->buffer );

    FREE( node->elements );
    FREE( node );
  }


  FT_CALLBACK_DEF( FT_Error )
  ftc_bitmap_copy( FT_Memory   memory,
                   FT_Bitmap*  source,
                   FTC_SBit    target )
  {
    FT_Error  error;
    FT_Int    pitch = source->pitch;
    FT_ULong  size;


    if ( pitch < 0 )
      pitch = -pitch;

    size = (FT_ULong)( pitch * source->rows );

    if ( !ALLOC( target->buffer, size ) )
      MEM_Copy( target->buffer, source->buffer, size );

    return error;
  }


  FT_CALLBACK_DEF( FT_Error )
  ftc_sbit_chunk_node_new( FTC_ChunkSet    cset,
                           FT_UInt         index,
                           FTC_ChunkNode  *anode )
  {
    FT_Error       error;
    FT_Memory      memory  = cset->memory;
    FTC_SBitSet    sbitset = (FTC_SBitSet)cset;
    FTC_ChunkNode  node    = 0;
    FT_Face        face;
    FT_Size        size;


    /* allocate node */
    if ( ALLOC( node, sizeof ( *node ) ) )
      goto Exit;

    /* initialize its inner fields */
    error = FTC_ChunkNode_Init( node, cset, index, 1 );
    if ( error )
      goto Exit;

    /* we will now load all glyph images for this chunk */
    error = FTC_Manager_Lookup_Size( cset->manager,
                                     &sbitset->desc.font,
                                     &face, &size );
    if ( !error )
    {
      FT_UInt   glyph_index = index * cset->element_count;
      FT_UInt   load_flags  = FT_LOAD_DEFAULT;
      FT_UInt   image_type  = sbitset->desc.image_type;
      FT_UInt   count       = node->num_elements;
      FTC_SBit  sbit        = (FTC_SBit)node->elements;


      /* determine load flags, depending on the font description's */
      /* image type                                                */

      if ( FTC_IMAGE_FORMAT( image_type ) == ftc_image_format_bitmap )
      {
        if ( image_type & ftc_image_flag_monochrome )
          load_flags |= FT_LOAD_MONOCHROME;

        /* disable embedded bitmaps loading if necessary */
        if ( image_type & ftc_image_flag_no_sbits )
          load_flags |= FT_LOAD_NO_BITMAP;
      }
      else
      {
        FT_ERROR(( "FTC_SBit_Cache: cannot load scalable glyphs in an"
                   " sbit cache, please check your arguments!\n" ));
        error = FTC_Err_Invalid_Argument;
        goto Exit;
      }

      /* always render glyphs to bitmaps */
      load_flags |= FT_LOAD_RENDER;

      if ( image_type & ftc_image_flag_unhinted )
        load_flags |= FT_LOAD_NO_HINTING;

      if ( image_type & ftc_image_flag_autohinted )
        load_flags |= FT_LOAD_FORCE_AUTOHINT;

      /* load a chunk of small bitmaps in a row */
      for ( ; count > 0; count--, glyph_index++, sbit++ )
      {
        /* by default, indicates a `missing' glyph */
        sbit->buffer = 0;

        error = FT_Load_Glyph( face, glyph_index, load_flags );
        if ( !error )
        {
          FT_Int        temp;
          FT_GlyphSlot  slot   = face->glyph;
          FT_Bitmap*    bitmap = &slot->bitmap;
          FT_Int        xadvance, yadvance;


          /* check that our values fit into 8-bit containers!       */
          /* If this is not the case, our bitmap is too large       */
          /* and we will leave it as `missing' with sbit.buffer = 0 */

#define CHECK_CHAR( d )  ( temp = (FT_Char)d, temp == d )
#define CHECK_BYTE( d )  ( temp = (FT_Byte)d, temp == d )

          /* XXX: FIXME: add support for vertical layouts maybe */

          /* horizontal advance in pixels */
          xadvance = ( slot->metrics.horiAdvance + 32 ) >> 6;
          yadvance = ( slot->metrics.vertAdvance + 32 ) >> 6;

          if ( CHECK_BYTE( bitmap->rows  )     &&
               CHECK_BYTE( bitmap->width )     &&
               CHECK_CHAR( bitmap->pitch )     &&
               CHECK_CHAR( slot->bitmap_left ) &&
               CHECK_CHAR( slot->bitmap_top  ) &&
               CHECK_CHAR( xadvance )          &&
               CHECK_CHAR( yadvance )          )
          {
            sbit->width    = (FT_Byte)bitmap->width;
            sbit->height   = (FT_Byte)bitmap->rows;
            sbit->pitch    = (FT_Char)bitmap->pitch;
            sbit->left     = (FT_Char)slot->bitmap_left;
            sbit->top      = (FT_Char)slot->bitmap_top;
            sbit->xadvance = (FT_Char)xadvance;
            sbit->yadvance = (FT_Char)yadvance;
            sbit->format   = (FT_Byte)bitmap->pixel_mode;

            /* grab the bitmap when possible */
            if ( slot->flags & ft_glyph_own_bitmap )
            {
              slot->flags &= ~ft_glyph_own_bitmap;
              sbit->buffer = bitmap->buffer;
            }
            else
            {
              /* copy the bitmap into a new buffer -- ignore error */
              ftc_bitmap_copy( memory, bitmap, sbit );
            }
          }
        }
      }

      /* ignore the errors that might have occurred --        */
      /* we recognize unloaded glyphs with `sbit.buffer == 0' */
      error = 0;
    }

  Exit:
    if ( error && node )
    {
      FREE( node->elements );
      FREE( node );
    }

    *anode = node;

    return error;
  }


  /* this function is important because it is both part of */
  /* an FTC_ChunkSet_Class and an FTC_CacheNode_Class      */
  /*                                                       */
  FT_CALLBACK_DEF( FT_ULong )
  ftc_sbit_chunk_node_size( FTC_ChunkNode  node )
  {
    FT_ULong      size;
    FTC_ChunkSet  cset  = node->cset;
    FT_UInt       count = node->num_elements;
    FT_Int        pitch;
    FTC_SBit      sbit  = (FTC_SBit)node->elements;


    /* the node itself */
    size  = sizeof ( *node );

    /* the sbit records */
    size += cset->element_count * sizeof ( FTC_SBitRec );

    for ( ; count > 0; count--, sbit++ )
    {
      if ( sbit->buffer )
      {
        pitch = sbit->pitch;
        if ( pitch < 0 )
          pitch = -pitch;

        /* add the size of a given glyph image */
        size += pitch * sbit->height;
      }
    }

    return size;
  }


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                     SBIT CHUNK SETS                           *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_CALLBACK_DEF( FT_Error )
  ftc_sbit_chunk_set_sizes( FTC_ChunkSet     cset,
                            FTC_Image_Desc*  desc )
  {
    FT_Error  error;
    FT_Face   face;


    cset->element_count = FTC_SBITSET_ELEMENT_COUNT;
    cset->element_size  = sizeof ( FTC_SBitRec );

    /* lookup the FT_Face to obtain the number of glyphs */
    error = FTC_Manager_Lookup_Face( cset->manager,
                                     desc->font.face_id, &face );
    if ( !error )
      cset->element_max = face->num_glyphs;

    return error;
  }


  FT_CALLBACK_DEF( FT_Error )
  ftc_sbit_chunk_set_init( FTC_SBitSet      sset,
                           FTC_Image_Desc*  type )
  {
    sset->desc = *type;

    return 0;
  }


  FT_CALLBACK_DEF( FT_Bool )
  ftc_sbit_chunk_set_compare( FTC_SBitSet      sset,
                              FTC_Image_Desc*  type )
  {
    return FT_BOOL( !memcmp( &sset->desc, type, sizeof ( *type ) ) );
  }


  FT_CALLBACK_TABLE_DEF
  const FTC_ChunkSet_Class  ftc_sbit_chunk_set_class =
  {
    sizeof( FTC_SBitSetRec ),

    (FTC_ChunkSet_InitFunc)       ftc_sbit_chunk_set_init,
    (FTC_ChunkSet_DoneFunc)       0,
    (FTC_ChunkSet_CompareFunc)    ftc_sbit_chunk_set_compare,
    (FTC_ChunkSet_SizesFunc)      ftc_sbit_chunk_set_sizes,

    (FTC_ChunkSet_NewNodeFunc)    ftc_sbit_chunk_node_new,
    (FTC_ChunkSet_SizeNodeFunc)   ftc_sbit_chunk_node_size,
    (FTC_ChunkSet_DestroyNodeFunc)ftc_sbit_chunk_node_destroy
  };


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                     SBITS CACHE                               *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_CALLBACK_TABLE_DEF
  const FTC_Chunk_Cache_Class  ftc_sbit_cache_class =
  {
    {
      sizeof( FTC_SBit_CacheRec ),
      (FTC_Cache_InitFunc)FTC_Chunk_Cache_Init,
      (FTC_Cache_DoneFunc)FTC_Chunk_Cache_Done
    },
    (FTC_ChunkSet_Class*)&ftc_sbit_chunk_set_class
  };


  /* documentation is in ftcsbits.h */

  FT_EXPORT_DEF( FT_Error )
  FTC_SBit_Cache_New( FTC_Manager      manager,
                      FTC_SBit_Cache  *acache )
  {
    return FTC_Manager_Register_Cache(
             manager,
             (FTC_Cache_Class*)&ftc_sbit_cache_class,
             (FTC_Cache*)acache );
  }


  /* documentation is in ftcsbits.h */

  FT_EXPORT_DEF( FT_Error )
  FTC_SBit_Cache_Lookup( FTC_SBit_Cache   cache,
                         FTC_Image_Desc*  desc,
                         FT_UInt          gindex,
                         FTC_SBit        *ansbit )
  {
    FT_Error       error;
    FTC_ChunkNode  node;
    FT_UInt        cindex;


    /* argument checks delayed to FTC_Chunk_Cache_Lookup */
    if ( !ansbit )
      return FTC_Err_Invalid_Argument;

    *ansbit = 0;
    error   = FTC_Chunk_Cache_Lookup( &cache->root, desc, gindex,
                                      &node, &cindex );
    if ( !error )
      *ansbit = (FTC_SBit)node->elements + cindex;

    return error;
  }


/* END */
