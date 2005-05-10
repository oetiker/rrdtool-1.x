/*
 * rrdtoolmodule.c
 *
 * RRDTool Python binding
 *
 * Author  : Hye-Shik Chang <perky@fallin.lv>
 * Date    : $Date: 2003/02/22 07:41:19 $
 * Created : 23 May 2002
 *
 * $Revision: 1.14 $
 *
 *  ==========================================================================
 *  This file is part of py-rrdtool.
 *
 *  py-rrdtool is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  py-rrdtool is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with Foobar; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

static const char *__version__ = "$Revision: 1.14 $";

#include "Python.h"
#include "rrd.h"
#include "rrd_extra.h"

static PyObject *ErrorObject;
extern int optind;
extern int opterr;

/* forward declaration to keep compiler happy */
void initrrdtool(void);

static int
create_args(char *command, PyObject *args, int *argc, char ***argv)
{
    PyObject        *o;
    int              size, i;
    
    size    = PyTuple_Size(args);
    *argv   = PyMem_New(char *, size + 1);
    if (*argv == NULL)
        return -1;

    for (i = 0; i < size; i++) {
        o = PyTuple_GET_ITEM(args, i);
        if (PyString_Check(o))
            (*argv)[i + 1] = PyString_AS_STRING(o);
        else {
            PyMem_Del(*argv);
            PyErr_Format(PyExc_TypeError, "argument %d must be string", i);
            return -1;
        }
    }
    (*argv)[0] = command;
    *argc = size + 1;

    /* reset getopt state */
    opterr = optind = 0;

    return 0;
}

static void
destroy_args(char ***argv)
{
    PyMem_Del(*argv);
    *argv = NULL;
}

static char PyRRD_create__doc__[] =
"create(args..): Set up a new Round Robin Database\n\
    create filename [--start|-b start time] \
[--step|-s step] [DS:ds-name:DST:heartbeat:min:max] \
[RRA:CF:xff:steps:rows]";

static PyObject *
PyRRD_create(PyObject *self, PyObject *args)
{
    PyObject        *r;
    char           **argv;
    int              argc;

    if (create_args("create", args, &argc, &argv) < 0)
        return NULL;

    if (rrd_create(argc, argv) == -1) {
        PyErr_SetString(ErrorObject, rrd_get_error());
        rrd_clear_error();
        r = NULL;
    } else {
        Py_INCREF(Py_None);
        r = Py_None;
    }

    destroy_args(&argv);
    return r;
}

static char PyRRD_update__doc__[] =
"update(args..): Store a new set of values into the rrd\n"
"    update filename [--template|-t ds-name[:ds-name]...] "
"N|timestamp:value[:value...] [timestamp:value[:value...] ...]";

static PyObject *
PyRRD_update(PyObject *self, PyObject *args)
{
    PyObject        *r;
    char           **argv;
    int              argc;

    if (create_args("update", args, &argc, &argv) < 0)
        return NULL;

    if (rrd_update(argc, argv) == -1) {
        PyErr_SetString(ErrorObject, rrd_get_error());
        rrd_clear_error();
        r = NULL;
    } else {
        Py_INCREF(Py_None);
        r = Py_None;
    }

    destroy_args(&argv);
    return r;
}

static char PyRRD_fetch__doc__[] =
"fetch(args..): fetch data from an rrd.\n"
"    fetch filename CF [--resolution|-r resolution] "
"[--start|-s start] [--end|-e end]";

static PyObject *
PyRRD_fetch(PyObject *self, PyObject *args)
{
    PyObject        *r;
    rrd_value_t     *data, *datai;
    unsigned long    step, ds_cnt;
    time_t           start, end;
    int              argc;
    char           **argv, **ds_namv;

    if (create_args("fetch", args, &argc, &argv) < 0)
        return NULL;

    if (rrd_fetch(argc, argv, &start, &end, &step,
                  &ds_cnt, &ds_namv, &data) == -1) {
        PyErr_SetString(ErrorObject, rrd_get_error());
        rrd_clear_error();
        r = NULL;
    } else {
        /* Return :
          ((start, end, step), (name1, name2, ...), [(data1, data2, ..), ...]) */
        PyObject    *range_tup, *dsnam_tup, *data_list, *t;
        unsigned long          i, j, row;
        rrd_value_t  dv;

        row = ((end - start) / step + 1);

        r = PyTuple_New(3);
        range_tup = PyTuple_New(3);
        dsnam_tup = PyTuple_New(ds_cnt);
        data_list = PyList_New(row);
        PyTuple_SET_ITEM(r, 0, range_tup);
        PyTuple_SET_ITEM(r, 1, dsnam_tup);
        PyTuple_SET_ITEM(r, 2, data_list);

        datai = data;

        PyTuple_SET_ITEM(range_tup, 0, PyInt_FromLong((long)start));
        PyTuple_SET_ITEM(range_tup, 1, PyInt_FromLong((long)end));
        PyTuple_SET_ITEM(range_tup, 2, PyInt_FromLong((long)step));

        for (i = 0; i < ds_cnt; i++)
            PyTuple_SET_ITEM(dsnam_tup, i, PyString_FromString(ds_namv[i]));

        for (i = 0; i < row; i ++) {
            t = PyTuple_New(ds_cnt);
            PyList_SET_ITEM(data_list, i, t);

            for (j = 0; j < ds_cnt; j++) {
                dv = *(datai++);
                if (isnan(dv)) {
                    PyTuple_SET_ITEM(t, j, Py_None);
                    Py_INCREF(Py_None);
                } else {
                    PyTuple_SET_ITEM(t, j, PyFloat_FromDouble((double)dv));
                }
            }
        }

        for (i = 0; i < ds_cnt; i++)
            free(ds_namv[i]);
        free(ds_namv); /* rrdtool don't use PyMem_Malloc :) */
        free(data);
    }

    destroy_args(&argv);
    return r;
}

static char PyRRD_graph__doc__[] =
"graph(args..): Create a graph based on data from one or several RRD\n"
"    graph filename [-s|--start seconds] "
"[-e|--end seconds] [-x|--x-grid x-axis grid and label] "
"[-y|--y-grid y-axis grid and label] [--alt-y-grid] [--alt-y-mrtg] "
"[--alt-autoscale] [--alt-autoscale-max] [--units-exponent] value "
"[-v|--vertical-label text] [-w|--width pixels] [-h|--height pixels] "
"[-i|--interlaced] "
"[-f|--imginfo formatstring] [-a|--imgformat GIF|PNG|GD] "
"[-B|--background value] [-O|--overlay value] "
"[-U|--unit value] [-z|--lazy] [-o|--logarithmic] "
"[-u|--upper-limit value] [-l|--lower-limit value] "
"[-g|--no-legend] [-r|--rigid] [--step value] "
"[-b|--base value] [-c|--color COLORTAG#rrggbb] "
"[-t|--title title] [DEF:vname=rrd:ds-name:CF] "
"[CDEF:vname=rpn-expression] [PRINT:vname:CF:format] "
"[GPRINT:vname:CF:format] [COMMENT:text] "
"[HRULE:value#rrggbb[:legend]] [VRULE:time#rrggbb[:legend]] "
"[LINE{1|2|3}:vname[#rrggbb[:legend]]] "
"[AREA:vname[#rrggbb[:legend]]] "
"[STACK:vname[#rrggbb[:legend]]]";

static PyObject *
PyRRD_graph(PyObject *self, PyObject *args)
{
    PyObject        *r;
    char           **argv, **calcpr;
    int              argc, xsize, ysize, i;
    double          ymin, ymax;
    if (create_args("graph", args, &argc, &argv) < 0)
        return NULL;

    if (rrd_graph(argc, argv, &calcpr, &xsize, &ysize, NULL, &ymin, &ymax) == -1) {
        PyErr_SetString(ErrorObject, rrd_get_error());
        rrd_clear_error();
        r = NULL;
    } else {
        r = PyTuple_New(3);

        PyTuple_SET_ITEM(r, 0, PyInt_FromLong((long)xsize));
        PyTuple_SET_ITEM(r, 1, PyInt_FromLong((long)ysize));

        if (calcpr) {
            PyObject    *e, *t;
            
            e = PyList_New(0);
            PyTuple_SET_ITEM(r, 2, e);

            for(i = 0; calcpr[i]; i++) {
                t = PyString_FromString(calcpr[i]);
                PyList_Append(e, t);
                Py_DECREF(t);
                free(calcpr[i]);
            }
            free(calcpr);
        } else {
            Py_INCREF(Py_None);
            PyTuple_SET_ITEM(r, 2, Py_None);
        }
    }

    destroy_args(&argv);
    return r;
}

static char PyRRD_tune__doc__[] =
"tune(args...): Modify some basic properties of a Round Robin Database\n"
"    tune filename [--heartbeat|-h ds-name:heartbeat] "
"[--minimum|-i ds-name:min] [--maximum|-a ds-name:max] "
"[--data-source-type|-d ds-name:DST] [--data-source-rename|-r old-name:new-name]";

static PyObject *
PyRRD_tune(PyObject *self, PyObject *args)
{
    PyObject        *r;
    char           **argv;
    int              argc;

    if (create_args("tune", args, &argc, &argv) < 0)
        return NULL;

    if (rrd_tune(argc, argv) == -1) {
        PyErr_SetString(ErrorObject, rrd_get_error());
        rrd_clear_error();
        r = NULL;
    } else {
        Py_INCREF(Py_None);
        r = Py_None;
    }

    destroy_args(&argv);
    return r;
}

static char PyRRD_last__doc__[] =
"last(filename): Return the timestamp of the last data sample in an RRD";

static PyObject *
PyRRD_last(PyObject *self, PyObject *args)
{
    PyObject        *r;
    int              argc, ts;
    char           **argv;

    if (create_args("last", args, &argc, &argv) < 0)
        return NULL;

    if ((ts = rrd_last(argc, argv)) == -1) {
        PyErr_SetString(ErrorObject, rrd_get_error());
        rrd_clear_error();
        r = NULL;
    } else
        r = PyInt_FromLong((long)ts);

    destroy_args(&argv);
    return r;
}

static char PyRRD_resize__doc__[] =
"resize(args...): alters the size of an RRA.\n"
"    resize filename rra-num GROW|SHRINK rows";

static PyObject *
PyRRD_resize(PyObject *self, PyObject *args)
{
    PyObject        *r;
    char           **argv;
    int              argc, ts;

    if (create_args("resize", args, &argc, &argv) < 0)
        return NULL;

    if ((ts = rrd_resize(argc, argv)) == -1) {
        PyErr_SetString(ErrorObject, rrd_get_error());
        rrd_clear_error();
        r = NULL;
    } else {
        Py_INCREF(Py_None);
        r = Py_None;
    }

    destroy_args(&argv);
    return r;
}

static char PyRRD_info__doc__[] =
"info(filename): extract header information from an rrd";

static PyObject *
PyRRD_info(PyObject *self, PyObject *args)
{
    PyObject        *r, *t, *ds;
    rrd_t            rrd;
    FILE            *in_file;
    char            *filename;
    unsigned long   i, j;

    if (! PyArg_ParseTuple(args, "s:info", &filename))
        return NULL;

    if (rrd_open(filename, &in_file, &rrd, RRD_READONLY) == -1) {
        PyErr_SetString(ErrorObject, rrd_get_error());
        rrd_clear_error();
        return NULL;
    }
    fclose(in_file);

#define DICTSET_STR(dict, name, value) \
    t = PyString_FromString(value); \
    PyDict_SetItemString(dict, name, t); \
    Py_DECREF(t);

#define DICTSET_CNT(dict, name, value) \
    t = PyInt_FromLong((long)value); \
    PyDict_SetItemString(dict, name, t); \
    Py_DECREF(t);

#define DICTSET_VAL(dict, name, value) \
    t = isnan(value) ? (Py_INCREF(Py_None), Py_None) :  \
        PyFloat_FromDouble((double)value); \
    PyDict_SetItemString(dict, name, t); \
    Py_DECREF(t);

    r = PyDict_New();

    DICTSET_STR(r, "filename", filename);
    DICTSET_STR(r, "rrd_version", rrd.stat_head->version);
    DICTSET_CNT(r, "step", rrd.stat_head->pdp_step);
    DICTSET_CNT(r, "last_update", rrd.live_head->last_up);

    ds = PyDict_New();
    PyDict_SetItemString(r, "ds", ds);
    Py_DECREF(ds);

    for (i = 0; i < rrd.stat_head->ds_cnt; i++) {
        PyObject    *d;

        d = PyDict_New();
        PyDict_SetItemString(ds, rrd.ds_def[i].ds_nam, d);
        Py_DECREF(d);

        DICTSET_STR(d, "ds_name", rrd.ds_def[i].ds_nam);
        DICTSET_STR(d, "type", rrd.ds_def[i].dst);
        DICTSET_CNT(d, "minimal_heartbeat", rrd.ds_def[i].par[DS_mrhb_cnt].u_cnt);
        DICTSET_VAL(d, "min", rrd.ds_def[i].par[DS_min_val].u_val);
        DICTSET_VAL(d, "max", rrd.ds_def[i].par[DS_max_val].u_val);
        DICTSET_STR(d, "last_ds", rrd.pdp_prep[i].last_ds);
        DICTSET_VAL(d, "value", rrd.pdp_prep[i].scratch[PDP_val].u_val);
        DICTSET_CNT(d, "unknown_sec", rrd.pdp_prep[i].scratch[PDP_unkn_sec_cnt].u_cnt);
    }

    ds = PyList_New(rrd.stat_head->rra_cnt);
    PyDict_SetItemString(r, "rra", ds);
    Py_DECREF(ds);

    for (i = 0; i < rrd.stat_head->rra_cnt; i++) {
        PyObject    *d, *cdp;

        d = PyDict_New();
        PyList_SET_ITEM(ds, i, d);

        DICTSET_STR(d, "cf", rrd.rra_def[i].cf_nam);
        DICTSET_CNT(d, "rows", rrd.rra_def[i].row_cnt);
        DICTSET_CNT(d, "pdp_per_row", rrd.rra_def[i].pdp_cnt);
        DICTSET_VAL(d, "xff", rrd.rra_def[i].par[RRA_cdp_xff_val].u_val);

        cdp = PyList_New(rrd.stat_head->ds_cnt);
        PyDict_SetItemString(d, "cdp_prep", cdp);
        Py_DECREF(cdp);

        for (j = 0; j < rrd.stat_head->ds_cnt; j++) {
            PyObject    *cdd;

            cdd = PyDict_New();
            PyList_SET_ITEM(cdp, j, cdd);

            DICTSET_VAL(cdd, "value",
                    rrd.cdp_prep[i*rrd.stat_head->ds_cnt+j].scratch[CDP_val].u_val);
            DICTSET_CNT(cdd, "unknown_datapoints",
                    rrd.cdp_prep[i*rrd.stat_head->ds_cnt+j].scratch[CDP_unkn_pdp_cnt].u_cnt);
        }
    }

    rrd_free(&rrd);

    return r;
}

/* List of methods defined in the module */
#define meth(name, func, doc) {name, (PyCFunction)func, METH_VARARGS, doc}

static PyMethodDef _rrdtool_methods[] = {
    meth("create",  PyRRD_create,   PyRRD_create__doc__),
    meth("update",  PyRRD_update,   PyRRD_update__doc__),
    meth("fetch",   PyRRD_fetch,    PyRRD_fetch__doc__),
    meth("graph",   PyRRD_graph,    PyRRD_graph__doc__),
    meth("tune",    PyRRD_tune,     PyRRD_tune__doc__),
    meth("last",    PyRRD_last,     PyRRD_last__doc__),
    meth("resize",  PyRRD_resize,   PyRRD_resize__doc__),
    meth("info",    PyRRD_info,     PyRRD_info__doc__),
    {NULL, NULL}
};

#define SET_INTCONSTANT(dict, value) \
            t = PyInt_FromLong((long)value); \
            PyDict_SetItemString(dict, #value, t); \
            Py_DECREF(t);
#define SET_STRCONSTANT(dict, value) \
            t = PyString_FromString(value); \
            PyDict_SetItemString(dict, #value, t); \
            Py_DECREF(t);

/* Initialization function for the module */
void
initrrdtool(void)
{
    PyObject    *m, *d, *t;

    /* Create the module and add the functions */
    m = Py_InitModule("rrdtool", _rrdtool_methods);

    /* Add some symbolic constants to the module */
    d = PyModule_GetDict(m);

    SET_STRCONSTANT(d, __version__);
    ErrorObject = PyErr_NewException("_rrdtool.error", NULL, NULL);
    PyDict_SetItemString(d, "error", ErrorObject);

    /* Check for errors */
    if (PyErr_Occurred())
        Py_FatalError("can't initialize the rrdtool module");
}

/*
 * $Id: _rrdtoolmodule.c,v 1.14 2003/02/22 07:41:19 perky Exp $
 * ex: ts=8 sts=4 et
 */
