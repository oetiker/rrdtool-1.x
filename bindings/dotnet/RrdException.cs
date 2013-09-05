using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace dnrrdlib
{
    public class RrdException : Exception
    {
        public RrdException()
            : base(rrd.Get_Error())
        {
            rrd.rrd_clear_error();
        }
    }
}
