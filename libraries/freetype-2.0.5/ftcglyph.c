/***************************************************************************/
/*                                                                         */
/*  ftcglyph.c                                                             */
/*                                                                         */
/*    FreeType Glyph Image (FT_Glyph) cache (body).                        */
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
#include FT_CACHE_INTERNAL_GLYPH_H
#include FT_ERRORS_H
#include FT_LIST_H
#include FT_INTERNAL_OBJECTS_H
#include FT_INTERNAL_DEBUG_H

#include "ftcerror.h"


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                      GLYPH NODES                              *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  /* create a new glyph node, setting its cache index and ref count */
  FT_EXPORT_DEF( void )
  FTC_GlyphNode_Init( FTC_GlyphNode  node,
                      FTC_GlyphSet   gset,
                      FT_UInt        gindex )
  {
    FTC_Glyph_Cache      cache = gset->cache;
    FTC_CacheNode_Data*  data  = FTC_CACHENODE_TO_DATA_P( &node->root );


    data->cache_index = (FT_UShort)cache->root.cache_index;
    data->ref_count   = (FT_Short) 0;
    node->gset_index  = (FT_UShort)gset->gset_index;
    node->glyph_index = (FT_UShort)gindex;
  }


  /* Important: This function is called from the cache manager to */
  /* destroy a given cache node during `cache compression'.  The  */
  /* second argument is always `cache.cache_data'.  Thus be       */
  /* certain that the function FTC_Glyph_Cache_New() does indeed  */
  /* set its `cache_data' field correctly, otherwise bad things   */
  /* will happen!                                                 */

  FT_EXPORT_DEF( void )
  FTC_GlyphNode_Destroy( FTC_GlyphNode    node,
                         FTC_Glyph_Cache  cache )
  {
    FT_LruNode    gset_lru = cache->gsets_lru->nodes + node->gset_index;
    FTC_GlyphSet  gset     = (FTC_GlyphSet)gset_lru->root.data;
    FT_UInt       hash     = node->glyph_index % gset->hash_size;


    /* remove the node from its gset's bucket list */
    {
      FTC_GlyphNode*  pnode = gset->buckets + hash;
      FTC_GlyphNode   cur;


      for (;;)
      {
        cur = *pnode;
        if ( !cur )
        {
          /* this should never happen */
          FT_ERROR(( "FTC_GlyphNode_Destroy:"
                     " trying to delete an unlisted node!" ));
          return;
        }

        if ( cur == node )
        {
          *pnode = cur->gset_next;
          break;
        }
        pnode = &cur->gset_next;
      }
    }

    /* destroy the node */
    gset->clazz->destroy_node( node, gset );
  }


  /* Important: This function is called from the cache manager to */
  /* size a given cache node during `cache compression'.  The     */
  /* second argument is always `cache.user_data'.  Thus be        */
  /* certain that the function FTC_Glyph_Cache_New() does indeed  */
  /* set its `user_data' field correctly, otherwise bad things    */
  /* will happen!                                                 */

  FT_EXPORT_DEF( FT_ULong )
  FTC_GlyphNode_Size( FTC_GlyphNode    node,
                      FTC_Glyph_Cache  cache )
  {
    FT_LruNode    gset_lru = cache->gsets_lru->nodes + node->gset_index;
    FTC_GlyphSet  gset     = (FTC_GlyphSet)gset_lru->root.data;


    return gset->clazz->size_node( node, gset );
  }


  FT_CALLBACK_TABLE_DEF
  const FTC_CacheNode_Class  ftc_glyph_cache_node_class =
  {
    (FTC_CacheNode_SizeFunc)   FTC_GlyphNode_Size,
    (FTC_CacheNode_DestroyFunc)FTC_GlyphNode_Destroy
  };


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                      GLYPH SETS                               *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_EXPORT_DEF( FT_Error )
  FTC_GlyphSet_New( FTC_Glyph_Cache  cache,
                    FT_Pointer       type,
                    FTC_GlyphSet    *aset )
  {
    FT_Error                error;
    FT_Memory               memory  = cache->root.memory;
    FTC_Manager             manager = cache->root.manager;
    FTC_GlyphSet            gset    = 0;

    FTC_Glyph_Cache_Class*  gcache_class;
    FTC_GlyphSet_Class*     clazz;


    gcache_class = (FTC_Glyph_Cache_Class*)cache->root.clazz;
    clazz        = gcache_class->gset_class;

    *aset = 0;

    if ( ALLOC( gset, clazz->gset_byte_size ) )
      goto Exit;

    gset->cache     = cache;
    gset->manager   = manager;
    gset->memory    = memory;
    gset->hash_size = FTC_GSET_HASH_SIZE_DEFAULT;
    gset->clazz     = clazz;

    /* allocate buckets table */
    if ( ALLOC_ARRAY( gset->buckets, gset->hash_size, FTC_GlyphNode ) )
      goto Exit;

    /* initialize gset by type if needed */
    if ( clazz->init )
    {
      error = clazz->init( gset, type );
      if ( error )
        goto Exit;
    }

    *aset = gset;

  Exit:
    if ( error && gset )
    {
      FREE( gset->buckets );
      FREE( gset );
    }

    return error;
  }


  FT_EXPORT_DEF( void )
  FTC_GlyphSet_Destroy( FTC_GlyphSet  gset )
  {
    FTC_Glyph_Cache      cache        = gset->cache;
    FTC_Manager          manager      = cache->root.manager;
    FT_List              glyphs_lru   = &manager->global_lru;
    FTC_GlyphNode*       bucket       = gset->buckets;
    FTC_GlyphNode*       bucket_limit = bucket + gset->hash_size;
    FT_Memory            memory       = cache->root.memory;

    FTC_GlyphSet_Class*  clazz        = gset->clazz;


    /* for each bucket, free the list of glyph nodes */
    for ( ; bucket < bucket_limit; bucket++ )
    {
      FTC_GlyphNode   node = bucket[0];
      FTC_GlyphNode   next = 0;
      FT_ListNode     lrunode;


      for ( ; node; node = next )
      {
        next    = node->gset_next;
        lrunode = FTC_GLYPHNODE_TO_LRUNODE( node );

        manager->num_bytes -= clazz->size_node( node, gset );
        manager->num_nodes--;

        FT_List_Remove( glyphs_lru, lrunode );

        clazz->destroy_node( node, gset );
      }

      bucket[0] = 0;
    }

    if ( clazz->done )
      clazz->done( gset );

    FREE( gset->buckets );
    FREE( gset );
  }


  FT_EXPORT_DEF( FT_Error )
  FTC_GlyphSet_Lookup_Node( FTC_GlyphSet    gset,
                            FT_UInt         glyph_index,
                            FTC_GlyphNode  *anode )
  {
    FTC_Glyph_Cache      cache      = gset->cache;
    FTC_Manager          manager    = cache->root.manager;
    FT_UInt              hash_index = glyph_index % gset->hash_size;
    FTC_GlyphNode*       bucket     = gset->buckets + hash_index;
    FTC_GlyphNode*       pnode      = bucket;
    FTC_GlyphNode        node;
    FT_Error             error;

    FTC_GlyphSet_Class*  clazz      = gset->clazz;


    *anode = 0;

    for ( ;; )
    {
      node = *pnode;
      if ( !node )
        break;

      if ( (FT_UInt)node->glyph_index == glyph_index )
      {
        /* we found it! -- move glyph to start of the lists */
        *pnode          = node->gset_next;
        node->gset_next = bucket[0];
        bucket[0]       = node;

        FT_List_Up( &manager->global_lru, FTC_GLYPHNODE_TO_LRUNODE( node ) );
        *anode = node;
        return 0;
      }
      /* go to next node in bucket */
      pnode = &node->gset_next;
    }

    /* we didn't found the glyph image, we will now create a new one */
    error = clazz->new_node( gset, glyph_index, &node );
    if ( error )
      goto Exit;

    /* insert the node at the start of our bucket list */
    node->gset_next = bucket[0];
    bucket[0]       = node;

    /* insert the node at the start the global LRU glyph list */
    FT_List_Insert( &manager->global_lru, FTC_GLYPHNODE_TO_LRUNODE( node ) );

    manager->num_bytes += clazz->size_node( node, gset );
    manager->num_nodes++;

    if ( manager->num_bytes > manager->max_bytes )
    {
      FTC_GlyphNode_Ref   ( node );
      FTC_Manager_Compress( manager );
      FTC_GlyphNode_Unref ( node );
    }

    *anode = node;

  Exit:
    return error;
  }


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                   GLYPH SETS LRU CALLBACKS                    *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


#define FTC_GSET_LRU_GET_CACHE( lru )   \
          ( (FTC_Glyph_Cache)(lru)->user_data )

#define FTC_GSET_LRU_GET_MANAGER( lru ) \
          FTC_GSET_LRU_GET_CACHE( lru )->manager

#define FTC_LRUNODE_GSET( node )        \
          ( (FTC_GlyphSet)(node)->root.data )


  FT_CALLBACK_DEF( FT_Error )
  ftc_glyph_set_lru_init( FT_Lru      lru,
                          FT_LruNode  node )
  {
    FTC_Glyph_Cache  cache = FTC_GSET_LRU_GET_CACHE( lru );
    FT_Error         error;
    FTC_GlyphSet     gset;


    error = FTC_GlyphSet_New( cache, (FT_Pointer)node->key, &gset );
    if ( !error )
    {
      /* good, now set the gset index within the gset object */
      gset->gset_index = (FT_UInt)( node - lru->nodes );
      node->root.data  = gset;
    }

    return error;
  }


  FT_CALLBACK_DEF( void )
  ftc_glyph_set_lru_done( FT_Lru      lru,
                          FT_LruNode  node )
  {
    FTC_GlyphSet  gset = FTC_LRUNODE_GSET( node );

    FT_UNUSED( lru );


    FTC_GlyphSet_Destroy( gset );
  }


  FT_CALLBACK_DEF( FT_Bool )
  ftc_glyph_set_lru_compare( FT_LruNode  node,
                             FT_LruKey   key )
  {
    FTC_GlyphSet  gset = FTC_LRUNODE_GSET( node );


    return gset->clazz->compare( gset, (FT_Pointer)key );
  }


  FT_CALLBACK_TABLE_DEF
  const FT_Lru_Class  ftc_glyph_set_lru_class =
  {
    sizeof( FT_LruRec ),
    ftc_glyph_set_lru_init,
    ftc_glyph_set_lru_done,
    0,  /* no flush */
    ftc_glyph_set_lru_compare
  };


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                      GLYPH CACHE OBJECTS                      *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_EXPORT_DEF( FT_Error )
  FTC_Glyph_Cache_Init( FTC_Glyph_Cache  cache )
  {
    FT_Memory  memory = cache->root.memory;
    FT_Error   error;

    FTC_Glyph_Cache_Class*  gcache_clazz;


    /* set up root node_class to be used by manager */
    cache->root.node_clazz =
      (FTC_CacheNode_Class*)&ftc_glyph_cache_node_class;

    /* setup the `compare' shortcut */
    gcache_clazz   = (FTC_Glyph_Cache_Class*)cache->root.clazz;
    cache->compare = gcache_clazz->gset_class->compare;

    /* The following is extremely important for ftc_destroy_glyph_image() */
    /* to work properly, as the second parameter that is sent to it       */
    /* through the cache manager is `cache_data' and must be set to       */
    /* `cache' here.                                                      */
    /*                                                                    */
    cache->root.cache_data = cache;

    error = FT_Lru_New( &ftc_glyph_set_lru_class,
                        FTC_MAX_GLYPH_SETS,
                        cache,
                        memory,
                        1, /* pre_alloc == TRUE */
                        &cache->gsets_lru );
    return error;
  }


  FT_EXPORT_DEF( void )
  FTC_Glyph_Cache_Done( FTC_Glyph_Cache  cache )
  {
    /* discard glyph sets */
    FT_Lru_Done( cache->gsets_lru );
  }


  FT_EXPORT_DEF( FT_Error )
  FTC_Glyph_Cache_Lookup( FTC_Glyph_Cache  cache,
                          FT_Pointer       type,
                          FT_UInt          gindex,
                          FTC_GlyphNode   *anode )
  {
    FT_Error       error;
    FTC_GlyphSet   gset;
    FTC_GlyphNode  node;
    FTC_Manager    manager;


    /* check for valid `desc' delayed to FT_Lru_Lookup() */

    if ( !cache || !anode )
      return FTC_Err_Invalid_Argument;

    *anode = 0;
    gset   = cache->last_gset;

    if ( !gset || !cache->compare( gset, type ) )
    {
      error = FT_Lru_Lookup( cache->gsets_lru,
                             (FT_LruKey)type,
                             (FT_Pointer*)&gset );
      cache->last_gset = gset;
      if ( error )
        goto Exit;
    }

    error = FTC_GlyphSet_Lookup_Node( gset, gindex, &node );
    if ( error )
      goto Exit;

    /* now compress the manager's cache pool if needed */
    manager = cache->root.manager;
    if ( manager->num_bytes > manager->max_bytes )
    {
      FTC_GlyphNode_Ref   ( node );
      FTC_Manager_Compress( manager );
      FTC_GlyphNode_Unref ( node );
    }

    *anode = node;

  Exit:
    return error;
  }


/* END */
