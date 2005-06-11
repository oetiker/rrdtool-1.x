/*
 * tclrrd.c -- A TCL interpreter extension to access the RRD library.
 *
 * Copyright (c) 1999,2000 Frank Strauss, Technical University of Braunschweig.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * $Id$
 */



#include <time.h>
#include <tcl.h>
#include <rrd_tool.h>
#include <rrd_format.h>

extern int Tclrrd_Init(Tcl_Interp *interp, int safe);

extern int __getopt_initialized;


/*
 * some rrd_XXX() functions might modify the argv strings passed to it.
 * Hence, we need to do some preparation before
 * calling the rrd library functions.
 */
static char ** getopt_init(argc, argv)
    int argc;
    char *argv[];
{
    char **argv2;
    int i;
    
    argv2 = calloc(argc, sizeof(char *));
    for (i = 0; i < argc; i++) {
	argv2[i] = strdup(argv[i]);
    }
    return argv2;
}

static void getopt_cleanup(argc, argv2)
    int argc;
    char *argv2[];
{
    int i;
    
    for (i = 0; i < argc; i++) {
	free(argv2[i]);
    }
    free(argv2);
}



static int
Rrd_Create(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    char **argv2;

    argv2 = getopt_init(argc, argv);
    rrd_create(argc, argv2);
    getopt_cleanup(argc, argv2);
    
    if (rrd_test_error()) {
	Tcl_AppendResult(interp, "RRD Error: ",
			 rrd_get_error(), (char *) NULL);
        rrd_clear_error();
	return TCL_ERROR;
    }

    return TCL_OK;
}



static int
Rrd_Dump(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    rrd_dump(argc, argv2);
    getopt_cleanup(argv, argv2);

    /* NOTE: rrd_dump() writes to stdout. No interaction with TCL. */

    if (rrd_test_error()) {
	Tcl_AppendResult(interp, "RRD Error: ",
			 rrd_get_error(), (char *) NULL);
        rrd_clear_error();
	return TCL_ERROR;
    }

    return TCL_OK;
}



static int
Rrd_Last(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    time_t t;
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    t = rrd_last(argc, argv2);
    getopt_cleanup(argv, argv2);


    if (rrd_test_error()) {
	Tcl_AppendResult(interp, "RRD Error: ",
			 rrd_get_error(), (char *) NULL);
        rrd_clear_error();
	return TCL_ERROR;
    }

    Tcl_SetIntObj(Tcl_GetObjResult(interp), t);

    return TCL_OK;
}



static int
Rrd_Update(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    rrd_update(argc, argv2);
    getopt_cleanup(argv, argv2);

    if (rrd_test_error()) {
	Tcl_AppendResult(interp, "RRD Error: ",
			 rrd_get_error(), (char *) NULL);
        rrd_clear_error();
	return TCL_ERROR;
    }

    return TCL_OK;
}



static int
Rrd_Fetch(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    time_t start, end, j;
    unsigned long step, ds_cnt, i, ii;
    rrd_value_t *data, *datai;
    char **ds_namv;
    Tcl_Obj *listPtr;
    char s[30];
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    if (rrd_fetch(argc, argv2, &start, &end, &step,
		  &ds_cnt, &ds_namv, &data) != -1) {
        datai = data;
        listPtr = Tcl_GetObjResult(interp);
        for (j = start; j <= end; j += step) {
            for (ii = 0; ii < ds_cnt; ii++) {
		sprintf(s, "%.2f", *(datai++));
                Tcl_ListObjAppendElement(interp, listPtr,
					 Tcl_NewStringObj(s, -1));
            }
        }
        for (i=0; i<ds_cnt; i++) free(ds_namv[i]);
        free(ds_namv);
        free(data);
    }
    getopt_cleanup(argv, argv2);

    if (rrd_test_error()) {
	Tcl_AppendResult(interp, "RRD Error: ",
			 rrd_get_error(), (char *) NULL);
        rrd_clear_error();
	return TCL_ERROR;
    }

    return TCL_OK;
}



static int
Rrd_Graph(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    char **calcpr;
    int xsize, ysize;
    double ymin, ymax;
    Tcl_Obj *listPtr;
    char **argv2;
    
    calcpr = NULL;

    argv2 = getopt_init(argc, argv);
    if (rrd_graph(argc, argv2, &calcpr, &xsize, &ysize, NULL, &ymin, &ymax) != -1 ) {
        listPtr = Tcl_GetObjResult(interp);
        Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(xsize));
        Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(ysize));
        if (calcpr) {
#if 0
	    int i;
	    
            for(i = 0; calcpr[i]; i++){
                printf("%s\n", calcpr[i]);
                free(calcpr[i]);
            } 
#endif
            free(calcpr);
        }
    }
    getopt_cleanup(argv, argv2);

    if (rrd_test_error()) {
	Tcl_AppendResult(interp, "RRD Error: ",
			 rrd_get_error(), (char *) NULL);
        rrd_clear_error();
	return TCL_ERROR;
    }

    return TCL_OK;
}



static int
Rrd_Tune(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    rrd_tune(argc, argv2);
    getopt_cleanup(argv, argv2);

    if (rrd_test_error()) {
	Tcl_AppendResult(interp, "RRD Error: ",
			 rrd_get_error(), (char *) NULL);
        rrd_clear_error();
	return TCL_ERROR;
    }

    return TCL_OK;
}



static int
Rrd_Resize(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    rrd_resize(argc, argv2);
    getopt_cleanup(argv, argv2);

    if (rrd_test_error()) {
	Tcl_AppendResult(interp, "RRD Error: ",
			 rrd_get_error(), (char *) NULL);
        rrd_clear_error();
	return TCL_ERROR;
    }

    return TCL_OK;
}



static int
Rrd_Restore(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char *argv[];
{
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    rrd_restore(argc, argv2);
    getopt_cleanup(argv, argv2);

    if (rrd_test_error()) {
	Tcl_AppendResult(interp, "RRD Error: ",
			 rrd_get_error(), (char *) NULL);
        rrd_clear_error();
	return TCL_ERROR;
    }

    return TCL_OK;
}



/*
 * The following structure defines the commands in the Rrd extension.
 */

typedef struct {
    char *name;			/* Name of the command. */
    Tcl_CmdProc *proc;		/* Procedure for command. */
} CmdInfo;

static CmdInfo rrdCmds[] = {
    { "Rrd::create",	Rrd_Create		},
    { "Rrd::dump",	Rrd_Dump		},
    { "Rrd::last",	Rrd_Last		},
    { "Rrd::update",	Rrd_Update		},
    { "Rrd::fetch",	Rrd_Fetch		},
    { "Rrd::graph",	Rrd_Graph		},
    { "Rrd::tune",	Rrd_Tune		},
    { "Rrd::resize",	Rrd_Resize		},
    { "Rrd::restore",	Rrd_Restore		},
    { (char *) NULL,	(Tcl_CmdProc *) NULL	}
};



int
Tclrrd_Init(interp, safe)
    Tcl_Interp *interp;
    int safe;
{ 
    CmdInfo *cmdInfoPtr;
    Tcl_CmdInfo info;

    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 1) == NULL) {
        return TCL_ERROR;
    }

    Tcl_SetVar2(interp, "rrd", "version", VERSION, TCL_GLOBAL_ONLY);

    for (cmdInfoPtr = rrdCmds; cmdInfoPtr->name != NULL; cmdInfoPtr++) {
	/*
	 * Check if the command already exists and return an error
	 * to ensure we detect name clashes while loading the Rrd
	 * extension.
	 */
	if (Tcl_GetCommandInfo(interp, cmdInfoPtr->name, &info)) {
	    Tcl_AppendResult(interp, "command \"", cmdInfoPtr->name,
			     "\" already exists", (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_CreateCommand(interp, cmdInfoPtr->name, cmdInfoPtr->proc,
		          (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    }

    if (Tcl_PkgProvide(interp, "Rrd", VERSION) != TCL_OK) {
	return TCL_ERROR;
    }

    return TCL_OK;
}
