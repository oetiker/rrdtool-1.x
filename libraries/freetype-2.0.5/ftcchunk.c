/***************************************************************************/
/*                                                                         */
/*  ftcchunk.c                                                             */
/*                                                                         */
/*    FreeType chunk cache cache (body).                                   */
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
#include FT_CACHE_INTERNAL_CHUNK_H
#include FT_LIST_H
#include FT_ERRORS_H
#include FT_INTERNAL_OBJECTS_H

#include "ftcerror.h"


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                      GLYPH NODES                              *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  /* create a new chunk node, setting its cache index and ref count */
  FT_EXPORT_DEF( FT_Error )
  FTC_ChunkNode_Init( FTC_ChunkNode  node,
                      FTC_ChunkSet   cset,
                      FT_UInt        index,
                      FT_Bool        alloc )
  {
    FTC_Chunk_Cache      cache = cset->cache;
    FTC_CacheNode_Data*  data  = FTC_CACHENODE_TO_DATA_P( &node->root );
    FT_Error             error = 0;


    data->cache_index  = (FT_UShort)cache->root.cache_index;
    data->ref_count    = (FT_Short) 0;
    node->cset         = cset;
    node->cset_index   = (FT_UShort)index;
    node->num_elements = (unsigned short)(
                          ( index + 1 < cset->num_chunks )
                            ? cset->element_count
                            : cset->element_max - cset->element_count*index );
    if ( alloc )
    {
      /* allocate elements array */
      FT_Memory   memory;


      memory = cache->root.memory;
      error  = MEM_Alloc( node->elements,
                          cset->element_size * cset->element_count );
    }

    return error;
  }


  FT_EXPORT_DEF( void )
  FTC_ChunkNode_Destroy( FTC_ChunkNode  node )
  {
    FTC_ChunkSet  cset = node->cset;


    /* remove from parent set table */
    cset->chunks[node->cset_index] = 0;

    /* destroy the node */
    cset->clazz->destroy_node( node );
  }


  FT_EXPORT_DEF( FT_ULong )
  FTC_ChunkNode_Size( FTC_ChunkNode  node )
  {
    FTC_ChunkSet  cset = node->cset;


    return cset->clazz->size_node( node );
  }


  FT_CALLBACK_TABLE_DEF
  const FTC_CacheNode_Class  ftc_chunk_cache_node_class =
  {
    (FTC_CacheNode_SizeFunc)   FTC_ChunkNode_Size,
    (FTC_CacheNode_DestroyFunc)FTC_ChunkNode_Destroy
  };


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                      CHUNK SETS                               *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_EXPORT_DEF( FT_Error )
  FTC_ChunkSet_New( FTC_Chunk_Cache  cache,
                    FT_Pointer       type,
                    FTC_ChunkSet    *aset )
  {
    FT_Error      error;
    FT_Memory     memory  = cache->root.memory;
    FTC_Manager   manager = cache->root.manager;
    FTC_ChunkSet  cset    = 0;

    FTC_Chunk_Cache_Class*  ccache_class;
    FTC_ChunkSet_Class*     clazz;


    ccache_class = (FTC_Chunk_Cache_Class*)cache->root.clazz;
    clazz        = ccache_class->cset_class;

    *aset = 0;

    if ( ALLOC( cset, clazz->cset_byte_size ) )
      goto Exit;

    cset->cache   = cache;
    cset->manager = manager;
    cset->memory  = memory;
    cset->clazz   = clazz;

    /* now compute element_max, element_count and element_size */
    error = clazz->sizes( cset, type );
    if ( error )
      goto Exit;

    /* compute maximum number of nodes */
    cset->num_chunks = ( cset->element_max + cset->element_count - 1 ) /
                       cset->element_count;

    /* allocate chunk pointers table */
    if ( ALLOC_ARRAY( cset->chunks, cset->num_chunks, FTC_ChunkNode ) )
      goto Exit;

    /* initialize set by type if needed */
    if ( clazz->init )
    {
      error = clazz->init( cset, type );
      if ( error )
        goto Exit;
    }

    *aset = cset;

  Exit:
    if ( error && cset )
    {
      FREE( cset->chunks );
      FREE( cset );
    }

    return error;
  }


  FT_EXPORT_DEF( void )
  FTC_ChunkSet_Destroy( FTC_ChunkSet  cset )
  {
    FTC_Chunk_Cache      cache        = cset->cache;
    FTC_Manager          manager      = cache->root.manager;
    FT_List              glyphs_lru   = &manager->global_lru;
    FTC_ChunkNode*       bucket       = cset->chunks;
    FTC_ChunkNode*       bucket_limit = bucket + cset->num_chunks;
    FT_Memory            memory       = cache->root.memory;

    FTC_ChunkSet_Class*  clazz        = cset->clazz;


    /* for each bucket, free the list of glyph nodes */
    for ( ; bucket < bucket_limit; bucket++ )
    {
      FTC_ChunkNode  node = bucket[0];
      FT_ListNode    lrunode;


      if ( node )
      {
        lrunode = FTC_CHUNKNODE_TO_LRUNODE( node );

        manager->num_bytes -= clazz->size_node( node );
        manager->num_nodes--;

        FT_List_Remove( glyphs_lru, lrunode );

        clazz->destroy_node( node );

        bucket[0] = 0;
      }
    }

    if ( clazz->done )
      clazz->done( cset );

    FREE( cset->chunks );
    FREE( cset );
  }


  FT_EXPORT_DEF( FT_Error )
  FTC_ChunkSet_Lookup_Node( FTC_ChunkSet    cset,
                            FT_UInt         glyph_index,
                            FTC_ChunkNode  *anode,
                            FT_UInt        *anindex )
  {
    FTC_Chunk_Cache      cache   = cset->cache;
    FTC_Manager          manager = cache->root.manager;
    FT_Error             error   = 0;

    FTC_ChunkSet_Class*  clazz   = cset->clazz;


    *anode = 0;

    if ( glyph_index >= cset->element_max )
      error = FTC_Err_Invalid_Argument;
    else
    {
      FT_UInt         chunk_size  = cset->element_count;
      FT_UInt         chunk_index = glyph_index / chunk_size;
      FTC_ChunkNode*  pnode       = cset->chunks + chunk_index;
      FTC_ChunkNode   node        = *pnode;


      if ( !node )
      {
        /* we didn't found the glyph image; we will now create a new one */
        error = clazz->new_node( cset, chunk_index, &node );
        if ( error )
          goto Exit;

        /* store the new chunk in the cset's table */
        *pnode = node;

        /* insert the node at the start the global LRU glyph list */
        FT_List_Insert( &manager->global_lru,
                        FTC_CHUNKNODE_TO_LRUNODE( node ) );

        manager->num_bytes += clazz->size_node( node );
        manager->num_nodes++;

        if ( manager->num_bytes > manager->max_bytes )
        {
          FTC_ChunkNode_Ref   ( node );
          FTC_Manager_Compress( manager );
          FTC_ChunkNode_Unref ( node );
        }
      }

      *anode   = node;
      *anindex = glyph_index - chunk_index * chunk_size;
    }

  Exit:
    return error;
  }


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                   CHUNK SETS LRU CALLBACKS                    *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


#define FTC_CSET_LRU_GET_CACHE( lru )   \
          ( (FTC_Chunk_Cache)((lru)->user_data) )

#define FTC_CSET_LRU_GET_MANAGER( lru ) \
          FTC_CSET_LRU_GET_CACHE( lru )->manager

#define FTC_LRUNODE_CSET( node )        \
          ( (FTC_ChunkSet)(node)->root.data )


  FT_CALLBACK_DEF( FT_Error )
  ftc_chunk_set_lru_init( FT_Lru      lru,
                          FT_LruNode  node )
  {
    FTC_Chunk_Cache  cache = FTC_CSET_LRU_GET_CACHE( lru );
    FT_Error         error;
    FTC_ChunkSet     cset;


    error = FTC_ChunkSet_New( cache,
                              (FT_Pointer)node->key,
                              &cset );
    if ( !error )
    {
      /* good, now set the set index within the set object */
      cset->cset_index = (FT_UInt)( node - lru->nodes );
      node->root.data  = cset;
    }

    return error;
  }


  FT_CALLBACK_DEF( void )
  ftc_chunk_set_lru_done( FT_Lru      lru,
                          FT_LruNode  node )
  {
    FTC_ChunkSet  cset = FTC_LRUNODE_CSET( node );

    FT_UNUSED( lru );


    FTC_ChunkSet_Destroy( cset );
  }


  FT_CALLBACK_DEF( FT_Bool )
  ftc_chunk_set_lru_compare( FT_LruNode  node,
                             FT_LruKey   key )
  {
    FTC_ChunkSet  cset = FTC_LRUNODE_CSET( node );


    return cset->clazz->compare( cset, (FT_Pointer)key );
  }


  FT_CALLBACK_TABLE_DEF
  const FT_Lru_Class  ftc_chunk_set_lru_class =
  {
    sizeof( FT_LruRec ),
    ftc_chunk_set_lru_init,
    ftc_chunk_set_lru_done,
    0,  /* no flush */
    ftc_chunk_set_lru_compare
  };


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                   CHUNK CACHE OBJECTS                         *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_EXPORT_DEF( FT_Error )
  FTC_Chunk_Cache_Init( FTC_Chunk_Cache  cache )
  {
    FT_Memory  memory = cache->root.memory;
    FT_Error   error;

    FTC_Chunk_Cache_Class*  ccache_clazz;


    /* set up root node_class to be used by manager */
    cache->root.node_clazz =
      (FTC_CacheNode_Class*)&ftc_chunk_cache_node_class;

    /* setup `compare' shortcut */
    ccache_clazz   = (FTC_Chunk_Cache_Class*)cache->root.clazz;
    cache->compare = ccache_clazz->cset_class->compare;

    error = FT_Lru_New( &ftc_chunk_set_lru_class,
                        FTC_MAX_CHUNK_SETS,
                        cache,
                        memory,
                        1, /* pre_alloc == TRUE */
                        &cache->csets_lru );
    return error;
  }


  FT_EXPORT_DEF( void )
  FTC_Chunk_Cache_Done( FTC_Chunk_Cache  cache )
  {
    /* discard glyph sets */
    FT_Lru_Done( cache->csets_lru );
  }


  FT_EXPORT_DEF( FT_Error )
  FTC_Chunk_Cache_Lookup( FTC_Chunk_Cache  cache,
                          FT_Pointer       type,
                          FT_UInt          gindex,
                          FTC_ChunkNode   *anode,
                          FT_UInt         *aindex )
  {
    FT_Error       error;
    FTC_ChunkSet   cset;
    FTC_ChunkNode  node;
    FT_UInt        cindex;
    FTC_Manager    manager;


    /* check for valid `desc' delayed to FT_Lru_Lookup() */

    if ( !cache || !anode || !aindex )
      return FTC_Err_Invalid_Argument;

    *anode  = 0;
    *aindex = 0;
    cset    = cache->last_cset;

    if ( !cset || !cache->compare( cset, type ) )
    {
      error = FT_Lru_Lookup( cache->csets_lru,
                             (FT_LruKey)type,
                             (FT_Pointer*)&cset );
      cache->last_cset = cset;
      if ( error )
        goto Exit;
    }

    error = FTC_ChunkSet_Lookup_Node( cset, gindex, &node, &cindex );
    if ( error )
      goto Exit;

    /* now compress the manager's cache pool if needed */
    manager = cache->root.manager;
    if ( manager->num_bytes > manager->max_bytes )
    {
      FTC_ChunkNode_Ref   ( node );
      FTC_Manager_Compress( manager );
      FTC_ChunkNode_Unref ( node );
    }

    *anode  = node;
    *aindex = cindex;

  Exit:
    return error;
  }


/* END */
