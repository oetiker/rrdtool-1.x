/*
 *  This file is part of RRDtool.
 *
 *  RRDtool is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  RRDtool is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Foobar; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*****************************************************************************
 * RRDtool 1.0.37  Copyright Tobias Oetiker, 1997 - 2000
 *****************************************************************************
 * rrd_tool.h   Common Header File
 *****************************************************************************
 * Id: rrd_tool.h,v 1.1.1.1 2002/02/26 10:21:37 oetiker Exp
 * Log: rrd_tool.h,v
 * Revision 1.1.1.1  2002/02/26 10:21:37  oetiker
 * Intial Import
 *
 *****************************************************************************/
#ifdef  __cplusplus
extern    "C" {
#endif

#ifndef _RRD_EXTRA_H
#define _RRD_EXTRA_H

#include "rrd_format.h"

#ifndef WIN32
#ifndef isnan           /* POSIX */
    int       isnan(
    double value);
#endif
#else                   /* Windows only */
#include <float.h>
#define isnan _isnan
#endif

    void      rrd_free(
    rrd_t *rrd);
    void      rrd_init(
    rrd_t *rrd);

    int       rrd_open(
    char *file_name,
    rrd_t *rrd,
    int rdwr);
    int       readfile(
    char *file,
    char **buffer,
    int skipfirst);

#define RRD_READONLY    0
#define RRD_READWRITE   1

#endif

#ifdef  __cplusplus
}
#endif
