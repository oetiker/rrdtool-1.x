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



#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <tcl.h>
#include <rrd_tool.h>
#include <rrd_format.h>

extern int Tclrrd_Init(Tcl_Interp *interp);
extern int Tclrrd_SafeInit(Tcl_Interp *interp);


/*
 * some rrd_XXX() functions might modify the argv strings passed to it.
 * Hence, we need to do some preparation before
 * calling the rrd library functions.
 */
static char ** getopt_init(int argc, CONST84 char *argv[])
{
    char **argv2;
    int i;
    
    argv2 = calloc(argc, sizeof(char *));
    for (i = 0; i < argc; i++) {
	argv2[i] = strdup(argv[i]);
    }
    return argv2;
}

static void getopt_cleanup(int argc, char **argv2)
{
    int i;
    
    for (i = 0; i < argc; i++) {
	free(argv2[i]);
    }
    free(argv2);
}



static int
Rrd_Create(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char *argv[])
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
Rrd_Dump(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char *argv[])
{
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    rrd_dump(argc, argv2);
    getopt_cleanup(argc, argv2);

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
Rrd_Last(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char *argv[])
{
    time_t t;
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    t = rrd_last(argc, argv2);
    getopt_cleanup(argc, argv2);


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
Rrd_Update(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char *argv[])
{
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    rrd_update(argc, argv2);
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
Rrd_Fetch(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char *argv[])
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
Rrd_Graph(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char *argv[])
{
    Tcl_Channel channel;
    int mode, fd2;
    ClientData fd1;
    FILE *stream = NULL;
    char **calcpr = NULL;
    int rc, xsize, ysize;
    double ymin, ymax;
    char dimensions[50];
    char **argv2;
    CONST84 char *save;
    
    /*
     * If the "filename" is a Tcl fileID, then arrange for rrd_graph() to write to
     * that file descriptor.  Will this work with windoze?  I have no idea.
     */
    if ((channel = Tcl_GetChannel(interp, argv[1], &mode)) != NULL) {
	/*
	 * It >is< a Tcl fileID
	 */
	if (!(mode & TCL_WRITABLE)) {
	    Tcl_AppendResult(interp, "channel \"", argv[1],
		"\" wasn't opened for writing", (char *) NULL);
	    return TCL_ERROR;
	}
	/*
	 * Must flush channel to make sure any buffered data is written before
	 * rrd_graph() writes to the stream
	 */
	if (Tcl_Flush(channel) != TCL_OK) {
	    Tcl_AppendResult(interp, "flush failed for \"", argv[1], "\": ",
		strerror(Tcl_GetErrno()), (char *) NULL);
	    return TCL_ERROR;
	}
	if (Tcl_GetChannelHandle(channel, TCL_WRITABLE, &fd1) != TCL_OK) {
	    Tcl_AppendResult(interp, "cannot get file descriptor associated with \"",
		argv[1], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	/*
	 * Must dup() file descriptor so we can fclose(stream), otherwise the fclose()
	 * would close Tcl's file descriptor
	 */
	if ((fd2 = dup((int)fd1)) == -1) {
	    Tcl_AppendResult(interp, "dup() failed for file descriptor associated with \"",
		argv[1], "\": ", strerror(errno), (char *) NULL);
	    return TCL_ERROR;
	}
	/*
	 * rrd_graph() wants a FILE*
	 */
	if ((stream = fdopen(fd2, "wb")) == NULL) {
	    Tcl_AppendResult(interp, "fdopen() failed for file descriptor associated with \"",
		argv[1], "\": ", strerror(errno), (char *) NULL);
	    close(fd2);		/* plug potential file descriptor leak */
	    return TCL_ERROR;
	}

	save = argv[1];
	argv[1] = "-";
	argv2 = getopt_init(argc, argv);
	argv[1] = save;
    } else {
	Tcl_ResetResult(interp);	/* clear error from Tcl_GetChannel() */
	argv2 = getopt_init(argc, argv);
    }

    rc = rrd_graph(argc, argv2, &calcpr, &xsize, &ysize, stream, &ymin, &ymax);
    getopt_cleanup(argc, argv2);

    if (stream != NULL)
	fclose(stream);		/* plug potential malloc & file descriptor leak */

    if (rc != -1) {
        sprintf(dimensions, "%d %d", xsize, ysize);
        Tcl_AppendResult(interp, dimensions, (char *) NULL);
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

    if (rrd_test_error()) {
	Tcl_AppendResult(interp, "RRD Error: ",
			 rrd_get_error(), (char *) NULL);
        rrd_clear_error();
	return TCL_ERROR;
    }

    return TCL_OK;
}



static int
Rrd_Tune(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char *argv[])
{
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    rrd_tune(argc, argv2);
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
Rrd_Resize(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char *argv[])
{
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    rrd_resize(argc, argv2);
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
Rrd_Restore(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char *argv[])
{
    char **argv2;
    
    argv2 = getopt_init(argc, argv);
    rrd_restore(argc, argv2);
    getopt_cleanup(argc, argv2);

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
    int hide;			/* Hide if safe interpreter */
} CmdInfo;

static CmdInfo rrdCmds[] = {
    { "Rrd::create",	Rrd_Create,	1 },
    { "Rrd::dump",	Rrd_Dump,	0 },
    { "Rrd::last",	Rrd_Last,	0 },
    { "Rrd::update",	Rrd_Update,	1 },
    { "Rrd::fetch",	Rrd_Fetch,	0 },
    { "Rrd::graph",	Rrd_Graph,	1 }, /* Due to RRD's API, a safe
						interpreter cannot create
						a graph since it writes to
					        a filename supplied by the
					        caller */
    { "Rrd::tune",	Rrd_Tune,	1 },
    { "Rrd::resize",	Rrd_Resize,	1 },
    { "Rrd::restore",	Rrd_Restore,	1 },
    { (char *) NULL,	(Tcl_CmdProc *) NULL, 0	}
};



static int
init(Tcl_Interp *interp, int safe)
{ 
    CmdInfo *cmdInfoPtr;
    Tcl_CmdInfo info;

    if ( Tcl_InitStubs(interp,TCL_VERSION,0) == NULL )
	return TCL_ERROR;

    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 1) == NULL) {
        return TCL_ERROR;
    }

    /*
     * Why a global array?  In keeping with the Rrd:: namespace, why
     * not simply create a normal variable Rrd::version and set it?
     */
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
	if (safe && cmdInfoPtr->hide) {
#if 0
	    /*
	     * Turns out the one cannot hide a command in a namespace
	     * due to a limitation of Tcl, one can only hide global
	     * commands.  Thus, if we created the commands without
	     * the Rrd:: namespace in a safe interpreter, then the
	     * "unsafe" commands could be hidden -- which would allow
	     * an owning interpreter either un-hiding them or doing
	     * an "interp invokehidden".  If the Rrd:: namespace is
	     * used, then it's still possible for the owning interpreter
	     * to fake out the missing commands:
	     *
	     *   # Make all Rrd::* commands available in master interperter
	     *   package require Rrd
	     *   set safe [interp create -safe]
	     *   # Make safe Rrd::* commands available in safe interperter
	     *   interp invokehidden $safe -global load ./tclrrd1.2.11.so
	     *   # Provide the safe interpreter with the missing commands
	     *   $safe alias Rrd::update do_update $safe
	     *   proc do_update {which_interp $args} {
	     *     # Do some checking maybe...
	     *       :
	     *     return [eval Rrd::update $args]
	     *   }
	     *
	     * Our solution for now is to just not create the "unsafe"
	     * commands in a safe interpreter.
	     */
	    if (Tcl_HideCommand(interp, cmdInfoPtr->name, cmdInfoPtr->name) != TCL_OK)
		return TCL_ERROR;
#endif
	}
	else
	    Tcl_CreateCommand(interp, cmdInfoPtr->name, cmdInfoPtr->proc,
		          (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    }

    if (Tcl_PkgProvide(interp, "Rrd", VERSION) != TCL_OK) {
	return TCL_ERROR;
    }

    return TCL_OK;
}

int
Tclrrd_Init(Tcl_Interp *interp)
{ 
  return init(interp, 0);
}

/*
 * See the comments above and note how few commands are considered "safe"...
 * Using rrdtool in a safe interpreter has very limited functionality.  It's
 * tempting to just return TCL_ERROR and forget about it.
 */
int
Tclrrd_SafeInit(Tcl_Interp *interp)
{ 
  return init(interp, 1);
}
