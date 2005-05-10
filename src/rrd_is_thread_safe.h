/*****************************************************************************
 * RRDtool 1.2.6  Copyright by Tobi Oetiker, 1997-2005
 * This file:     Copyright 2003 Peter Stamfest <peter@stamfest.at> 
 *                             & Tobias Oetiker
 * Distributed under the GPL
 *****************************************************************************
 * rrd_is_thread_safe.c   Poisons some nasty function calls using GNU cpp
 *****************************************************************************
 * $Id$
 *************************************************************************** */

#ifndef _RRD_IS_THREAD_SAFE_H
#define _RRD_IS_THREAD_SAFE_H

#ifdef  __cplusplus
extern "C" {
#endif

#undef strerror
#pragma GCC poison strtok asctime ctime gmtime localtime tmpnam strerror

#ifdef  __cplusplus
}
#endif

#endif /*_RRD_IS_THREAD_SAFE_H */
