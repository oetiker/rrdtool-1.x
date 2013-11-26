/*****************************************************************************
 * RRDtool 1.4.3  Copyright by Tobi Oetiker, 1997-2010
 * This file:     Copyright 2003 Peter Stamfest <peter@stamfest.at> 
 *                             & Tobias Oetiker
 * Distributed under the GPL
 *****************************************************************************
 * rrd_is_thread_safe.c   Poisons some nasty function calls using GNU cpp
 *****************************************************************************
 * $Id$
 *************************************************************************** */

#ifndef RRD_IS_THREAD_SAFE_H_F7EEC7F1E9844C6DB63692B2673768F2
#define RRD_IS_THREAD_SAFE_H_F7EEC7F1E9844C6DB63692B2673768F2

#ifdef  __cplusplus
extern    "C" {
#endif

#undef strerror

#if( 2 < __GNUC__ )
#pragma GCC poison strtok asctime ctime gmtime localtime tmpnam strerror
#endif

#ifdef  __cplusplus
}
#endif
#endif /*_RRD_IS_THREAD_SAFE_H */
