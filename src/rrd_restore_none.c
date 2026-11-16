/*****************************************************************************
 * RRDtool 1.11.0 Copyright by Tobi Oetiker, 1997-2026
 *****************************************************************************
 * rrd_restore_none.c  rrd_restore for builds with --disable-rrd_restore
 *****************************************************************************
 * librrd.sym exports rrd_restore in every build, so a build without the XML
 * import keeps the symbol (and the library's ABI) and reports the missing
 * feature at run time.
 *****************************************************************************/

#include "rrd_tool.h"
#include "unused.h"

int rrd_restore(
    int UNUSED(argc),
    const char **UNUSED(argv))
{
    rrd_set_error
        ("the instance of rrdtool has been compiled without XML import functions");
    return -1;
}
