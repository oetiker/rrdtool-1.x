/*
 * python-rrdtool, Python bindings for rrdtool.
 * Based on the rrdtool Python bindings for Python 2 from
 * Hye-Shik Chang <perky@fallin.lv>.
 *
 * Copyright 2012 Christian Jurk <commx@commx.ws>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <Python.h>
#include <datetime.h>
#include "rrd_config.h"
#include "rrd_tool.h"

/* Some macros to maintain compatibility between Python 2.x and 3.x */
#if PY_MAJOR_VERSION >= 3
#define HAVE_PY3K
#define PyRRD_String_Check(x)                 PyUnicode_Check(x)
#define PyRRD_String_FromString(x)            PyUnicode_FromString(x)
#define PyRRD_String_AS_STRING(x)             PyUnicode_AsUTF8(x)
#define PyRRD_String_FromStringAndSize(x, y)  PyBytes_FromStringAndSize(x, y)
#define PyRRD_String_Size(x)                  PyUnicode_Size(x)
#define PyRRD_Int_FromLong(x)                 PyLong_FromLong(x)
#define PyRRD_Int_FromString(x, y, z)         PyLong_FromString(x,y,z)
#define PyRRD_Long_Check(x)                   PyLong_Check(x)
#else
#define PyRRD_String_Check(x)                 PyString_Check(x)
#define PyRRD_String_FromString(x)            PyString_FromString(x)
#define PyRRD_String_AS_STRING(x)             PyString_AS_STRING(x)
#define PyRRD_String_FromStringAndSize(x, y)  PyString_FromStringAndSize(x, y)
#define PyRRD_String_Size(x)                  PyString_Size(x)
#define PyRRD_Int_FromLong(x)                 PyInt_FromLong(x)
#define PyRRD_Int_FromString(x, y, z)         PyInt_FromString(x,y,z)
#define PyRRD_Long_Check(x)                   (PyInt_Check(x) || PyLong_Check(x))
#endif

#ifndef Py_UNUSED
#ifdef __GNUC__
 #define Py_UNUSED(name) _unused_ ## name __attribute__((unused))
#else
 #define Py_UNUSED(name) _unused_ ## -name
#endif
#endif

/** Binding version. */
static const char *_version = "0.1.10";

/** Exception types. */
static PyObject *rrdtool_OperationalError;
static PyObject *rrdtool_ProgrammingError;

static char **rrdtool_argv = NULL;
static int    rrdtool_argc = 0;

/**
 * PyRRD_DateTime_FromTS: convert UNIX timestamp (time_t)
 * to Python datetime object.
 *
 * @param ts UNIX timestamp (time_t)
 * @return Pointer to new PyObject (New Reference)
 */
static PyObject *
PyRRD_DateTime_FromTS(time_t ts)
{
    PyObject *ret;
    struct tm lt;

    localtime_r(&ts, &lt);

    ret = PyDateTime_FromDateAndTime(
        lt.tm_year + 1900,
        lt.tm_mon + 1,
        lt.tm_mday,
        lt.tm_hour,
        lt.tm_min,
        lt.tm_sec,
        0);

    return ret;
}

/**
 * PyRRD_String_FromCF: get string representation of CF enum index
 *
 * @param cf enum cf_en
 * @return Null-terminated string
 */
const char *
PyRRD_String_FromCF(enum cf_en cf)
{
    switch (cf) {
        case CF_AVERAGE:
            return "AVERAGE";
        case CF_MINIMUM:
            return "MIN";
        case CF_MAXIMUM:
            return "MAX";
        case CF_LAST:
            return "LAST";
        default:
            return "INVALID";
    }
}

/**
 * Helper function to convert Python objects into a representation that the
 * rrdtool functions can work with.
 *
 * @param command RRDtool command name
 * @param args Command arguments
 * @return Zero if the function succeeds, otherwise -1
 */
static int
convert_args(char *command, PyObject *args)
{
    PyObject *o, *lo;
    int i, j, args_count, argv_count, element_count;

    argv_count = element_count = 0;
    args_count = PyTuple_Size(args);

    for (i = 0; i < args_count; i++) {
        o = PyTuple_GET_ITEM(args, i);

        if (PyRRD_String_Check(o))
            element_count++;
        else if (PyList_CheckExact(o))
            element_count += PyList_Size(o);
        else {
            PyErr_Format(PyExc_TypeError,
                         "Argument %d must be str or a list of str", i);
            return -1;
        }
    }

    rrdtool_argv = PyMem_New(char *, element_count + 1);

    if (rrdtool_argv == NULL)
        return -1;

    for (i = 0; i < args_count; i++) {
        o = PyTuple_GET_ITEM(args, i);

        if (PyRRD_String_Check(o))
            rrdtool_argv[++argv_count] = PyRRD_String_AS_STRING(o);
        else if (PyList_CheckExact(o)) {
            for (j = 0; j < PyList_Size(o); j++) {
                lo = PyList_GetItem(o, j);

                if (PyRRD_String_Check(lo))
                    rrdtool_argv[++argv_count] = PyRRD_String_AS_STRING(lo);
                else {
                    PyMem_Del(rrdtool_argv);
                    PyErr_Format(PyExc_TypeError,
                      "Element %d in argument %d must be str", j, i);
                    return -1;
                }
            }
        } else {
            PyMem_Del(rrdtool_argv);
            PyErr_Format(rrdtool_ProgrammingError,
              "Argument %d must be str or list of str", i);
            return -1;
        }
    }

    rrdtool_argv[0] = command;
    rrdtool_argc = element_count + 1;

    return 0;
}

/**
 * Destroy argument vector.
 */
static void
destroy_args(void)
{
    PyMem_Del(rrdtool_argv);
    rrdtool_argv = NULL;
}

/**
 * Convert RRDtool info to dict.
 *
 * @param data RRDtool info object
 * @return Python dict object
 */
static PyObject *
_rrdtool_util_info2dict(const rrd_info_t *data)
{
    PyObject *dict, *val;

    dict = PyDict_New();

    while (data) {
        val = NULL;

        switch (data->type) {
            case RD_I_VAL:
                if (isnan(data->value.u_val)) {
                    Py_INCREF(Py_None);
                    val = Py_None;
                } else
                    PyFloat_FromDouble(data->value.u_val);
                break;

            case RD_I_CNT:
                val = PyLong_FromUnsignedLong(data->value.u_cnt);
                break;

            case RD_I_INT:
                val = PyLong_FromLong(data->value.u_int);
                break;

            case RD_I_STR:
                val = PyRRD_String_FromString(data->value.u_str);
                break;

            case RD_I_BLO:
                val = PyRRD_String_FromStringAndSize(
                    (char *)data->value.u_blo.ptr,
                    data->value.u_blo.size);
                break;
            default:
                break;
        }

        if (val != NULL) {
            PyDict_SetItemString(dict, data->key, val);
            Py_DECREF(val);
        }

        data = data->next;
    }

    return dict;
}

static char _rrdtool_create__doc__[] = "Create a new Round Robin Database.\n\n\
  Usage: create(args...)\n\
  Arguments:\n\n\
    filename\n\
    [-b|--start start time]\n\
    [-s|--step step]\n\
    [-t|--template template-file]\n\
    [-r|--source source-file]\n\
    [-O|--no-overwrite]\n\
    [-d|--daemon address]\n\
    [DS:ds-name[=mapped-ds-name[source-index]]:DST:heartbeat:min:max]\n\
    [RRA:CF:xff:steps:rows]\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdcreate.en.html";

static PyObject *
_rrdtool_create(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    int status;

    if (convert_args("create", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    status = rrd_create(rrdtool_argc, rrdtool_argv);
    Py_END_ALLOW_THREADS

    if (status == -1) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        Py_INCREF(Py_None);
        ret = Py_None;
    }

    destroy_args();
    return ret;
}

static char _rrdtool_dump__doc__[] = "Dump an RRD to XML.\n\n\
  Usage: dump(args..)\n\
  Arguments:\n\n\
    [-h|--header {none,xsd,dtd}\n\
    [-n|--no-header]\n\
    [-d|--daemon address]\n\
    file.rrd\n\
    [file.xml]\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrddump.en.html";

static PyObject *
_rrdtool_dump(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    int status;

    if (convert_args("dump", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    status = rrd_dump(rrdtool_argc, rrdtool_argv);
    Py_END_ALLOW_THREADS

    if (status != 0) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        Py_INCREF(Py_None);
        ret = Py_None;
    }

    destroy_args();
    return ret;
}

static char _rrdtool_update__doc__[] = "Store a new set of values into\
 the RRD.\n\n\
 Usage: update(args..)\n\
 Arguments:\n\n\
   filename\n\
   [--template|-t ds-name[:ds-name]...]\n\
   N|timestamp:value[:value...]\n\
   [timestamp:value[:value...] ...]\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdupdate.en.html";

static PyObject *
_rrdtool_update(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    int status;

    if (convert_args("update", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    status = rrd_update(rrdtool_argc, rrdtool_argv);
    Py_END_ALLOW_THREADS

    if (status == -1) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        Py_INCREF(Py_None);
        ret = Py_None;
    }

    destroy_args();
    return ret;
}

static char _rrdtool_updatev__doc__[] = "Store a new set of values into "\
  "the Round Robin Database and return an info dictionary.\n\n\
  This function works in the same manner as 'update', but will return an\n\
  info dictionary instead of None.";

static PyObject *
_rrdtool_updatev(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    rrd_info_t *data;

    if (convert_args("updatev", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    data = rrd_update_v(rrdtool_argc, rrdtool_argv);
    Py_END_ALLOW_THREADS

    if (data == NULL) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        ret = _rrdtool_util_info2dict(data);
        rrd_info_free(data);
    }

    destroy_args();
    return ret;
}

static char _rrdtool_fetch__doc__[] = "Fetch data from an RRD.\n\n\
  Usage: fetch(args..)\n\
  Arguments:\n\n\
    filename\n\
    CF\n\
    [-r|--resolution resolution]\n\
    [-s|--start start]\n\
    [-e|--end end]\n\
    [-a|--align-start]\n\
    [-d|--daemon address]\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdfetch.en.html";

static PyObject *
_rrdtool_fetch(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret, *range_tup, *dsnam_tup, *data_list, *t;
    rrd_value_t *data, *datai, dv;
    unsigned long step, ds_cnt, i, j, row;
    time_t start, end;
    char **ds_namv;
    int status;

    if (convert_args("fetch", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    status = rrd_fetch(rrdtool_argc, rrdtool_argv, &start, &end, &step,
                       &ds_cnt, &ds_namv, &data);
    Py_END_ALLOW_THREADS

    if (status == -1) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        row = (end - start) / step;
        ret = PyTuple_New(3);
        range_tup = PyTuple_New(3);
        dsnam_tup = PyTuple_New(ds_cnt);
        data_list = PyList_New(row);

        PyTuple_SET_ITEM(ret, 0, range_tup);
        PyTuple_SET_ITEM(ret, 1, dsnam_tup);
        PyTuple_SET_ITEM(ret, 2, data_list);

        datai = data;

        PyTuple_SET_ITEM(range_tup, 0, PyRRD_Int_FromLong((long) start));
        PyTuple_SET_ITEM(range_tup, 1, PyRRD_Int_FromLong((long) end));
        PyTuple_SET_ITEM(range_tup, 2, PyRRD_Int_FromLong((long) step));

        for (i = 0; i < ds_cnt; i++)
            PyTuple_SET_ITEM(dsnam_tup, i, PyRRD_String_FromString(ds_namv[i]));

        for (i = 0; i < row; i++) {
            t = PyTuple_New(ds_cnt);
            PyList_SET_ITEM(data_list, i, t);

            for (j = 0; j < ds_cnt; j++) {
                dv = *(datai++);
                if (isnan(dv)) {
                    PyTuple_SET_ITEM(t, j, Py_None);
                    Py_INCREF(Py_None);
                } else
                    PyTuple_SET_ITEM(t, j, PyFloat_FromDouble((double) dv));
            }
        }

        for (i = 0; i < ds_cnt; i++)
            rrd_freemem(ds_namv[i]);
    }

    rrd_freemem(ds_namv);
    rrd_freemem(data);
    destroy_args();
    return ret;
}

static char _rrdtool_flushcached__doc__[] = "Flush RRD files from memory.\n\n\
  Usage: flushcached(args..)\n\
  Arguments:\n\n\
    [-d|--daemon address]\n\
    filename\n\
    [filename ...]\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdflushcached.en.html";

static PyObject *
_rrdtool_flushcached(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    int status;

    if (convert_args("flushcached", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    status = rrd_flushcached(rrdtool_argc, rrdtool_argv);
    Py_END_ALLOW_THREADS

    if (status != 0) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        Py_INCREF(Py_None);
        ret = Py_None;
    }

    destroy_args();
    return ret;
}

#ifdef HAVE_RRD_GRAPH

static char _rrdtool_graph__doc__[] = "Create a graph based on one or more " \
  "RRDs.\n\n\
  Usage: graph(args..)\n\
  Arguments:\n\n\
    filename | -\n\
    [-s|--start start]\n\
    [-e|--end end]\n\
    [-S|--step step]\n\
    [-t|--title string]\n\
    [-v|--vertical-label string]\n\
    [-w|--width pixels]\n\
    [-h|--height pixels]\n\
    [-j|--only-graph]\n\
    [-D|--full-size-mode]\n\
    [-u|--upper-limit value]\n\
    [-l|--lower-limit value]\n\
    [-r|--rigid]\n\
    [-A|--alt-autoscale]\n\
    [-J|--alt-autoscale-min]\n\
    [-M|--alt-autoscale-max]\n\
    [-N|--no-gridfit]\n\
    [-x|--x-grid (GTM:GST:MTM:MST:LTM:LST:LPR:LFM|none)]\n\
    [-y|--y-grid (grid step:label factor|none)]\n\
    [--week-fmt strftime format string]\n\
    [--left-axis-formatter formatter-name]\n\
    [--left-axis-format format-string]\n\
    [-Y|--alt-y-grid]\n\
    [-o|--logarithmic]\n\
    [-X|--units-exponent value]\n\
    [-L|--units-length value]\n\
    [--units=si]\n\
    [--right-axis scale:shift]\n\
    [--right-axis-label label]\n\
    [--right-axis-format format-string]\n\
    [-g|--no-legend]\n\
    [-F|--force-rules-legend]\n\
    [--legend-position=(north|south|west|east)]\n\
    [--legend-direction=(topdown|bottomup)]\n\
    [-z|--lazy]\n\
    [-d|--daemon address]\n\
    [-f|--imginfo printfstr]\n\
    [-c|--color COLORTAG#rrggbb[aa]]\n\
    [--grid-dash on:off]\n\
    [--border width]\n\
    [--dynamic-labels]\n\
    [-m|--zoom factor]\n\
    [-n|--font FONTTAG:size:[font]]\n\
    [-R|--font-render-mode {normal,light,mono}]\n\
    [-B|--font-smoothing-threshold size]\n\
    [-P|--pango-markup]\n\
    [-G|--graph-render-mode {normal,mono}]\n\
    [-E|--slope-mode]\n\
    [-a|--imgformat {PNG,SVG,EPS,PDF,XML,XMLENUM,JSON,JSONTIME,CSV,TSV,SSV}]\n\
    [-i|--interlaced]\n\
    [-T|--tabwidth value]\n\
    [-b|--base value]\n\
    [-W|--watermark string]\n\
    [-Z|--use-nan-for-all-missing-data]\n\
    DEF:vname=rrdfile:ds-name:CF[:step=step][:start=time][:end=time]\n\
    CDEF:vname=RPN expression\n\
    VDEF=vname:RPN expression\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdgraph.en.html";

static PyObject *
_rrdtool_graph(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    int xsize, ysize, i, status;
    double ymin, ymax;
    char **calcpr;

    if (convert_args("graph", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    status = rrd_graph(rrdtool_argc, rrdtool_argv, &calcpr, &xsize, &ysize,
                       NULL, &ymin, &ymax);
    Py_END_ALLOW_THREADS

    if (status == -1) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        ret = PyTuple_New(3);

        PyTuple_SET_ITEM(ret, 0, PyRRD_Int_FromLong((long) xsize));
        PyTuple_SET_ITEM(ret, 1, PyRRD_Int_FromLong((long) ysize));

        if (calcpr) {
            PyObject *e, *t;

            e = PyList_New(0);
            PyTuple_SET_ITEM(ret, 2, e);

            for (i = 0; calcpr[i]; i++) {
                t = PyRRD_String_FromString(calcpr[i]);
                PyList_Append(e, t);
                Py_DECREF(t);
                rrd_freemem(calcpr[i]);
            }
        } else {
            Py_INCREF(Py_None);
            PyTuple_SET_ITEM(ret, 2, Py_None);
        }
    }

    rrd_freemem(calcpr);
    destroy_args();
    return ret;
}

static char _rrdtool_graphv__doc__[] = "Create a graph based on one or more " \
  "RRDs and return data in RRDtool info format.\n\n\
  This function works the same way as 'graph', but will return a info\n\
  dictionary instead of None.\n\n\
  Full documentation can be found at (graphv section):\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdgraph.en.html";

static PyObject *
_rrdtool_graphv(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    rrd_info_t *data;

    if (convert_args("graphv", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    data = rrd_graph_v(rrdtool_argc, rrdtool_argv);
    Py_END_ALLOW_THREADS

    if (data == NULL) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        ret = _rrdtool_util_info2dict(data);
        rrd_info_free(data);
    }

    destroy_args();
    return ret;
}

static char _rrdtool_xport__doc__[] = "Dictionary representation of data " \
  "stored in RRDs.\n\n\
  Usage: xport(args..)\n\
  Arguments:\n\n\
    [-s[--start seconds]\n\
    [-e|--end seconds]\n\
    [-m|--maxrows rows]\n\
    [--step value]\n\
    [--json]\n\
    [--enumds]\n\
    [--daemon address]\n\
    [DEF:vname=rrd:ds-name:CF]\n\
    [CDEF:vname=rpn-expression]\n\
    [XPORT:vname[:legend]]\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdxport.en.html";

static PyObject *
_rrdtool_xport(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    int xsize, status;
    char **legend_v;
    time_t start, end;
    unsigned long step, col_cnt;
    rrd_value_t *data, *datai;

    if (convert_args("xport", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    status = rrd_xport(rrdtool_argc, rrdtool_argv, &xsize, &start, &end, &step,
                       &col_cnt, &legend_v, &data);
    Py_END_ALLOW_THREADS

    if (status == -1) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        PyObject *meta_dict, *data_list, *legend_list, *t;
        rrd_value_t dv;
        unsigned long i, j, row_cnt = (end - start) / step;

        ret = PyDict_New();
        meta_dict = PyDict_New();
        legend_list = PyList_New(col_cnt);
        data_list = PyList_New(row_cnt);

        PyDict_SetItem(ret, PyRRD_String_FromString("meta"), meta_dict);
        PyDict_SetItem(ret, PyRRD_String_FromString("data"), data_list);

        datai = data;

        PyDict_SetItem(meta_dict,
            PyRRD_String_FromString("start"),
            PyRRD_Int_FromLong((long) start));
        PyDict_SetItem(meta_dict,
            PyRRD_String_FromString("end"),
            PyRRD_Int_FromLong((long) end));
        PyDict_SetItem(meta_dict,
            PyRRD_String_FromString("step"),
            PyRRD_Int_FromLong((long) step));
        PyDict_SetItem(meta_dict,
            PyRRD_String_FromString("rows"),
            PyRRD_Int_FromLong((long) row_cnt));
        PyDict_SetItem(meta_dict,
            PyRRD_String_FromString("columns"),
            PyRRD_Int_FromLong((long) col_cnt));
        PyDict_SetItem(meta_dict,
            PyRRD_String_FromString("legend"),
            legend_list);

        for (i = 0; i < col_cnt; i++)
            PyList_SET_ITEM(legend_list, i, PyRRD_String_FromString(legend_v[i]));

        for (i = 0; i < row_cnt; i++) {
            t = PyTuple_New(col_cnt);
            PyList_SET_ITEM(data_list, i, t);

            for (j = 0; j < col_cnt; j++) {
                dv = *(datai++);

                if (isnan(dv)) {
                    PyTuple_SET_ITEM(t, j, Py_None);
                    Py_INCREF(Py_None);
                } else {
                    PyTuple_SET_ITEM(t, j, PyFloat_FromDouble((double) dv));
                }
            }
        }

        for (i = 0; i < col_cnt; i++)
            rrd_freemem(legend_v[i]);

        rrd_freemem(legend_v);
        rrd_freemem(data);
    }

    destroy_args();

    return ret;
}

#endif /* HAVE_RRD_GRAPH */

static char _rrdtool_list__doc__[] = "List RRDs in storage.\n\n" \
  "Usage: list(args..)\n\
  Arguments:\n\n\
    dirname\n\
    [-r|--recursive]\n\
    [-d|--daemon address]";

static PyObject *
_rrdtool_list(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret, *str;
    char *data, *ptr, *end;

    if (convert_args("list", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    data = rrd_list(rrdtool_argc, rrdtool_argv);
    Py_END_ALLOW_THREADS

    if (data == NULL) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        ret = PyList_New(0);
        ptr = data;
        end = strchr(ptr, '\n');

        while (end) {
            *end = '\0';
            str = PyRRD_String_FromString(ptr);

            if (PyList_Append(ret, str)) {
                PyErr_SetString(rrdtool_OperationalError, "Failed to append to list");
                ret = NULL;
                break;
            }

            ptr = end + 1;

            if (strlen(ptr) == 0)
                break;

            end = strchr(ptr, '\n');
        }

        rrd_freemem(data);
    }

    destroy_args();
    return ret;
}

static char _rrdtool_tune__doc__[] = "Modify some basic properties of a " \
  "Round Robin Database.\n\n\
  Usage: tune(args..)\n\
  Arguments:\n\n\
    filename\n\
    [-h|--heartbeat ds-name:heartbeat]\n\
    [-i|--minimum ds-name:min]\n\
    [-a|--maximum ds-name:max]\n\
    [-d|--data-source-type ds-name:DST]\n\
    [-r|--data-source-rename old-name:new-name]\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdtune.en.html";

static PyObject *
_rrdtool_tune(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    int status;

    if (convert_args("tune", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    status = rrd_tune(rrdtool_argc, rrdtool_argv);
    Py_END_ALLOW_THREADS

    if (status == -1) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        Py_INCREF(Py_None);
        ret = Py_None;
    }

    destroy_args();
    return ret;
}

static char _rrdtool_first__doc__[] = "Get the first UNIX timestamp of the "\
  "first data sample in an Round Robin Database.\n\n\
  Usage: first(args..)\n\
  Arguments:\n\n\
    filename\n\
    [--rraindex number]\n\
    [-d|--daemon address]\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdfirst.en.html";

static PyObject *
_rrdtool_first(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    int ts;

    if (convert_args("first", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    ts = rrd_first(rrdtool_argc, rrdtool_argv);
    Py_END_ALLOW_THREADS

    if (ts == -1) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else
        ret = PyRRD_Int_FromLong((long) ts);

    destroy_args();
    return ret;
}

static char _rrdtool_last__doc__[] = "Get the UNIX timestamp of the most "\
  "recent data sample in an Round Robin Database.\n\n\
  Usage: last(args..)\n\
  Arguments:\n\n\
    filename\n\
    [-d|--daemon address]\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdlast.en.html";

static PyObject *
_rrdtool_last(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    int ts;

    if (convert_args("last", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    ts = rrd_last(rrdtool_argc, rrdtool_argv);
    Py_END_ALLOW_THREADS

    if (ts == -1) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else
        ret = PyRRD_Int_FromLong((long) ts);

    destroy_args();
    return ret;
}

static char _rrdtool_resize__doc__[] = "Modify the number of rows in a "\
 "Round Robin Database.\n\n\
  Usage: resize(args..)\n\
  Arguments:\n\n\
    filename\n\
    rra-num\n\
    GROW|SHRINK\n\
    rows\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdlast.en.html";

static PyObject *
_rrdtool_resize(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    int status;

    if (convert_args("resize", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    status = rrd_resize(rrdtool_argc, rrdtool_argv);
    Py_END_ALLOW_THREADS

    if (status == -1) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        Py_INCREF(Py_None);
        ret = Py_None;
    }

    destroy_args();
    return ret;
}

static char _rrdtool_info__doc__[] = "Extract header information from an "\
 "Round Robin Database.\n\n\
  Usage: info(filename, ...)\n\
  Arguments:\n\n\
    filename\n\
    [-d|--daemon address]\n\
    [-F|--noflush]\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdinfo.en.html";

static PyObject *
_rrdtool_info(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret;
    rrd_info_t *data;

    if (convert_args("info", args) == -1)
        return NULL;

    Py_BEGIN_ALLOW_THREADS
    data = rrd_info(rrdtool_argc, rrdtool_argv);
    Py_END_ALLOW_THREADS

    if (data == NULL) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        ret = _rrdtool_util_info2dict(data);
        rrd_info_free(data);
    }

    destroy_args();
    return ret;
}

static char _rrdtool_lastupdate__doc__[] = "Returns datetime and value stored "\
 "for each datum in the most recent update of an RRD.\n\n\
  Usage: lastupdate(filename, ...)\n\
  Arguments:\n\n\
    filename\n\
    [-d|--daemon address]\n\n\
  Full documentation can be found at:\n\
  http://oss.oetiker.ch/rrdtool/doc/rrdlastupdate.en.html";

static PyObject *
_rrdtool_lastupdate(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *ret, *ds_dict, *lastupd;
    int status;
    time_t last_update;
    char **ds_names, **last_ds;
    unsigned long ds_cnt, i;

    if (convert_args("lastupdate", args) == -1)
        return NULL;
    else if (rrdtool_argc < 2) {
        PyErr_SetString(rrdtool_ProgrammingError, "Missing filename argument");
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
    status = rrd_lastupdate_r(rrdtool_argv[1],
                              &last_update,
                              &ds_cnt,
                              &ds_names,
                              &last_ds);
    Py_END_ALLOW_THREADS

    if (status != 0) {
        PyErr_SetString(rrdtool_OperationalError, rrd_get_error());
        rrd_clear_error();
        ret = NULL;
    } else {
        /* convert last_update to Python datetime object */
        ret = PyDict_New();
        ds_dict = PyDict_New();
        lastupd = PyRRD_DateTime_FromTS(last_update);

        PyDict_SetItemString(ret, "date", lastupd);
        PyDict_SetItemString(ret, "ds", ds_dict);

        Py_DECREF(lastupd);
        Py_DECREF(ds_dict);

        for (i = 0; i < ds_cnt; i++) {
            PyObject* val = Py_None;

            double num;
            if (sscanf(last_ds[i], "%lf", &num) == 1) {
                val = PyFloat_FromDouble(num);
            }

            if (!val) {
            	free(last_ds[i]);
            	free(last_ds);
            	free(ds_names);
                return NULL;
            }

            PyDict_SetItemString(ds_dict, ds_names[i], val);
            Py_DECREF(val);

            free(last_ds[i]);
            free(ds_names[i]);
        }

        free(last_ds);
        free(ds_names);

    }

    destroy_args();

    return ret;
}


/** An Python object which will hold an callable for fetch callbacks */
static PyObject *_rrdtool_fetch_callable = NULL;

static int
_rrdtool_fetch_cb_wrapper(
    const char *filename,
    enum cf_en cf_idx,
    time_t *start,
    time_t *end,
    unsigned long *step,
    unsigned long *ds_cnt,
    char ***ds_namv,
    rrd_value_t **data)
{
    PyObject *args;
    PyObject *kwargs;
    PyObject *ret = NULL;
    PyObject *tmp;
    PyObject *tmp_min_ts;
    PyGILState_STATE gstate;
    Py_ssize_t rowcount = 0;
    int rc = -1;
    unsigned int i, ii;

    gstate = PyGILState_Ensure();

    if (_rrdtool_fetch_callable == NULL) {
        rrd_set_error("use rrdtool.register_fetch_cb to register a fetch callback");
        goto gil_release_err;
    }

    args = PyTuple_New(0);
    kwargs = PyDict_New();

    /* minimum possible UNIX datetime */
    tmp_min_ts = PyLong_FromLong(0);

    PyObject *po_filename = PyRRD_String_FromString(filename);
    PyDict_SetItemString(kwargs, "filename", po_filename);
    Py_DECREF(po_filename);

    PyObject *po_cfstr = PyRRD_String_FromString(PyRRD_String_FromCF(cf_idx));
    PyDict_SetItemString(kwargs, "cf", po_cfstr);
    Py_DECREF(po_cfstr);

    PyObject *po_start = PyLong_FromLong(*start);
    PyDict_SetItemString(kwargs, "start", po_start);
    Py_DECREF(po_start);

    PyObject *po_end = PyLong_FromLong(*end);
    PyDict_SetItemString(kwargs, "end", po_end);
    Py_DECREF(po_end);

    PyObject *po_step = PyLong_FromUnsignedLong(*step);
    PyDict_SetItemString(kwargs, "step", po_step);
    Py_DECREF(po_step);

    /* execute Python callback method */
    ret = PyObject_Call(_rrdtool_fetch_callable, args, kwargs);
    Py_DECREF(args);
    Py_DECREF(kwargs);

    if (ret == NULL) {
        rrd_set_error("calling python callback failed");
        goto gil_release_err;
    }

    /* handle return value of callback */
    if (!PyDict_Check(ret)) {
        rrd_set_error("expected callback method to be a dict");
        goto gil_release_err;
    }

    tmp = PyDict_GetItemString(ret, "step");
    if (tmp == NULL) {
        rrd_set_error("expected 'step' key in callback return value");
        goto gil_release_err;
    } else if (!PyRRD_Long_Check(tmp)) {
        rrd_set_error("the 'step' key in callback return value must be int");
        goto gil_release_err;
    } else
        *step = PyLong_AsLong(tmp);

    tmp = PyDict_GetItemString(ret, "start");
    if (tmp == NULL) {
        rrd_set_error("expected 'start' key in callback return value");
        goto gil_release_err;
    } else if (!PyRRD_Long_Check(tmp)) {
        rrd_set_error("expected 'start' key in callback return value to be "
            "of type int");
        goto gil_release_err;
    } else if (PyObject_RichCompareBool(tmp, tmp_min_ts, Py_EQ) || 
               PyObject_RichCompareBool(tmp, po_start, Py_LT)) {
        rrd_set_error("expected 'start' value in callback return dict to be "
            "equal or earlier than passed start timestamp");
        goto gil_release_err;
    } else {
        *start = PyLong_AsLong(po_start);

        if (*start == -1) {
            rrd_set_error("expected 'start' value in callback return value to"
                " not exceed LONG_MAX");
            goto gil_release_err;
        }
    }

    tmp = PyDict_GetItemString(ret, "data");
    if (tmp == NULL) {
        rrd_set_error("expected 'data' key in callback return value");
        goto gil_release_err;
    } else if (!PyDict_Check(tmp)) {
        rrd_set_error("expected 'data' key in callback return value of type "
            "dict");
        goto gil_release_err;
    } else {
        *ds_cnt = (unsigned long)PyDict_Size(tmp);
        *ds_namv = (char **)calloc(*ds_cnt, sizeof(char *));

        if (*ds_namv == NULL) {
            rrd_set_error("an error occured while allocating memory for "
                "ds_namv when allocating memory for python callback");
            goto gil_release_err;
        }

        PyObject *key, *value;
        Py_ssize_t pos = 0;  /* don't use pos for indexing */
        unsigned int x = 0;

        while (PyDict_Next(tmp, &pos, &key, &value)) {
            char *key_str = PyRRD_String_AS_STRING(key);

            if (key_str == NULL) {
                rrd_set_error("key of 'data' element from callback return "
                    "value is not a string");
                goto gil_release_free_dsnamv_err;
            } else if (strlen(key_str) > DS_NAM_SIZE) {
                rrd_set_error("key '%s' longer than the allowed maximum of %d "
                    "byte", key_str, DS_NAM_SIZE - 1);
                goto gil_release_free_dsnamv_err;
            }

            if ((((*ds_namv)[x]) = (char *)malloc(sizeof(char) * DS_NAM_SIZE)) == NULL) {
                rrd_set_error("malloc fetch ds_namv entry");
                goto gil_release_free_dsnamv_err;
            }

            strncpy((*ds_namv)[x], key_str, DS_NAM_SIZE - 1);
            (*ds_namv)[x][DS_NAM_SIZE - 1] = '\0';

            if (!PyList_Check(value)) {
                rrd_set_error("expected 'data' dict values in callback return "
                    "value of type list");
                goto gil_release_free_dsnamv_err;
            } else if (PyList_Size(value) > rowcount)
                rowcount = PyList_Size(value);

            ++x;
        }

        *end = *start + *step * rowcount;

        if (((*data) = (rrd_value_t *)malloc(*ds_cnt * rowcount * sizeof(rrd_value_t))) == NULL) {
            rrd_set_error("malloc fetch data area");
            goto gil_release_free_dsnamv_err;
        }

        for (i = 0; i < *ds_cnt; i++) {
            for (ii = 0; ii < (unsigned int)rowcount; ii++) {
                char *ds_namv_i = (*ds_namv)[i];
                double va;
                PyObject *lstv = PyList_GetItem(PyDict_GetItemString(tmp, ds_namv_i), ii);

                /* lstv may be NULL here in case an IndexError has been raised;
                   in such case the rowcount is higher than the number of elements for
                   the list of that ds. use DNAN as value for these then */
                if (lstv == NULL || lstv == Py_None) {
                    if (lstv == NULL)
                        PyErr_Clear();
                    va = DNAN;
                }
                else {
                    va = PyFloat_AsDouble(lstv);
                    if (va == -1.0 && PyErr_Occurred()) {
                        PyObject *exc_type, *exc_value, *exc_value_str = NULL, *exc_tb;
                        PyErr_Fetch(&exc_type, &exc_value, &exc_tb);

                        if (exc_value != NULL) {
                            exc_value_str = PyObject_Str(exc_value);
                            char *exc_str = PyRRD_String_AS_STRING(exc_value_str);
                            rrd_set_error(exc_str);
                            Py_DECREF(exc_value);
                        }

                        Py_DECREF(exc_type);
                        if (exc_value_str != NULL)
                            Py_DECREF(exc_value_str);
                        if (exc_tb != NULL)
                            Py_DECREF(exc_tb);
                        goto gil_release_free_dsnamv_err;
                    }
                }

                (*data)[i + ii * (*ds_cnt)] = va;
            }
        }
    }

    /* success */
    rc = 1;
    goto gil_release;

gil_release_free_dsnamv_err:
    for (i = 0; i < *ds_cnt; i++) {
        if ((*ds_namv)[i]) {
            free((*ds_namv)[i]);
        }
    }

    free(*ds_namv);

gil_release_err:
    rc = -1;

gil_release:
    if (ret != NULL)
        Py_DECREF(ret);
    PyGILState_Release(gstate);
    return rc;
}

static char _rrdtool_register_fetch_cb__doc__[] = "Register callback for "
    "fetching data";

static PyObject *
_rrdtool_register_fetch_cb(PyObject *Py_UNUSED(self), PyObject *args)
{
    PyObject *callable;

    if (!PyArg_ParseTuple(args, "O", &callable))
        return NULL;
    else if (!PyCallable_Check(callable)) {
        PyErr_SetString(rrdtool_ProgrammingError, "first argument must be callable");
        return NULL;
    } else {
        _rrdtool_fetch_callable = callable;
        rrd_fetch_cb_register(_rrdtool_fetch_cb_wrapper);
        Py_RETURN_NONE;
    }
}

static char _rrdtool_clear_fetch_cb__doc__[] = "Clear callback for "
    "fetching data";

static PyObject *
_rrdtool_clear_fetch_cb(PyObject *Py_UNUSED(self), PyObject *Py_UNUSED(args))
{
    if (_rrdtool_fetch_callable == NULL) {
        PyErr_SetString(rrdtool_ProgrammingError, "no callback set");
        return NULL;
    }

    _rrdtool_fetch_callable = NULL;
    rrd_fetch_cb_register(NULL);
    Py_RETURN_NONE;
}

static char _rrdtool_lib_version__doc__[] = "Get the version this binding "\
  "was compiled against.";

/**
 * Returns a str object that contains the librrd version.
 *
 * @return librrd version (Python str object)
 */
static PyObject *
_rrdtool_lib_version(PyObject *Py_UNUSED(self), PyObject *Py_UNUSED(args))
{
    return PyRRD_String_FromString(rrd_strversion());
}

/** Method table. */
static PyMethodDef rrdtool_methods[] = {
    {"create", (PyCFunction)_rrdtool_create,
     METH_VARARGS, _rrdtool_create__doc__},
    {"dump", (PyCFunction)_rrdtool_dump,
     METH_VARARGS, _rrdtool_dump__doc__},
    {"update", (PyCFunction)_rrdtool_update,
     METH_VARARGS, _rrdtool_update__doc__},
    {"updatev", (PyCFunction)_rrdtool_updatev,
     METH_VARARGS, _rrdtool_updatev__doc__},
    {"fetch", (PyCFunction)_rrdtool_fetch,
     METH_VARARGS, _rrdtool_fetch__doc__},
    {"flushcached", (PyCFunction)_rrdtool_flushcached,
     METH_VARARGS, _rrdtool_flushcached__doc__},
#ifdef HAVE_RRD_GRAPH
    {"graph", (PyCFunction)_rrdtool_graph,
     METH_VARARGS, _rrdtool_graph__doc__},
    {"graphv", (PyCFunction)_rrdtool_graphv,
     METH_VARARGS, _rrdtool_graphv__doc__},
    {"xport", (PyCFunction)_rrdtool_xport,
     METH_VARARGS, _rrdtool_xport__doc__},
#endif
    {"list", (PyCFunction)_rrdtool_list,
     METH_VARARGS, _rrdtool_list__doc__},
    {"tune", (PyCFunction)_rrdtool_tune,
     METH_VARARGS, _rrdtool_tune__doc__},
    {"first", (PyCFunction)_rrdtool_first,
     METH_VARARGS, _rrdtool_first__doc__},
    {"last", (PyCFunction)_rrdtool_last,
     METH_VARARGS, _rrdtool_last__doc__},
    {"resize", (PyCFunction)_rrdtool_resize,
     METH_VARARGS, _rrdtool_resize__doc__},
    {"info", (PyCFunction)_rrdtool_info,
     METH_VARARGS, _rrdtool_info__doc__},
    {"lastupdate", (PyCFunction)_rrdtool_lastupdate,
     METH_VARARGS, _rrdtool_lastupdate__doc__},
    {"register_fetch_cb", (PyCFunction)_rrdtool_register_fetch_cb,
     METH_VARARGS, _rrdtool_register_fetch_cb__doc__},
    {"clear_fetch_cb", (PyCFunction)_rrdtool_clear_fetch_cb,
     METH_NOARGS, _rrdtool_clear_fetch_cb__doc__},
    {"lib_version", (PyCFunction)_rrdtool_lib_version,
     METH_NOARGS, _rrdtool_lib_version__doc__},
    {NULL, NULL, 0, NULL}
};

/** Library init function. */
#ifdef HAVE_PY3K
static struct PyModuleDef rrdtoolmodule = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "rrdtool",
    .m_doc = "Python bindings for rrdtool",
    .m_size = -1,
    .m_methods = rrdtool_methods
};

#endif

#ifdef HAVE_PY3K
PyMODINIT_FUNC
PyInit_rrdtool(void)
#else
void
initrrdtool(void)
#endif
{
    PyObject *m;

    PyDateTime_IMPORT;  /* initialize PyDateTime_ functions */

#ifdef HAVE_PY3K
    m = PyModule_Create(&rrdtoolmodule);
#else
    m = Py_InitModule3("rrdtool",
                       rrdtool_methods,
                       "Python bindings for rrdtool");
#endif

    if (m == NULL)
#ifdef HAVE_PY3K
        return NULL;
#else
        return;
#endif

    rrdtool_ProgrammingError = PyErr_NewException("rrdtool.ProgrammingError",
                                                  NULL, NULL);
    Py_INCREF(rrdtool_ProgrammingError);
    PyModule_AddObject(m, "ProgrammingError", rrdtool_ProgrammingError);

    rrdtool_OperationalError = PyErr_NewException("rrdtool.OperationalError",
                                                  NULL, NULL);
    Py_INCREF(rrdtool_OperationalError);
    PyModule_AddObject(m, "OperationalError", rrdtool_OperationalError);
    PyModule_AddStringConstant(m, "__version__", _version);

#ifdef HAVE_PY3K
    return m;
#endif
}
