/*****************************************************************************
 * RRDtool 1.3.8  Copyright by Tobi Oetiker, 1997-2009
 *****************************************************************************
 * rrd_tool.c  Startup wrapper
 *****************************************************************************/

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__) && !defined(HAVE_CONFIG_H)
#include "../win32/config.h"
#include <stdlib.h>
#include <sys/stat.h>
#else
#ifdef HAVE_CONFIG_H
#include "../rrd_config.h"
#endif
#endif

#include "rrd_tool.h"
#include "rrd_xport.h"
#include "rrd_i18n.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

void      PrintUsage(
    char *cmd);
int       CountArgs(
    char *aLine);
int       CreateArgs(
    char *,
    char *,
    int,
    char **);
int       HandleInputLine(
    int,
    char **,
    FILE *);
int       RemoteMode = 0;
int       ChangeRoot = 0;

#define TRUE		1
#define FALSE		0
#define MAX_LENGTH	10000


void PrintUsage(
    char *cmd)
{

    const char *help_main =
        N_("RRDtool %s"
           "  Copyright 1997-2009 by Tobias Oetiker <tobi@oetiker.ch>\n"
           "               Compiled %s %s\n\n"
           "Usage: rrdtool [options] command command_options\n\n");

    const char *help_list =
        N_
        ("Valid commands: create, update, updatev, graph, graphv,  dump, restore,\n"
         "\t\tlast, lastupdate, first, info, fetch, tune,\n"
         "\t\tresize, xport\n\n");

    const char *help_listremote =
        N_("Valid remote commands: quit, ls, cd, mkdir, pwd\n\n");


    const char *help_create =
        N_("* create - create a new RRD\n\n"
           "\trrdtool create filename [--start|-b start time]\n"
           "\t\t[--step|-s step]\n"
           "\t\t[DS:ds-name:DST:dst arguments]\n"
           "\t\t[RRA:CF:cf arguments]\n\n");

    const char *help_dump =
        N_("* dump - dump an RRD to XML\n\n"
           "\trrdtool dump filename.rrd >filename.xml\n\n");

    const char *help_info =
        N_("* info - returns the configuration and status of the RRD\n\n"
           "\trrdtool info filename.rrd\n\n");

    const char *help_restore =
        N_("* restore - restore an RRD file from its XML form\n\n"
           "\trrdtool restore [--range-check|-r] [--force-overwrite|-f] filename.xml filename.rrd\n\n");

    const char *help_last =
        N_("* last - show last update time for RRD\n\n"
           "\trrdtool last filename.rrd\n\n");

    const char *help_lastupdate =
        N_("* lastupdate - returns the most recent datum stored for\n"
           "  each DS in an RRD\n\n" "\trrdtool lastupdate filename.rrd\n\n");

    const char *help_first =
        N_("* first - show first update time for RRA within an RRD\n\n"
           "\trrdtool first filename.rrd [--rraindex number]\n\n");

    const char *help_update =
        N_("* update - update an RRD\n\n"
           "\trrdtool update filename\n"
           "\t\t--template|-t ds-name:ds-name:...\n"
           "\t\ttime|N:value[:value...]\n\n"
           "\t\tat-time@value[:value...]\n\n"
           "\t\t[ time:value[:value...] ..]\n\n");

    const char *help_updatev =
        N_("* updatev - a verbose version of update\n"
           "\treturns information about values, RRAs, and datasources updated\n\n"
           "\trrdtool updatev filename\n"
           "\t\t--template|-t ds-name:ds-name:...\n"
           "\t\ttime|N:value[:value...]\n\n"
           "\t\tat-time@value[:value...]\n\n"
           "\t\t[ time:value[:value...] ..]\n\n");

    const char *help_fetch =
        N_("* fetch - fetch data out of an RRD\n\n"
           "\trrdtool fetch filename.rrd CF\n"
           "\t\t[-r|--resolution resolution]\n"
           "\t\t[-s|--start start] [-e|--end end]\n\n");

/* break up very large strings (help_graph, help_tune) for ISO C89 compliance*/

    const char *help_graph0 =
        N_("* graph - generate a graph from one or several RRD\n\n"
           "\trrdtool graph filename [-s|--start seconds] [-e|--end seconds]\n");
    const char *help_graphv0 =
        N_("* graphv - generate a graph from one or several RRD\n"
           "           with meta data printed before the graph\n\n"
           "\trrdtool graphv filename [-s|--start seconds] [-e|--end seconds]\n");
    const char *help_graph1 =
        N_("\t\t[-x|--x-grid x-axis grid and label]\n"
           "\t\t[-Y|--alt-y-grid]\n"
           "\t\t[-y|--y-grid y-axis grid and label]\n"
           "\t\t[-v|--vertical-label string] [-w|--width pixels]\n"
           "\t\t[--right-axis scale:shift] [--right-axis-label label]\n"
           "\t\t[--right-axis-format format]\n"          
           "\t\t[-h|--height pixels] [-o|--logarithmic]\n"
           "\t\t[-u|--upper-limit value] [-z|--lazy]\n"
           "\t\t[-l|--lower-limit value] [-r|--rigid]\n"
           "\t\t[-g|--no-legend] [--full-size-mode]\n"
           "\t\t[-F|--force-rules-legend]\n" "\t\t[-j|--only-graph]\n");
    const char *help_graph2 =
        N_("\t\t[-n|--font FONTTAG:size:font]\n"
           "\t\t[-m|--zoom factor]\n"
           "\t\t[-A|--alt-autoscale]\n"
           "\t\t[-M|--alt-autoscale-max]\n"
           "\t\t[-G|--graph-render-mode {normal,mono}]\n"
           "\t\t[-R|--font-render-mode {normal,light,mono}]\n"
           "\t\t[-B|--font-smoothing-threshold size]\n"
           "\t\t[-T|--tabwidth width]\n"
           "\t\t[-E|--slope-mode]\n"
           "\t\t[-N|--no-gridfit]\n"
           "\t\t[-X|--units-exponent value]\n"
           "\t\t[-L|--units-length value]\n"
           "\t\t[-S|--step seconds]\n"
           "\t\t[-f|--imginfo printfstr]\n"
           "\t\t[-a|--imgformat PNG]\n"
           "\t\t[-c|--color COLORTAG#rrggbb[aa]] [-t|--title string]\n"
           "\t\t[-W|--watermark string]\n"
           "\t\t[DEF:vname=rrd:ds-name:CF]\n");
    const char *help_graph3 =
        N_("\t\t[CDEF:vname=rpn-expression]\n"
           "\t\t[VDEF:vdefname=rpn-expression]\n"
           "\t\t[PRINT:vdefname:format]\n"
           "\t\t[GPRINT:vdefname:format]\n" "\t\t[COMMENT:text]\n"
           "\t\t[SHIFT:vname:offset]\n"
           "\t\t[TICK:vname#rrggbb[aa][:[fraction][:legend]]]\n"
           "\t\t[HRULE:value#rrggbb[aa][:legend]]\n"
           "\t\t[VRULE:value#rrggbb[aa][:legend]]\n"
           "\t\t[LINE[width]:vname[#rrggbb[aa][:[legend][:STACK]]]]\n"
           "\t\t[AREA:vname[#rrggbb[aa][:[legend][:STACK]]]]\n"
           "\t\t[PRINT:vname:CF:format] (deprecated)\n"
           "\t\t[GPRINT:vname:CF:format] (deprecated)\n"
           "\t\t[STACK:vname[#rrggbb[aa][:legend]]] (deprecated)\n\n");
    const char *help_tune1 =
        N_(" * tune -  Modify some basic properties of an RRD\n\n"
           "\trrdtool tune filename\n"
           "\t\t[--heartbeat|-h ds-name:heartbeat]\n"
           "\t\t[--data-source-type|-d ds-name:DST]\n"
           "\t\t[--data-source-rename|-r old-name:new-name]\n"
           "\t\t[--minimum|-i ds-name:min] [--maximum|-a ds-name:max]\n"
           "\t\t[--deltapos scale-value] [--deltaneg scale-value]\n"
           "\t\t[--failure-threshold integer]\n"
           "\t\t[--window-length integer]\n"
           "\t\t[--alpha adaptation-parameter]\n");
    const char *help_tune2 =
        N_(" * tune -  Modify some basic properties of an RRD\n\n"
           "\t\t[--beta adaptation-parameter]\n"
           "\t\t[--gamma adaptation-parameter]\n"
           "\t\t[--gamma-deviation adaptation-parameter]\n"
           "\t\t[--aberrant-reset ds-name]\n\n");
    const char *help_resize =
        N_
        (" * resize - alter the length of one of the RRAs in an RRD\n\n"
         "\trrdtool resize filename rranum GROW|SHRINK rows\n\n");
    const char *help_xport =
        N_("* xport - generate XML dump from one or several RRD\n\n"
           "\trrdtool xport [-s|--start seconds] [-e|--end seconds]\n"
           "\t\t[-m|--maxrows rows]\n" "\t\t[--step seconds]\n"
           "\t\t[--enumds]\n" "\t\t[DEF:vname=rrd:ds-name:CF]\n"
           "\t\t[CDEF:vname=rpn-expression]\n"
           "\t\t[XPORT:vname:legend]\n\n");
    const char *help_quit =
        N_(" * quit - closing a session in remote mode\n\n"
           "\trrdtool quit\n\n");
    const char *help_ls =
        N_(" * ls - lists all *.rrd files in current directory\n\n"
           "\trrdtool ls\n\n");
    const char *help_cd =
        N_(" * cd - changes the current directory\n\n"
           "\trrdtool cd new directory\n\n");
    const char *help_mkdir =
        N_(" * mkdir - creates a new directory\n\n"
           "\trrdtool mkdir newdirectoryname\n\n");
    const char *help_pwd =
        N_(" * pwd - returns the current working directory\n\n"
           "\trrdtool pwd\n\n");
    const char *help_lic =
        N_("RRDtool is distributed under the Terms of the GNU General\n"
           "Public License Version 2. (www.gnu.org/copyleft/gpl.html)\n\n"
           "For more information read the RRD manpages\n\n");
    enum { C_NONE, C_CREATE, C_DUMP, C_INFO, C_RESTORE, C_LAST,
        C_LASTUPDATE, C_FIRST, C_UPDATE, C_FETCH, C_GRAPH, C_GRAPHV,
        C_TUNE,
        C_RESIZE, C_XPORT, C_QUIT, C_LS, C_CD, C_MKDIR, C_PWD,
        C_UPDATEV
    };
    int       help_cmd = C_NONE;

    if (cmd) {
        if (!strcmp(cmd, "create"))
            help_cmd = C_CREATE;
        else if (!strcmp(cmd, "dump"))
            help_cmd = C_DUMP;
        else if (!strcmp(cmd, "info"))
            help_cmd = C_INFO;
        else if (!strcmp(cmd, "restore"))
            help_cmd = C_RESTORE;
        else if (!strcmp(cmd, "last"))
            help_cmd = C_LAST;
        else if (!strcmp(cmd, "lastupdate"))
            help_cmd = C_LASTUPDATE;
        else if (!strcmp(cmd, "first"))
            help_cmd = C_FIRST;
        else if (!strcmp(cmd, "update"))
            help_cmd = C_UPDATE;
        else if (!strcmp(cmd, "updatev"))
            help_cmd = C_UPDATEV;
        else if (!strcmp(cmd, "fetch"))
            help_cmd = C_FETCH;
        else if (!strcmp(cmd, "graph"))
            help_cmd = C_GRAPH;
        else if (!strcmp(cmd, "graphv"))
            help_cmd = C_GRAPHV;
        else if (!strcmp(cmd, "tune"))
            help_cmd = C_TUNE;
        else if (!strcmp(cmd, "resize"))
            help_cmd = C_RESIZE;
        else if (!strcmp(cmd, "xport"))
            help_cmd = C_XPORT;
        else if (!strcmp(cmd, "quit"))
            help_cmd = C_QUIT;
        else if (!strcmp(cmd, "ls"))
            help_cmd = C_LS;
        else if (!strcmp(cmd, "cd"))
            help_cmd = C_CD;
        else if (!strcmp(cmd, "mkdir"))
            help_cmd = C_MKDIR;
        else if (!strcmp(cmd, "pwd"))
            help_cmd = C_PWD;
    }
    fprintf(stdout, _(help_main), PACKAGE_VERSION, __DATE__, __TIME__);
    fflush(stdout);
    switch (help_cmd) {
    case C_NONE:
        fputs(_(help_list), stdout);
        if (RemoteMode) {
            fputs(_(help_listremote), stdout);
        }
        break;
    case C_CREATE:
        fputs(_(help_create), stdout);
        break;
    case C_DUMP:
        fputs(_(help_dump), stdout);
        break;
    case C_INFO:
        fputs(_(help_info), stdout);
        break;
    case C_RESTORE:
        fputs(_(help_restore), stdout);
        break;
    case C_LAST:
        fputs(_(help_last), stdout);
        break;
    case C_LASTUPDATE:
        fputs(_(help_lastupdate), stdout);
        break;
    case C_FIRST:
        fputs(_(help_first), stdout);
        break;
    case C_UPDATE:
        fputs(_(help_update), stdout);
        break;
    case C_UPDATEV:
        fputs(_(help_updatev), stdout);
        break;
    case C_FETCH:
        fputs(_(help_fetch), stdout);
        break;
    case C_GRAPH:
        fputs(_(help_graph0), stdout);
        fputs(_(help_graph1), stdout);
        fputs(_(help_graph2), stdout);
        fputs(_(help_graph3), stdout);
        break;
    case C_GRAPHV:
        fputs(_(help_graphv0), stdout);
        fputs(_(help_graph1), stdout);
        fputs(_(help_graph2), stdout);
        fputs(_(help_graph3), stdout);
        break;
    case C_TUNE:
        fputs(_(help_tune1), stdout);
        fputs(_(help_tune2), stdout);
        break;
    case C_RESIZE:
        fputs(_(help_resize), stdout);
        break;
    case C_XPORT:
        fputs(_(help_xport), stdout);
        break;
    case C_QUIT:
        fputs(_(help_quit), stdout);
        break;
    case C_LS:
        fputs(_(help_ls), stdout);
        break;
    case C_CD:
        fputs(_(help_cd), stdout);
        break;
    case C_MKDIR:
        fputs(_(help_mkdir), stdout);
        break;
    case C_PWD:
        fputs(_(help_pwd), stdout);
        break;
    }
    fputs(_(help_lic), stdout);
}

static char *fgetslong(
    char **aLinePtr,
    FILE * stream)
{
    char     *linebuf;
    size_t    bufsize = MAX_LENGTH;
    int       eolpos = 0;

    if (feof(stream))
        return *aLinePtr = 0;
    if (!(linebuf = malloc(bufsize))) {
        perror("fgetslong: malloc");
        exit(1);
    }
    linebuf[0] = '\0';
    while (fgets(linebuf + eolpos, MAX_LENGTH, stream)) {
        eolpos += strlen(linebuf + eolpos);
        if (linebuf[eolpos - 1] == '\n')
            return *aLinePtr = linebuf;
        bufsize += MAX_LENGTH;
        if (!(linebuf = realloc(linebuf, bufsize))) {
            free(linebuf);
            perror("fgetslong: realloc");
            exit(1);
        }
    }
    if (linebuf[0]){
        return  *aLinePtr = linebuf;
    }
    free(linebuf);
    return *aLinePtr = 0;
}

int main(
    int argc,
    char *argv[])
{
    char    **myargv;
    char     *aLine;
    char     *firstdir = "";

#ifdef MUST_DISABLE_SIGFPE
    signal(SIGFPE, SIG_IGN);
#endif
#ifdef MUST_DISABLE_FPMASK
    fpsetmask(0);
#endif
#ifdef HAVE_LOCALE_H
    setlocale(LC_ALL, "");
#endif

#if defined(HAVE_LIBINTL_H) && defined(BUILD_LIBINTL)
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
#endif
    if (argc == 1) {
        PrintUsage("");
        return 0;
    }

    if (((argc == 2) || (argc == 3)) && !strcmp("-", argv[1])) {
#if HAVE_GETRUSAGE
        struct rusage myusage;
        struct timeval starttime;
        struct timeval currenttime;

        gettimeofday(&starttime, NULL);
#endif
        RemoteMode = 1;
        if ((argc == 3) && strcmp("", argv[2])) {

            if (
#ifdef HAVE_GETUID
                   getuid()
#else
                   1
#endif
                   == 0) {

#ifdef HAVE_CHROOT
                if (chroot(argv[2]) != 0){
                    fprintf(stderr, "ERROR: chroot %s: %s\n", argv[2],rrd_strerror(errno));
                    exit(errno);
                }
                ChangeRoot = 1;
                firstdir = "/";
#else
                fprintf(stderr,
                        "ERROR: change root is not supported by your OS "
                        "or at least by this copy of rrdtool\n");
                exit(1);
#endif
            } else {
                firstdir = argv[2];
            }
        }
        if (strcmp(firstdir, "")) {
            if (chdir(firstdir) != 0){
                fprintf(stderr, "ERROR: chdir %s %s\n", firstdir,rrd_strerror(errno));
                exit(errno);
            }
        }

        while (fgetslong(&aLine, stdin)) {
            if ((argc = CountArgs(aLine)) == 0) {
                free(aLine);
                printf("ERROR: not enough arguments\n");
            }
            if ((myargv = (char **) malloc((argc + 1) *
                                           sizeof(char *))) == NULL) {
                perror("malloc");
                exit(1);
            }
            if ((argc = CreateArgs(argv[0], aLine, argc, myargv)) < 0) {
                free(aLine);
                free(myargv);
                printf("ERROR: creating arguments\n");
            } else {
                int       ret = HandleInputLine(argc, myargv, stdout);

                free(myargv);
                if (ret == 0) {
#if HAVE_GETRUSAGE
                    getrusage(RUSAGE_SELF, &myusage);
                    gettimeofday(&currenttime, NULL);
                    printf("OK u:%1.2f s:%1.2f r:%1.2f\n",
                           (double) myusage.ru_utime.tv_sec +
                           (double) myusage.ru_utime.tv_usec / 1000000.0,
                           (double) myusage.ru_stime.tv_sec +
                           (double) myusage.ru_stime.tv_usec / 1000000.0,
                           (double) (currenttime.tv_sec - starttime.tv_sec)
                           + (double) (currenttime.tv_usec -
                                       starttime.tv_usec)
                           / 1000000.0);
#else
                    printf("OK\n");
#endif
                }
            }
            fflush(stdout); /* this is important for pipes to work */
            free(aLine);
        }
    } else if (argc == 2) {
        PrintUsage(argv[1]);
        exit(0);
    } else if (argc == 3 && !strcmp(argv[1], "help")) {
        PrintUsage(argv[2]);
        exit(0);
    } else {
        exit(HandleInputLine(argc, argv, stderr));
    }
    return 0;
}

/* HandleInputLine is NOT thread safe - due to readdir issues,
   resolving them portably is not really simple. */
int HandleInputLine(
    int argc,
    char **argv,
    FILE * out)
{
#if defined(HAVE_OPENDIR) && defined (HAVE_READDIR)
    DIR      *curdir;   /* to read current dir with ls */
    struct dirent *dent;
#endif
#if defined(HAVE_SYS_STAT_H)
    struct stat st;
#endif
    char     *cwd;      /* To hold current working dir on call to pwd */

    /* Reset errno to 0 before we start.
     */
    if (RemoteMode) {
        if (argc > 1 && strcmp("quit", argv[1]) == 0) {
            if (argc > 2) {
                printf("ERROR: invalid parameter count for quit\n");
                return (1);
            }
            exit(0);
        }
#if defined(HAVE_OPENDIR) && defined(HAVE_READDIR) && defined(HAVE_CHDIR)
        if (argc > 1 && strcmp("cd", argv[1]) == 0) {
            if (argc > 3) {
                printf("ERROR: invalid parameter count for cd\n");
                return (1);
            }
#if ! defined(HAVE_CHROOT) || ! defined(HAVE_GETUID)
            if (getuid() == 0 && !ChangeRoot) {
                printf
                    ("ERROR: chdir security problem - rrdtool is running as "
                     "root but not chroot!\n");
                return (1);
            }
#endif
            if (chdir(argv[2]) != 0){
                printf("ERROR: chdir %s %s\n", argv[2], rrd_strerror(errno));
                return (1);
            }
            return (0);
        }
        if (argc > 1 && strcmp("pwd", argv[1]) == 0) {
            if (argc > 2) {
                printf("ERROR: invalid parameter count for pwd\n");
                return (1);
            }
            cwd = getcwd(NULL, MAXPATH);
            if (cwd == NULL) {
                printf("ERROR: getcwd %s\n", rrd_strerror(errno));
                return (1);
            }
            printf("%s\n", cwd);
            free(cwd);
            return (0);
        }
        if (argc > 1 && strcmp("mkdir", argv[1]) == 0) {
            if (argc > 3) {
                printf("ERROR: invalid parameter count for mkdir\n");
                return (1);
            }
#if ! defined(HAVE_CHROOT) || ! defined(HAVE_GETUID)
            if (getuid() == 0 && !ChangeRoot) {
                printf
                    ("ERROR: mkdir security problem - rrdtool is running as "
                     "root but not chroot!\n");
                return (1);
            }
#endif
            if(mkdir(argv[2], 0777)!=0){
                printf("ERROR: mkdir %s: %s\n", argv[2],rrd_strerror(errno));
                return (1);
            }
            return (0);
        }
        if (argc > 1 && strcmp("ls", argv[1]) == 0) {
            if (argc > 2) {
                printf("ERROR: invalid parameter count for ls\n");
                return (1);
            }
            if ((curdir = opendir(".")) != NULL) {
                while ((dent = readdir(curdir)) != NULL) {
                    if (!stat(dent->d_name, &st)) {
                        if (S_ISDIR(st.st_mode)) {
                            printf("d %s\n", dent->d_name);
                        }
                        if (strlen(dent->d_name) > 4 && S_ISREG(st.st_mode)) {
                            if (!strcmp
                                (dent->d_name + NAMLEN(dent) - 4, ".rrd")
                                || !strcmp(dent->d_name + NAMLEN(dent) - 4,
                                           ".RRD")) {
                                printf("- %s\n", dent->d_name);
                            }
                        }
                    }
                }
                closedir(curdir);
            } else {
                printf("ERROR: opendir .: %s\n", rrd_strerror(errno));
                return (errno);
            }
            return (0);
        }
#endif                          /* opendir and readdir */

    }
    if (argc < 3
        || strcmp("help", argv[1]) == 0
        || strcmp("--help", argv[1]) == 0
        || strcmp("-help", argv[1]) == 0
        || strcmp("-?", argv[1]) == 0 || strcmp("-h", argv[1]) == 0) {
        PrintUsage("");
        return 0;
    }

    if (strcmp("create", argv[1]) == 0)
        rrd_create(argc - 1, &argv[1]);
    else if (strcmp("dump", argv[1]) == 0)
        rrd_dump(argc - 1, &argv[1]);
    else if (strcmp("info", argv[1]) == 0 || strcmp("updatev", argv[1]) == 0) {
        rrd_info_t *data;

        if (strcmp("info", argv[1]) == 0)

            data = rrd_info(argc - 1, &argv[1]);
        else
            data = rrd_update_v(argc - 1, &argv[1]);
        rrd_info_print(data);
        rrd_info_free(data);
    }

    else if (strcmp("--version", argv[1]) == 0 ||
             strcmp("version", argv[1]) == 0 ||
             strcmp("v", argv[1]) == 0 ||
             strcmp("-v", argv[1]) == 0 || strcmp("-version", argv[1]) == 0)
        printf("RRDtool " PACKAGE_VERSION
               "  Copyright by Tobi Oetiker, 1997-2008 (%f)\n",
               rrd_version());
    else if (strcmp("restore", argv[1]) == 0)
        rrd_restore(argc - 1, &argv[1]);
    else if (strcmp("resize", argv[1]) == 0)
        rrd_resize(argc - 1, &argv[1]);
    else if (strcmp("last", argv[1]) == 0)
        printf("%ld\n", rrd_last(argc - 1, &argv[1]));
    else if (strcmp("lastupdate", argv[1]) == 0) {
        time_t    last_update;
        char    **ds_namv;
        char    **last_ds;
        unsigned long ds_cnt, i;

        if (rrd_lastupdate(argc - 1, &argv[1], &last_update,
                           &ds_cnt, &ds_namv, &last_ds) == 0) {
            for (i = 0; i < ds_cnt; i++)
                printf(" %s", ds_namv[i]);
            printf("\n\n");
            printf("%10lu:", last_update);
            for (i = 0; i < ds_cnt; i++) {
                printf(" %s", last_ds[i]);
                free(last_ds[i]);
                free(ds_namv[i]);
            }
            printf("\n");
            free(last_ds);
            free(ds_namv);
        }
    } else if (strcmp("first", argv[1]) == 0)
        printf("%ld\n", rrd_first(argc - 1, &argv[1]));
    else if (strcmp("update", argv[1]) == 0)
        rrd_update(argc - 1, &argv[1]);
    else if (strcmp("fetch", argv[1]) == 0) {
        time_t    start, end, ti;
        unsigned long step, ds_cnt, i, ii;
        rrd_value_t *data, *datai;
        char    **ds_namv;

        if (rrd_fetch
            (argc - 1, &argv[1], &start, &end, &step, &ds_cnt, &ds_namv,
             &data) != -1) {
            datai = data;
            printf("           ");
            for (i = 0; i < ds_cnt; i++)
                printf("%20s", ds_namv[i]);
            printf("\n\n");
            for (ti = start + step; ti <= end; ti += step) {
                printf("%10lu:", ti);
                for (ii = 0; ii < ds_cnt; ii++)
                    printf(" %0.10e", *(datai++));
                printf("\n");
            }
            for (i = 0; i < ds_cnt; i++)
                free(ds_namv[i]);
            free(ds_namv);
            free(data);
        }
    } else if (strcmp("xport", argv[1]) == 0) {
        int       xxsize;
        unsigned long int j = 0;
        time_t    start, end, ti;
        unsigned long step, col_cnt, row_cnt;
        rrd_value_t *data, *ptr;
        char    **legend_v;
        int       enumds = 0;
        int       i;
        size_t    vtag_s = strlen(COL_DATA_TAG) + 10;
        char     *vtag = malloc(vtag_s);

        for (i = 2; i < argc; i++) {
            if (strcmp("--enumds", argv[i]) == 0)
                enumds = 1;
        }

        if (rrd_xport
            (argc - 1, &argv[1], &xxsize, &start, &end, &step, &col_cnt,
             &legend_v, &data) != -1) {
            row_cnt = (end - start) / step;
            ptr = data;
            printf("<?xml version=\"1.0\" encoding=\"%s\"?>\n\n",
                   XML_ENCODING);
            printf("<%s>\n", ROOT_TAG);
            printf("  <%s>\n", META_TAG);
            printf("    <%s>%lu</%s>\n", META_START_TAG,
                   (unsigned long) start + step, META_START_TAG);
            printf("    <%s>%lu</%s>\n", META_STEP_TAG, step, META_STEP_TAG);
            printf("    <%s>%lu</%s>\n", META_END_TAG, (unsigned long) end,
                   META_END_TAG);
            printf("    <%s>%lu</%s>\n", META_ROWS_TAG, row_cnt,
                   META_ROWS_TAG);
            printf("    <%s>%lu</%s>\n", META_COLS_TAG, col_cnt,
                   META_COLS_TAG);
            printf("    <%s>\n", LEGEND_TAG);
            for (j = 0; j < col_cnt; j++) {
                char     *entry = NULL;

                entry = legend_v[j];
                printf("      <%s>%s</%s>\n", LEGEND_ENTRY_TAG, entry,
                       LEGEND_ENTRY_TAG);
                free(entry);
            }
            free(legend_v);
            printf("    </%s>\n", LEGEND_TAG);
            printf("  </%s>\n", META_TAG);
            printf("  <%s>\n", DATA_TAG);
            for (ti = start + step; ti <= end; ti += step) {
                printf("    <%s>", DATA_ROW_TAG);
                printf("<%s>%lu</%s>", COL_TIME_TAG, ti, COL_TIME_TAG);
                for (j = 0; j < col_cnt; j++) {
                    rrd_value_t newval = DNAN;

                    if (enumds == 1)

                        snprintf(vtag, vtag_s, "%s%lu", COL_DATA_TAG, j);
                    else
                        snprintf(vtag, vtag_s, "%s", COL_DATA_TAG);
                    newval = *ptr;
                    if (isnan(newval)) {
                        printf("<%s>NaN</%s>", vtag, vtag);
                    } else {
                        printf("<%s>%0.10e</%s>", vtag, newval, vtag);
                    };
                    ptr++;
                }
                printf("</%s>\n", DATA_ROW_TAG);
            }
            free(data);
            printf("  </%s>\n", DATA_TAG);
            printf("</%s>\n", ROOT_TAG);
        }
        free(vtag);
    } else if (strcmp("graph", argv[1]) == 0) {
        char    **calcpr;

#ifdef notused /*XXX*/
        const char *imgfile = argv[2];  /* rrd_graph changes argv pointer */
#endif
        int       xsize, ysize;
        double    ymin, ymax;
        int       i;
        int       tostdout = (strcmp(argv[2], "-") == 0);
        int       imginfo = 0;

        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--imginfo") == 0
                || strcmp(argv[i], "-f") == 0) {
                imginfo = 1;
                break;
            }
        }
        if (rrd_graph
            (argc - 1, &argv[1], &calcpr, &xsize, &ysize, NULL, &ymin,
             &ymax) != -1) {
            if (!tostdout && !imginfo)
                printf("%dx%d\n", xsize, ysize);
            if (calcpr) {
                for (i = 0; calcpr[i]; i++) {
                    if (!tostdout)
                        printf("%s\n", calcpr[i]);
                    free(calcpr[i]);
                }
                free(calcpr);
            }
        }

    } else if (strcmp("graphv", argv[1]) == 0) {
        rrd_info_t *grinfo = NULL;  /* 1 to distinguish it from the NULL that rrd_graph sends in */

        grinfo = rrd_graph_v(argc - 1, &argv[1]);
        if (grinfo) {
            rrd_info_print(grinfo);
            rrd_info_free(grinfo);
        }

    } else if (strcmp("tune", argv[1]) == 0)
        rrd_tune(argc - 1, &argv[1]);
    else {
        rrd_set_error("unknown function '%s'", argv[1]);
    }
    if (rrd_test_error()) {
        fprintf(out, "ERROR: %s\n", rrd_get_error());
        rrd_clear_error();
        return 1;
    }
    return (0);
}

int CountArgs(
    char *aLine)
{
    int       i = 0;
    int       aCount = 0;
    int       inarg = 0;

    while (aLine[i] == ' ')
        i++;
    while (aLine[i] != 0) {
        if ((aLine[i] == ' ') && inarg) {
            inarg = 0;
        }
        if ((aLine[i] != ' ') && !inarg) {
            inarg = 1;
            aCount++;
        }
        i++;
    }
    return aCount;
}

/*
 * CreateArgs - take a string (aLine) and tokenize
 */
int CreateArgs(
    char *pName,
    char *aLine,
    int argc,
    char **argv)
{
    char     *getP, *putP;
    char    **pargv = argv;
    char      Quote = 0;
    int       inArg = 0;
    int       len;

    len = strlen(aLine);
    /* remove trailing space and newlines */
    while (len && aLine[len] <= ' ') {
        aLine[len] = 0;
        len--;
    }
    /* sikp leading blanks */
    while (*aLine && *aLine <= ' ')
        aLine++;
    pargv[0] = pName;
    argc = 1;
    getP = aLine;
    putP = aLine;
    while (*getP) {
        switch (*getP) {
        case ' ':
            if (Quote) {
                *(putP++) = *getP;
            } else if (inArg) {
                *(putP++) = 0;
                inArg = 0;
            }
            break;
        case '"':
        case '\'':
            if (Quote != 0) {
                if (Quote == *getP)
                    Quote = 0;
                else {
                    *(putP++) = *getP;
                }
            } else {
                if (!inArg) {
                    pargv[argc++] = putP;
                    inArg = 1;
                }
                Quote = *getP;
            }
            break;
        default:
            if (!inArg) {
                pargv[argc++] = putP;
                inArg = 1;
            }
            *(putP++) = *getP;
            break;
        }
        getP++;
    }

    *putP = '\0';
    if (Quote)
        return -1;
    else
        return argc;
}
