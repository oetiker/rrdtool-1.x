/***************************************************************************/
/*                                                                         */
/*  ftcimage.c                                                             */
/*                                                                         */
/*    FreeType Image cache (body).                                         */
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
#include FT_CACHE_IMAGE_H
#include FT_INTERNAL_MEMORY_H

#include "ftcerror.h"

#include <string.h>     /* memcmp() */
#include <stdlib.h>     /* labs()   */


  /* the FT_Glyph image `glyph node' type */
  typedef struct  FTC_GlyphImageRec_
  {
    FTC_GlyphNodeRec  root;
    FT_Glyph          ft_glyph;

  } FTC_GlyphImageRec, *FTC_GlyphImage;


  /* the glyph image queue type */
  typedef struct  FTC_ImageSetRec_
  {
    FTC_GlyphSetRec  root;
    FTC_Image_Desc   description;

  } FTC_ImageSetRec, *FTC_ImageSet;


  typedef struct  FTC_Image_CacheRec_
  {
    FTC_Glyph_CacheRec  root;

  } FTC_Image_CacheRec;



  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                    GLYPH IMAGE NODES                          *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_CALLBACK_DEF( void )
  ftc_glyph_image_node_destroy( FTC_GlyphImage  node,
                                FTC_GlyphSet    gset )
  {
    FT_Memory  memory = gset->memory;


    FT_Done_Glyph( node->ft_glyph );
    FREE( node );
  }


  FT_CALLBACK_DEF( FT_Error )
  ftc_glyph_image_node_new( FTC_GlyphSet     gset,
                            FT_UInt          glyph_index,
                            FTC_GlyphImage  *anode )
  {
    FT_Memory       memory   = gset->memory;
    FTC_ImageSet    imageset = (FTC_ImageSet)gset;
    FT_Error        error;
    FTC_GlyphImage  node = 0;
    FT_Face         face;
    FT_Size         size;


    /* allocate node */
    if ( ALLOC( node, sizeof ( *node ) ) )
      goto Exit;

    /* initialize its inner fields */
    FTC_GlyphNode_Init( FTC_GLYPHNODE( node ), gset, glyph_index );

    /* we will now load the glyph image */
    error = FTC_Manager_Lookup_Size( gset->manager,
                                     &imageset->description.font,
                                     &face, &size );
    if ( !error )
    {
      FT_UInt  gindex = node->root.glyph_index;
      FT_UInt  load_flags  = FT_LOAD_DEFAULT;
      FT_UInt  image_type  = imageset->description.image_type;


      if ( FTC_IMAGE_FORMAT( image_type ) == ftc_image_format_bitmap )
      {
        load_flags |= FT_LOAD_RENDER;
        if ( image_type & ftc_image_flag_monochrome )
          load_flags |= FT_LOAD_MONOCHROME;

        /* disable embedded bitmaps loading if necessary */
        if ( image_type & ftc_image_flag_no_sbits )
          load_flags |= FT_LOAD_NO_BITMAP;
      }
      else if ( FTC_IMAGE_FORMAT( image_type ) == ftc_image_format_outline )
      {
        /* disable embedded bitmaps loading */
        load_flags |= FT_LOAD_NO_BITMAP;

        if ( image_type & ftc_image_flag_unscaled )
          load_flags |= FT_LOAD_NO_SCALE;
      }

      if ( image_type & ftc_image_flag_unhinted )
        load_flags |= FT_LOAD_NO_HINTING;

      if ( image_type & ftc_image_flag_autohinted )
        load_flags |= FT_LOAD_FORCE_AUTOHINT;

      error = FT_Load_Glyph( face, gindex, load_flags );
      if ( !error )
      {
        if ( face->glyph->format == ft_glyph_format_bitmap  ||
             face->glyph->format == ft_glyph_format_outline )
        {
          /* ok, copy it */
          FT_Glyph  glyph;


          error = FT_Get_Glyph( face->glyph, &glyph );
          if ( !error )
            node->ft_glyph = glyph;
        }
        else
          error = FTC_Err_Invalid_Argument;
      }
    }

  Exit:
    if ( error && node )
      FREE( node );

    *anode = node;
    return error;
  }


  /* this function is important because it is both part of */
  /* an FTC_GlyphSet_Class and an FTC_CacheNode_Class      */
  /*                                                       */
  FT_CALLBACK_DEF( FT_ULong )
  ftc_glyph_image_node_size( FTC_GlyphImage  node )
  {
    FT_ULong  size  = 0;
    FT_Glyph  glyph = node->ft_glyph;


    switch ( glyph->format )
    {
    case ft_glyph_format_bitmap:
      {
        FT_BitmapGlyph  bitg;


        bitg = (FT_BitmapGlyph)glyph;
        size = bitg->bitmap.rows * labs( bitg->bitmap.pitch ) +
               sizeof ( *bitg );
      }
      break;

    case ft_glyph_format_outline:
      {
        FT_OutlineGlyph  outg;


        outg = (FT_OutlineGlyph)glyph;
        size = outg->outline.n_points *
                 ( sizeof( FT_Vector ) + sizeof ( FT_Byte ) ) +
               outg->outline.n_contours * sizeof ( FT_Short ) +
               sizeof ( *outg );
      }
      break;

    default:
      ;
    }

    size += sizeof ( *node );
    return size;
  }


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                    GLYPH IMAGE SETS                           *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_CALLBACK_DEF( FT_Error )
  ftc_image_set_init( FTC_ImageSet     iset,
                      FTC_Image_Desc*  type )
  {
    iset->description = *type;
    return 0;
  }


  FT_CALLBACK_DEF( FT_Bool )
  ftc_image_set_compare( FTC_ImageSet     iset,
                         FTC_Image_Desc*  type )
  {
    return FT_BOOL( !memcmp( &iset->description, type, sizeof ( *type ) ) );
  }


  FT_CALLBACK_TABLE_DEF
  const FTC_GlyphSet_Class  ftc_glyph_image_set_class =
  {
    sizeof( FTC_ImageSetRec ),

    (FTC_GlyphSet_InitFunc)       ftc_image_set_init,
    (FTC_GlyphSet_DoneFunc)       0,
    (FTC_GlyphSet_CompareFunc)    ftc_image_set_compare,

    (FTC_GlyphSet_NewNodeFunc)    ftc_glyph_image_node_new,
    (FTC_GlyphSet_SizeNodeFunc)   ftc_glyph_image_node_size,
    (FTC_GlyphSet_DestroyNodeFunc)ftc_glyph_image_node_destroy
  };


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                    GLYPH IMAGE CACHE                          *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_CALLBACK_TABLE_DEF
  const FTC_Glyph_Cache_Class  ftc_glyph_image_cache_class =
  {
    {
      sizeof( FTC_Image_CacheRec ),
      (FTC_Cache_InitFunc) FTC_Glyph_Cache_Init,
      (FTC_Cache_DoneFunc) FTC_Glyph_Cache_Done
    },
    (FTC_GlyphSet_Class*) &ftc_glyph_image_set_class
  };


  /* documentation is in ftcimage.h */

  FT_EXPORT_DEF( FT_Error )
  FTC_Image_Cache_New( FTC_Manager       manager,
                       FTC_Image_Cache  *acache )
  {
    return FTC_Manager_Register_Cache(
             manager,
             (FTC_Cache_Class*)&ftc_glyph_image_cache_class,
             (FTC_Cache*)acache );
  }


  /* documentation is in ftcimage.h */

  FT_EXPORT_DEF( FT_Error )
  FTC_Image_Cache_Lookup( FTC_Image_Cache  cache,
                          FTC_Image_Desc*  desc,
                          FT_UInt          gindex,
                          FT_Glyph        *aglyph )
  {
    FT_Error       error;
    FTC_GlyphNode  node;


    /* some argument checks are delayed to FTC_Glyph_Cache_Lookup */

    if ( !aglyph )
      return FTC_Err_Invalid_Argument;

    error = FTC_Glyph_Cache_Lookup( (FTC_Glyph_Cache)cache,
                                    desc, gindex, &node );

    if ( !error )
      *aglyph = ((FTC_GlyphImage)node)->ft_glyph;

    return error;
  }


/* END */
