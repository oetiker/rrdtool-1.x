/***************************************************************************/
/*                                                                         */
/*  ftcmanag.c                                                             */
/*                                                                         */
/*    FreeType Cache Manager (body).                                       */
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
#include FT_CACHE_MANAGER_H
#include FT_INTERNAL_OBJECTS_H
#include FT_INTERNAL_DEBUG_H
#include FT_LIST_H
#include FT_SIZES_H

#include "ftcerror.h"


#undef  FT_COMPONENT
#define FT_COMPONENT  trace_cache

#define FTC_LRU_GET_MANAGER( lru )  (FTC_Manager)lru->user_data


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****               FACE & SIZE LRU CALLBACKS                       *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_CALLBACK_DEF( FT_Error )
  ftc_manager_init_face( FT_Lru      lru,
                         FT_LruNode  node )
  {
    FTC_Manager  manager = FTC_LRU_GET_MANAGER( lru );
    FT_Error     error;
    FT_Face      face;


    error = manager->request_face( (FTC_FaceID)node->key,
                                   manager->library,
                                   manager->request_data,
                                   (FT_Face*)&node->root.data );
    if ( !error )
    {
      /* destroy initial size object; it will be re-created later */
      face = (FT_Face)node->root.data;
      if ( face->size )
        FT_Done_Size( face->size );
    }

    return error;
  }


  /* helper function for ftc_manager_done_face() */
  FT_CALLBACK_DEF( FT_Bool )
  ftc_manager_size_selector( FT_Lru      lru,
                             FT_LruNode  node,
                             FT_Pointer  data )
  {
    FT_UNUSED( lru );

    return FT_BOOL( ((FT_Size)node->root.data)->face == (FT_Face)data );
  }


  FT_CALLBACK_DEF( void )
  ftc_manager_done_face( FT_Lru      lru,
                         FT_LruNode  node )
  {
    FTC_Manager  manager = FTC_LRU_GET_MANAGER( lru );
    FT_Face      face    = (FT_Face)node->root.data;


    /* we must begin by removing all sizes for the target face */
    /* from the manager's list                                 */
    FT_Lru_Remove_Selection( manager->sizes_lru,
                             ftc_manager_size_selector,
                             face );

    /* all right, we can discard the face now */
    FT_Done_Face( face );
    node->root.data = 0;
  }


  typedef struct  FTC_FontRequest_
  {
    FT_Face    face;
    FT_UShort  width;
    FT_UShort  height;

  } FTC_FontRequest;


  FT_CALLBACK_DEF( FT_Error )
  ftc_manager_init_size( FT_Lru      lru,
                         FT_LruNode  node )
  {
    FTC_FontRequest*  font_req = (FTC_FontRequest*)node->key;
    FT_Size           size;
    FT_Error          error;
    FT_Face           face = font_req->face;

    FT_UNUSED( lru );


    node->root.data = 0;
    error = FT_New_Size( face, &size );
    if ( !error )
    {
      FT_Activate_Size( size );
      error = FT_Set_Pixel_Sizes( face,
                                  font_req->width,
                                  font_req->height );
      if ( error )
        FT_Done_Size( size );
      else
        node->root.data = size;
    }
    return error;
  }


  FT_CALLBACK_DEF( void )
  ftc_manager_done_size( FT_Lru      lru,
                         FT_LruNode  node )
  {
    FT_UNUSED( lru );

    FT_Done_Size( (FT_Size)node->root.data );
    node->root.data = 0;
  }


  FT_CALLBACK_DEF( FT_Error )
  ftc_manager_flush_size( FT_Lru      lru,
                          FT_LruNode  node,
                          FT_LruKey   key )
  {
    FTC_FontRequest*  req  = (FTC_FontRequest*)key;
    FT_Size           size = (FT_Size)node->root.data;
    FT_Error          error;


    if ( size->face == req->face )
    {
      FT_Activate_Size( size );
      error = FT_Set_Pixel_Sizes( req->face, req->width, req->height );
      if ( error )
        FT_Done_Size( size );
    }
    else
    {
      FT_Done_Size( size );
      node->key = key;
      error = ftc_manager_init_size( lru, node );
    }
    return error;
  }


  FT_CALLBACK_DEF( FT_Bool )
  ftc_manager_compare_size( FT_LruNode  node,
                            FT_LruKey   key )
  {
    FTC_FontRequest*  req  = (FTC_FontRequest*)key;
    FT_Size           size = (FT_Size)node->root.data;

    FT_UNUSED( node );


    return FT_BOOL( size->face           == req->face   &&
                    size->metrics.x_ppem == req->width  &&
                    size->metrics.y_ppem == req->height );
  }


  FT_CALLBACK_TABLE_DEF
  const FT_Lru_Class  ftc_face_lru_class =
  {
    sizeof ( FT_LruRec ),
    ftc_manager_init_face,
    ftc_manager_done_face,
    0,
    0
  };


  FT_CALLBACK_TABLE_DEF
  const FT_Lru_Class  ftc_size_lru_class =
  {
    sizeof ( FT_LruRec ),
    ftc_manager_init_size,
    ftc_manager_done_size,
    ftc_manager_flush_size,
    ftc_manager_compare_size
  };


  /* documentation is in ftcache.h */

  FT_EXPORT_DEF( FT_Error )
  FTC_Manager_New( FT_Library          library,
                   FT_UInt             max_faces,
                   FT_UInt             max_sizes,
                   FT_ULong            max_bytes,
                   FTC_Face_Requester  requester,
                   FT_Pointer          req_data,
                   FTC_Manager        *amanager )
  {
    FT_Error     error;
    FT_Memory    memory;
    FTC_Manager  manager = 0;


    if ( !library )
      return FTC_Err_Invalid_Library_Handle;

    memory = library->memory;

    if ( ALLOC( manager, sizeof ( *manager ) ) )
      goto Exit;

    if ( max_faces == 0 )
      max_faces = FTC_MAX_FACES_DEFAULT;

    if ( max_sizes == 0 )
      max_sizes = FTC_MAX_SIZES_DEFAULT;

    if ( max_bytes == 0 )
      max_bytes = FTC_MAX_BYTES_DEFAULT;

    error = FT_Lru_New( &ftc_face_lru_class,
                        max_faces,
                        manager,
                        memory,
                        1, /* pre_alloc = TRUE */
                        (FT_Lru*)&manager->faces_lru );
    if ( error )
      goto Exit;

    error = FT_Lru_New( &ftc_size_lru_class,
                        max_sizes,
                        manager,
                        memory,
                        1, /* pre_alloc = TRUE */
                        (FT_Lru*)&manager->sizes_lru );
    if ( error )
      goto Exit;

    manager->library      = library;
    manager->max_bytes    = max_bytes;
    manager->request_face = requester;
    manager->request_data = req_data;

    *amanager = manager;

  Exit:
    if ( error && manager )
    {
      FT_Lru_Done( manager->faces_lru );
      FT_Lru_Done( manager->sizes_lru );
      FREE( manager );
    }

    return error;
  }


  /* documentation is in ftcache.h */

  FT_EXPORT_DEF( void )
  FTC_Manager_Done( FTC_Manager  manager )
  {
    FT_Memory  memory;
    FT_UInt    index;


    if ( !manager || !manager->library )
      return;

    memory = manager->library->memory;

    /* now discard all caches */
    for (index = 0; index < FTC_MAX_CACHES; index++ )
    {
      FTC_Cache  cache = manager->caches[index];


      if ( cache )
      {
        cache->clazz->done_cache( cache );
        FREE( cache );
        manager->caches[index] = 0;
      }
    }

    /* discard faces and sizes */
    FT_Lru_Done( manager->faces_lru );
    manager->faces_lru = 0;

    FT_Lru_Done( manager->sizes_lru );
    manager->sizes_lru = 0;

    FREE( manager );
  }


  /* documentation is in ftcache.h */

  FT_EXPORT_DEF( void )
  FTC_Manager_Reset( FTC_Manager  manager )
  {
    if (manager )
    {
      FT_Lru_Reset( manager->sizes_lru );
      FT_Lru_Reset( manager->faces_lru );
    }
    /* XXX: FIXME: flush the caches? */
  }


  /* documentation is in ftcache.h */

  FT_EXPORT_DEF( FT_Error )
  FTC_Manager_Lookup_Face( FTC_Manager  manager,
                           FTC_FaceID   face_id,
                           FT_Face     *aface )
  {
    if ( !manager )
      return FTC_Err_Invalid_Cache_Handle;

    return  FT_Lru_Lookup( manager->faces_lru,
                           (FT_LruKey)face_id,
                           (FT_Pointer*)aface );
  }


  /* documentation is in ftcache.h */

  FT_EXPORT_DEF( FT_Error )
  FTC_Manager_Lookup_Size( FTC_Manager  manager,
                           FTC_Font     font,
                           FT_Face     *aface,
                           FT_Size     *asize )
  {
    FTC_FontRequest  req;
    FT_Error         error;


    /* check for valid `manager' delayed to FTC_Manager_Lookup_Face() */

    if ( aface )
      *aface = 0;

    if ( asize )
      *asize = 0;

    error = FTC_Manager_Lookup_Face( manager, font->face_id, aface );
    if ( !error )
    {
      FT_Size  size;


      req.face   = *aface;
      req.width  = font->pix_width;
      req.height = font->pix_height;

      error = FT_Lru_Lookup( manager->sizes_lru,
                             (FT_LruKey)&req,
                             (FT_Pointer*)&size );
      if ( !error )
      {
        /* select the size as the current one for this face */
        (*aface)->size = size;

        if ( asize )
          *asize = size;
      }
    }

    return error;
  }


  /* `Compress' the manager's data, i.e., get rid of old cache nodes */
  /* that are not referenced anymore in order to limit the total     */
  /* memory used by the cache.                                       */

  /* documentation is in ftcmanag.h */

  FT_EXPORT_DEF( void )
  FTC_Manager_Compress( FTC_Manager  manager )
  {
    FT_ListNode  node;


    node = manager->global_lru.tail;
    while ( manager->num_bytes > manager->max_bytes && node )
    {
      FTC_CacheNode        cache_node = FTC_LIST_TO_CACHENODE( node );
      FTC_CacheNode_Data*  data       = FTC_CACHENODE_TO_DATA_P( cache_node );
      FTC_Cache            cache;
      FT_ListNode          prev       = node->prev;


      if ( data->ref_count <= 0 )
      {
        /* ok, we are going to remove this node */
        FT_List_Remove( &manager->global_lru, node );

        /* finalize cache node */
        cache = manager->caches[data->cache_index];
        if ( cache )
        {
          FTC_CacheNode_Class*  clazz = cache->node_clazz;


          manager->num_bytes -= clazz->size_node( cache_node,
                                                  cache->cache_data );

          clazz->destroy_node( cache_node, cache->cache_data );
        }
        else
        {
          /* this should never happen! */
          FT_ERROR(( "FTC_Manager_Compress: Cache Manager is corrupted!\n" ));
        }

        /* check, just in case of general corruption :-) */
        if ( manager->num_nodes <= 0 )
          FT_ERROR(( "FTC_Manager_Compress: Invalid cache node count!\n" ));
        else
          manager->num_nodes--;
      }
      node = prev;
    }
  }


  FT_EXPORT_DEF( FT_Error )
  FTC_Manager_Register_Cache( FTC_Manager       manager,
                              FTC_Cache_Class*  clazz,
                              FTC_Cache        *acache )
  {
    FT_Error  error = FTC_Err_Invalid_Argument;


    if ( manager && clazz && acache )
    {
      FT_Memory  memory = manager->library->memory;
      FTC_Cache  cache;
      FT_UInt    index = 0;


      /* by default, return 0 */
      *acache = 0;

      /* check for an empty cache slot in the manager's table */
      for ( index = 0; index < FTC_MAX_CACHES; index++ )
      {
        if ( manager->caches[index] == 0 )
          break;
      }

      /* return an error if there are too many registered caches */
      if ( index >= FTC_MAX_CACHES )
      {
        error = FTC_Err_Too_Many_Caches;
        FT_ERROR(( "FTC_Manager_Register_Cache:" ));
        FT_ERROR(( " too many registered caches\n" ));
        goto Exit;
      }

      if ( !ALLOC( cache, clazz->cache_byte_size ) )
      {
        cache->manager = manager;
        cache->memory  = memory;
        cache->clazz   = clazz;

        /* THIS IS VERY IMPORTANT! IT WILL WRETCH THE MANAGER */
        /* IF IT IS NOT SET CORRECTLY                         */
        cache->cache_index = index;

        if ( clazz->init_cache )
          error = clazz->init_cache( cache );

        if ( error )
          FREE( cache );
        else
          manager->caches[index] = *acache = cache;
      }
    }

  Exit:
    return error;
  }


/* END */
