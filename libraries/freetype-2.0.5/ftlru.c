/***************************************************************************/
/*                                                                         */
/*  ftlru.c                                                                */
/*                                                                         */
/*    Simple LRU list-cache (body).                                        */
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
#include FT_CACHE_INTERNAL_LRU_H
#include FT_LIST_H
#include FT_INTERNAL_OBJECTS_H

#include "ftcerror.h"


  static void
  lru_build_free_list( FT_LruNode  nodes,
                       FT_UInt     count,
                       FT_List     free_list )
  {
    FT_LruNode  node  = nodes;
    FT_LruNode  limit = node + count;


    free_list->head = free_list->tail = 0;
    for ( ; node < limit; node++ )
      FT_List_Add( free_list, (FT_ListNode)node );
  }


  FT_EXPORT_DEF( FT_Error )
  FT_Lru_New( const FT_Lru_Class*  clazz,
              FT_UInt              max_elements,
              FT_Pointer           user_data,
              FT_Memory            memory,
              FT_Bool              pre_alloc,
              FT_Lru              *anlru )
  {
    FT_Error  error;
    FT_Lru    lru;


    if ( !anlru )
      return FTC_Err_Invalid_Argument;

    *anlru = 0;
    if ( !ALLOC( lru, sizeof ( *lru ) ) )
    {
      if ( pre_alloc )
      {
        /* allocate static array of lru list nodes */
        if ( ALLOC_ARRAY( lru->nodes, max_elements, FT_LruNodeRec ) )
        {
          FREE( lru );
          goto Exit;
        }

        /* build the `free_nodes' list from the array */
        lru_build_free_list( lru->nodes, max_elements, &lru->free_nodes );
      }

      /* initialize common fields */
      lru->clazz        = (FT_Lru_Class*)clazz;
      lru->max_elements = max_elements;
      lru->memory       = memory;
      lru->user_data    = user_data;

      *anlru = lru;
    }

  Exit:
    return error;
  }


  FT_EXPORT_DEF( void )
  FT_Lru_Reset( FT_Lru  lru )
  {
    FT_ListNode    node;
    FT_Lru_Class*  clazz;
    FT_Memory      memory;


    if ( !lru )
      return;

    node   = lru->elements.head;
    clazz  = lru->clazz;
    memory = lru->memory;

    while ( node )
    {
      FT_ListNode  next = node->next;


      clazz->done_element( lru, (FT_LruNode)node );
      if ( !lru->nodes )
        FREE( node );

      node = next;
    }

    /* rebuild free list if necessary */
    if ( lru->nodes )
      lru_build_free_list( lru->nodes, lru->max_elements, &lru->free_nodes );

    lru->elements.head = lru->elements.tail = 0;
    lru->num_elements  = 0;
  }


  FT_EXPORT_DEF( void )
  FT_Lru_Done( FT_Lru  lru )
  {
    FT_Memory  memory;


    if ( !lru )
      return;

    memory = lru->memory;

    FT_Lru_Reset( lru );

    FREE( lru->nodes );
    FREE( lru );
  }


  FT_EXPORT_DEF( FT_Error )
  FT_Lru_Lookup_Node( FT_Lru       lru,
                      FT_LruKey    key,
                      FT_LruNode  *anode )
  {
    FT_Error       error = 0;
    FT_ListNode    node;
    FT_Lru_Class*  clazz;
    FT_LruNode     found = 0;
    FT_Memory      memory;


    if ( !lru || !key || !anode )
      return FTC_Err_Invalid_Argument;

    node   = lru->elements.head;
    clazz  = lru->clazz;
    memory = lru->memory;

    if ( clazz->compare_element )
    {
      for ( ; node; node = node->next )
        if ( clazz->compare_element( (FT_LruNode)node, key ) )
        {
          found = (FT_LruNode)node;
          break;
        }
    }
    else
    {
      for ( ; node; node = node->next )
        if ( ((FT_LruNode)node)->key == key )
        {
          found = (FT_LruNode)node;
          break;
        }
    }

    if ( found )
    {
      /* move element to top of list */
      FT_List_Up( &lru->elements, node );
    }
    else
    {
      /* we haven't found the relevant element.  We will now try */
      /* to create a new one.                                    */
      if ( lru->num_elements >= lru->max_elements )
      {
        /* this lru list is full; we will now flush */
        /* the oldest node                          */
        FT_LruNode  lru_node;


        node     = lru->elements.tail;
        lru_node = (FT_LruNode)node;
        found    = lru_node;

        if ( clazz->flush_element )
          error = clazz->flush_element( lru, lru_node, key );
        else
        {
          clazz->done_element( lru, lru_node );
          lru_node->key = key;
          node->data    = 0;
          error = clazz->init_element( lru, lru_node );
        }

        if ( !error )
        {
          /* now, move element to top of list */
          FT_List_Up( &lru->elements, node );
        }
        else
        {
          /* in case of error, the node must be discarded */
          FT_List_Remove( &lru->elements, node );
          lru->num_elements--;

          if ( lru->nodes )
            FT_List_Insert( &lru->free_nodes, node );
          else
            FREE( lru_node );

          found = 0;
        }
      }
      else
      {
        FT_LruNode  lru_node;


        /* create a new lru list node, then the element for it */
        if ( lru->nodes )
        {
          node          = lru->free_nodes.head;
          lru_node      = (FT_LruNode)node;
          lru_node->key = key;

          error = clazz->init_element( lru, lru_node );
          if ( error )
            goto Exit;

          FT_List_Remove( &lru->free_nodes, node );
        }
        else
        {
          if ( ALLOC( lru_node, sizeof ( *lru_node ) ) )
            goto Exit;

          lru_node->key = key;
          error = clazz->init_element( lru, lru_node );
          if ( error )
          {
            FREE( lru_node );
            goto Exit;
          }
        }

        found = lru_node;
        node  = (FT_ListNode)lru_node;
        FT_List_Insert( &lru->elements, node );
        lru->num_elements++;
      }
    }

  Exit:
    *anode = found;
    return error;
  }


  FT_EXPORT_DEF( FT_Error )
  FT_Lru_Lookup( FT_Lru       lru,
                 FT_LruKey    key,
                 FT_Pointer  *anobject )
  {
    FT_Error    error;
    FT_LruNode  node;


    /* check for valid `lru' and `key' delayed to FT_Lru_Lookup_Node() */

    if ( !anobject )
      return FTC_Err_Invalid_Argument;

    *anobject = 0;
    error = FT_Lru_Lookup_Node( lru, key, &node );
    if ( !error )
      *anobject = node->root.data;

    return error;
  }


  FT_EXPORT_DEF( void )
  FT_Lru_Remove_Node( FT_Lru      lru,
                      FT_LruNode  node )
  {
    if ( !lru || !node )
      return;

    if ( lru->num_elements > 0 )
    {
      FT_List_Remove( &lru->elements, (FT_ListNode)node );
      lru->clazz->done_element( lru, node );

      if ( lru->nodes )
        FT_List_Insert( &lru->free_nodes, (FT_ListNode)node );
      else
      {
        FT_Memory  memory = lru->memory;


        FREE( node );
      }

      lru->num_elements--;
    }
  }


  FT_EXPORT_DEF( void )
  FT_Lru_Remove_Selection( FT_Lru           lru,
                           FT_Lru_Selector  selector,
                           FT_Pointer       data )
  {
    if ( !lru || !selector )
      return;

    if ( lru->num_elements > 0 )
    {
      FT_ListNode  node = lru->elements.head;
      FT_ListNode  next;


      while ( node )
      {
        next = node->next;
        if ( selector( lru, (FT_LruNode)node, data ) )
        {
          /* remove this element from the list, and destroy it */
          FT_Lru_Remove_Node( lru, (FT_LruNode)node );
        }
        node = next;
      }
    }
  }


/* END */
