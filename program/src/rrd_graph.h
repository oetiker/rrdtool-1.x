#ifndef _RRD_GRAPH_H
#define _RRD_GRAPH_H

#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#include "rrd_gfx.h"

#define MAX_VNAME_LEN 29
#define DEF_NAM_FMT "%29[-_A-Za-z0-9]"

#define ALTYGRID	0x01	/* use alternative y grid algorithm */
#define ALTAUTOSCALE	0x02	/* use alternative algorithm to find lower and upper bounds */
#define ALTAUTOSCALE_MAX 0x04	/* use alternative algorithm to find upper bounds */
#define NOLEGEND	0x08	/* use no legend */
#define NOMINOR         0x20    /* Turn off minor gridlines */
#define ONLY_GRAPH      0x24   /* use only graph */
#define FORCE_RULES_LEGEND	0x40	/* force printing of HRULE and VRULE legend */


enum tmt_en {TMT_SECOND=0,TMT_MINUTE,TMT_HOUR,TMT_DAY,
	     TMT_WEEK,TMT_MONTH,TMT_YEAR};

enum grc_en {GRC_CANVAS=0,GRC_BACK,GRC_SHADEA,GRC_SHADEB,
	     GRC_GRID,GRC_MGRID,GRC_FONT,GRC_FRAME,GRC_ARROW,__GRC_END__};

#define MGRIDWIDTH 0.6
#define GRIDWIDTH  0.4

enum gf_en {GF_PRINT=0,GF_GPRINT,GF_COMMENT,GF_HRULE,GF_VRULE,GF_LINE,
	    GF_AREA,GF_STACK,GF_TICK,
	    GF_DEF, GF_CDEF, GF_VDEF, GF_SHIFT,
	    GF_PART, GF_XPORT};

enum vdef_op_en {
		 VDEF_MAXIMUM	/* like the MAX in (G)PRINT */
		,VDEF_MINIMUM	/* like the MIN in (G)PRINT */
		,VDEF_AVERAGE	/* like the AVERAGE in (G)PRINT */
		,VDEF_PERCENT	/* Nth percentile */
		,VDEF_TOTAL	/* average multiplied by time */
		,VDEF_FIRST	/* first non-unknown value and time */
		,VDEF_LAST	/* last  non-unknown value and time */
		};
enum text_prop_en { TEXT_PROP_DEFAULT=0,   /* default settings */
	            TEXT_PROP_TITLE,       /* properties for the title */
		    TEXT_PROP_AXIS,        /* for the numbers next to the axis */
		    TEXT_PROP_UNIT,        /* for the vertical unit description */
		    TEXT_PROP_LEGEND,      /* fot the legend below the graph */
		    TEXT_PROP_LAST };

typedef struct text_prop_t {
  double       size;
  char *       font;
} text_prop_t;


typedef struct vdef_t {
    enum vdef_op_en	op;
    double		param;	/* parameter for function, if applicable */
    double		val;	/* resulting value */
    time_t		when;	/* timestamp, if applicable */
} vdef_t;

typedef struct xlab_t {
    long         minsec;       /* minimum sec per pix */
    enum tmt_en  gridtm;       /* grid interval in what ?*/
    long         gridst;       /* how many whats per grid*/
    enum tmt_en  mgridtm;      /* label interval in what ?*/
    long         mgridst;      /* how many whats per label*/
    enum tmt_en  labtm;        /* label interval in what ?*/
    long         labst;        /* how many whats per label*/
    long         precis;       /* label precision -> label placement*/
    char         *stst;        /* strftime string*/
} xlab_t;

typedef struct ygrid_scale_t {  /* y axis grid scaling info */
    double       gridstep;
    int          labfact;
    char         labfmt[64];
} ygrid_scale_t;

/* sensible y label intervals ...*/

typedef struct ylab_t {
    double   grid;    /* grid spacing */
    int      lfac[4]; /* associated label spacing*/
} ylab_t;


/* this structure describes the elements which can make up a graph.
   because they are quite diverse, not all elements will use all the
   possible parts of the structure. */
#ifdef HAVE_SNPRINTF
#define FMT_LEG_LEN 200
#else
#define FMT_LEG_LEN 2000
#endif

typedef  struct graph_desc_t {
    enum gf_en     gf;         /* graphing function */
    int            stack;      /* boolean */
    int            debug;      /* boolean */
    char           vname[MAX_VNAME_LEN+1];  /* name of the variable */
    long           vidx;       /* gdes reference */
    char           rrd[255];   /* name of the rrd_file containing data */
    char           ds_nam[DS_NAM_SIZE]; /* data source name */
    long           ds;         /* data source number */
    enum cf_en     cf;         /* consolidation function */
    enum cf_en     cf_reduce;  /* consolidation function for reduce_data() */
    gfx_color_t    col;        /* graph color */
    char  format[FMT_LEG_LEN+5]; /* format for PRINT AND GPRINT */
    char  legend[FMT_LEG_LEN+5]; /* legend*/
    double         leg_x,leg_y;  /* location of legend */   
    double         yrule;      /* value for y rule line and for VDEF */
    time_t         xrule;      /* time for x rule line and for VDEF */
    vdef_t         vf;         /* instruction for VDEF function */
    rpnp_t         *rpnp;     /* instructions for CDEF function */

    /* SHIFT implementation */
    int            shidx; /* gdes reference for offset (-1 --> constant) */
    time_t         shval; /* offset if shidx is -1 */
    time_t         shift; /* current shift applied */

    /* description of data fetched for the graph element */
    time_t         start,end; /* timestaps for first and last data element */
    unsigned long  step;      /* time between samples */
    unsigned long  ds_cnt; /* how many data sources are there in the fetch */
    long           data_first; /* first pointer to this data */
    char           **ds_namv; /* name of datasources  in the fetch. */
    rrd_value_t    *data; /* the raw data drawn from the rrd */
    rrd_value_t    *p_data; /* processed data, xsize elments */
    double         linewidth;  /* linewideth */
} graph_desc_t;

typedef struct image_desc_t {

    /* configuration of graph */

    char           graphfile[MAXPATH]; /* filename for graphic */
    FILE	  *graphhandle;        /* FILE to use if filename is "-" */
    long           xsize,ysize,piesize;    /* graph area size in pixels */
    gfx_color_t    graph_col[__GRC_END__]; /* real colors for the graph */   
    text_prop_t    text_prop[TEXT_PROP_LAST]; /* text properties */
    char           ylegend[200];   /* legend along the yaxis */
    char           title[200];     /* title for graph */
    int            draw_x_grid;      /* no x-grid at all */
    int            draw_y_grid;      /* no x-grid at all */
    double         grid_dash_on, grid_dash_off;
    xlab_t         xlab_user;      /* user defined labeling for xaxis */
    char           xlab_form[200]; /* format for the label on the xaxis */

    double         ygridstep;      /* user defined step for y grid */
    int            ylabfact;       /* every how many y grid shall a label be written ? */
    double         tabwidth;       /* tabwdith */
    time_t         start,end;      /* what time does the graph cover */
    unsigned long  step;           /* any preference for the default step ? */
    rrd_value_t    minval,maxval;  /* extreme values in the data */
    int            rigid;          /* do not expand range even with 
				      values outside */
    ygrid_scale_t  ygrid_scale;    /* calculated y axis grid info */
    int            gridfit;        /* adjust y-axis range etc so all
				      grindlines falls in integer pixel values */
    char*          imginfo;        /* construct an <IMG ... tag and return 
				      as first retval */
    int            lazy;           /* only update the image if there is
				      reasonable probablility that the
				      existing one is out of date */
    int            logarithmic;    /* scale the yaxis logarithmic */
    
    /* status information */
    	    
    long           xorigin,yorigin;/* where is (0,0) of the graph */
    long           pie_x,pie_y;    /* where is the centerpoint */
    long           ximg,yimg;      /* total size of the image */
    double         magfact;        /* numerical magnitude*/
    long         base;            /* 1000 or 1024 depending on what we graph */
    char           symbol;         /* magnitude symbol for y-axis */
    int            unitsexponent;    /* 10*exponent for units on y-asis */
    int            extra_flags;    /* flags for boolean options */
    /* data elements */

    long  prt_c;                  /* number of print elements */
    long  gdes_c;                  /* number of graphics elements */
    graph_desc_t   *gdes;          /* points to an array of graph elements */
    gfx_canvas_t   *canvas;        /* graphics library */
} image_desc_t;

/* Prototypes */
int xtr(image_desc_t *,time_t);
double ytr(image_desc_t *, double);
enum gf_en gf_conv(char *);
enum gfx_if_en if_conv(char *);
enum tmt_en tmt_conv(char *);
enum grc_en grc_conv(char *);
enum text_prop_en text_prop_conv(char *);
int im_free(image_desc_t *);
void auto_scale( image_desc_t *,  double *, char **, double *);
void si_unit( image_desc_t *);
void expand_range(image_desc_t *);
void apply_gridfit(image_desc_t *);
void reduce_data( enum cf_en,  unsigned long,  time_t *, time_t *,  unsigned long *,  unsigned long *,  rrd_value_t **);
int data_fetch( image_desc_t *);
long find_var(image_desc_t *, char *);
long find_var_wrapper(void *arg1, char *key);
long lcd(long *);
int data_calc( image_desc_t *);
int data_proc( image_desc_t *);
time_t find_first_time( time_t,  enum tmt_en,  long);
time_t find_next_time( time_t,  enum tmt_en,  long);
int print_calc(image_desc_t *, char ***);
int leg_place(image_desc_t *);
int calc_horizontal_grid(image_desc_t *);
int draw_horizontal_grid(image_desc_t *);
int horizontal_log_grid(image_desc_t *);
void vertical_grid(image_desc_t *);
void axis_paint(image_desc_t *);
void grid_paint(image_desc_t *);
int lazy_check(image_desc_t *);
int graph_paint(image_desc_t *, char ***);
void pie_part(image_desc_t *, gfx_color_t, double, double, double, double, double);
int gdes_alloc(image_desc_t *);
int scan_for_col(char *, int, char *);
int rrd_graph(int, char **, char ***, int *, int *, FILE *, double *, double *);
void rrd_graph_init(image_desc_t *);
void rrd_graph_options(int, char **, image_desc_t *);
void rrd_graph_script(int, char **, image_desc_t *, int);
int rrd_graph_check_vname(image_desc_t *, char *, char *);
int rrd_graph_color(image_desc_t *, char *, char *, int);
int rrd_graph_legend(graph_desc_t *, char *);
int bad_format(char *);
int vdef_parse(struct graph_desc_t *,char *);
int vdef_calc(image_desc_t *, int);
int vdef_percent_compar(const void *,const void *);
int graph_size_location(image_desc_t *, int, int);

#endif
