#include "rrd_tool.h"
#include "rrd_rpncalc.h"

#define MAX_VNAME_LEN 29
#define DEF_NAM_FMT "%29[_A-Za-z0-9]"

#define ALTYGRID	0x01	/* use alternative y grid algorithm */
#define ALTAUTOSCALE	0x02	/* use alternative algorithm to find lower and upper bounds */
#define ALTAUTOSCALE_MAX 0x04	/* use alternative algorithm to find upper bounds */
#define NOLEGEND	0x08	/* use no legend */


enum tmt_en {TMT_SECOND=0,TMT_MINUTE,TMT_HOUR,TMT_DAY,
	     TMT_WEEK,TMT_MONTH,TMT_YEAR};

enum grc_en {GRC_CANVAS=0,GRC_BACK,GRC_SHADEA,GRC_SHADEB,
	     GRC_GRID,GRC_MGRID,GRC_FONT,GRC_FRAME,GRC_ARROW,__GRC_END__};


enum gf_en {GF_PRINT=0,GF_GPRINT,GF_COMMENT,GF_HRULE,GF_VRULE,GF_LINE1,
	    GF_LINE2,GF_LINE3,GF_AREA,GF_STACK,GF_TICK,
	    GF_DEF, GF_CDEF, GF_VDEF};

enum if_en {IF_GIF=0,IF_PNG=1};

enum vdef_op_en {
		 VDEF_MAXIMUM	/* like the MAX in (G)PRINT */
		,VDEF_MINIMUM	/* like the MIN in (G)PRINT */
		,VDEF_AVERAGE	/* like the AVERAGE in (G)PRINT */
		,VDEF_PERCENT	/* Nth percentile */
		,VDEF_TOTAL	/* average multiplied by time */
		,VDEF_FIRST	/* first non-unknown value and time */
		,VDEF_LAST	/* last  non-unknown value and time */
		};
typedef struct vdef_t {
    enum vdef_op_en	op;
    double		param;	/* parameter for function, if applicable */
    double		val;	/* resulting value */
    time_t		when;	/* timestamp, if applicable */
} vdef_t;

typedef struct col_trip_t {
    int red; /* red = -1 is no color */
    int green;
    int blue;
    int i; /* color index assigned in gif image i=-1 is unasigned*/
} col_trip_t;


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

#if 0
xlab_t xlab[] = {
    {0,        TMT_SECOND,30, TMT_MINUTE,5,  TMT_MINUTE,5,         0,"%H:%M"},
    {2,        TMT_MINUTE,1,  TMT_MINUTE,5,  TMT_MINUTE,5,         0,"%H:%M"},
    {5,        TMT_MINUTE,2,  TMT_MINUTE,10, TMT_MINUTE,10,        0,"%H:%M"},
    {10,       TMT_MINUTE,5,  TMT_MINUTE,20, TMT_MINUTE,20,        0,"%H:%M"},
    {30,       TMT_MINUTE,10, TMT_HOUR,1,    TMT_HOUR,1,           0,"%H:%M"},
    {60,       TMT_MINUTE,30, TMT_HOUR,2,    TMT_HOUR,2,           0,"%H:%M"},
    {180,      TMT_HOUR,1,    TMT_HOUR,6,    TMT_HOUR,6,           0,"%H:%M"},
    /*{300,      TMT_HOUR,3,    TMT_HOUR,12,   TMT_HOUR,12,    12*3600,"%a %p"},  this looks silly*/
    {600,      TMT_HOUR,6,    TMT_DAY,1,     TMT_DAY,1,      24*3600,"%a"},
    {1800,     TMT_HOUR,12,   TMT_DAY,1,     TMT_DAY,2,      24*3600,"%a"},
    {3600,     TMT_DAY,1,     TMT_WEEK,1,     TMT_WEEK,1,    7*24*3600,"Week %W"},
    {3*3600,   TMT_WEEK,1,      TMT_MONTH,1,     TMT_WEEK,2,    7*24*3600,"Week %W"},
    {6*3600,   TMT_MONTH,1,   TMT_MONTH,1,   TMT_MONTH,1, 30*24*3600,"%b"},
    {48*3600,  TMT_MONTH,1,   TMT_MONTH,3,   TMT_MONTH,3, 30*24*3600,"%b"},
    {10*24*3600, TMT_YEAR,1,  TMT_YEAR,1,    TMT_YEAR,1, 365*24*3600,"%y"},
    {-1,TMT_MONTH,0,TMT_MONTH,0,TMT_MONTH,0,0,""}
};

/* sensible logarithmic y label intervals ...
   the first element of each row defines the possible starting points on the
   y axis ... the other specify the */

double yloglab[][12]= {{ 1e9, 1,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0 },
		       {  1e3, 1,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0 },
		       {  1e1, 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
		       /* {  1e1, 1,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0 }, */
		       {  1e1, 1,  2.5,  5,  7.5,  0,  0,  0,  0,  0,  0,  0 },
		       {  1e1, 1,  2,  4,  6,  8,  0,  0,  0,  0,  0,  0 },
		       {  1e1, 1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0 },
		       {  0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 }};

#endif

/* sensible y label intervals ...*/

typedef struct ylab_t {
    double   grid;    /* grid spacing */
    int      lfac[4]; /* associated label spacing*/
} ylab_t;

#if 0
ylab_t ylab[]= {
    {0.1, {1,2, 5,10}},
    {0.2, {1,5,10,20}},
    {0.5, {1,2, 4,10}},
    {1.0,   {1,2, 5,10}},
    {2.0,   {1,5,10,20}},
    {5.0,   {1,2, 4,10}},
    {10.0,  {1,2, 5,10}},
    {20.0,  {1,5,10,20}},
    {50.0,  {1,2, 4,10}},
    {100.0, {1,2, 5,10}},
    {200.0, {1,5,10,20}},
    {500.0, {1,2, 4,10}},
    {0.0,   {0,0,0,0}}};



col_trip_t graph_col[] = { /* default colors */
    {255,255,255,-1},   /* canvas */
    {245,245,245,-1},   /* background */
    {200,200,200,-1},   /* shade A    */
    {150,150,150,-1},   /* shade B    */
    {140,140,140,-1},      /* grid */
    {130,30,30,-1},      /* major grid */
    {0,0,0,-1},         /* font */ 	
    {0,0,0,-1},         /* frame */
    {255,0,0,-1}	/*arrow*/
};

#endif

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
    char           vname[MAX_VNAME_LEN+1];  /* name of the variable */
    long           vidx;       /* gdes reference */
    char           rrd[255];   /* name of the rrd_file containing data */
    char           ds_nam[DS_NAM_SIZE]; /* data source name */
    long           ds;         /* data source number */
    enum cf_en     cf;         /* consolidation function */
    col_trip_t     col;        /* graph color */
    char           format[FMT_LEG_LEN+5]; /* format for PRINT AND GPRINT */
    char           legend[FMT_LEG_LEN+5]; /* legend*/
    gdPoint        legloc;     /* location of legend */   
    double         yrule;      /* value for y rule line and for VDEF */
    time_t         xrule;      /* time for x rule line and for VDEF */
    vdef_t         vf;         /* instruction for VDEF function */
    rpnp_t         *rpnp;     /* instructions for CDEF function */

    /* description of data fetched for the graph element */
    time_t         start,end; /* timestaps for first and last data element */
    unsigned long  step;      /* time between samples */
    unsigned long  ds_cnt; /* how many data sources are there in the fetch */
    long           data_first; /* first pointer to this data */
    char           **ds_namv; /* name of datasources  in the fetch. */
    rrd_value_t    *data; /* the raw data drawn from the rrd */
    rrd_value_t    *p_data; /* processed data, xsize elments */

} graph_desc_t;

typedef struct image_desc_t {

    /* configuration of graph */

    char           graphfile[MAXPATH]; /* filename for graphic */
    long           xsize,ysize;    /* graph area size in pixels */
    col_trip_t     graph_col[__GRC_END__]; /* real colors for the graph */   
    char           ylegend[200];   /* legend along the yaxis */
    char           title[200];     /* title for graph */
    int            draw_x_grid;      /* no x-grid at all */
    int            draw_y_grid;      /* no x-grid at all */
    xlab_t         xlab_user;      /* user defined labeling for xaxis */
    char           xlab_form[200]; /* format for the label on the xaxis */

    double         ygridstep;      /* user defined step for y grid */
    int            ylabfact;       /* every how many y grid shall a label be written ? */

    time_t         start,end;      /* what time does the graph cover */
    unsigned long           step;           /* any preference for the default step ? */
    rrd_value_t    minval,maxval;  /* extreme values in the data */
    int            rigid;          /* do not expand range even with 
				      values outside */
    char*          imginfo;         /* construct an <IMG ... tag and return 
				      as first retval */
    int            lazy;           /* only update the gif if there is reasonable
				      probablility that the existing one is out of date */
    int            logarithmic;    /* scale the yaxis logarithmic */
    enum if_en     imgformat;         /* image format */
    
    /* status information */
    	    
    long           xorigin,yorigin;/* where is (0,0) of the graph */
    long           xgif,ygif;      /* total size of the gif */
    int            interlaced;     /* will the graph be interlaced? */
    double         magfact;        /* numerical magnitude*/
    long         base;            /* 1000 or 1024 depending on what we graph */
    char           symbol;         /* magnitude symbol for y-axis */
    int            unitsexponent;    /* 10*exponent for units on y-asis */
    int            extra_flags;    /* flags for boolean options */
    /* data elements */

    long  prt_c;                  /* number of print elements */
    long  gdes_c;                  /* number of graphics elements */
    graph_desc_t   *gdes;          /* points to an array of graph elements */

} image_desc_t;

/* Prototypes */
int xtr(image_desc_t *,time_t);
int ytr(image_desc_t *, double);
enum gf_en gf_conv(char *);
enum if_en if_conv(char *);
enum tmt_en tmt_conv(char *);
enum grc_en grc_conv(char *);
int im_free(image_desc_t *);
void auto_scale( image_desc_t *,  double *, char **, double *);
void si_unit( image_desc_t *);
void expand_range(image_desc_t *);
void reduce_data( enum cf_en,  unsigned long,  time_t *, time_t *,  unsigned long *,  unsigned long *,  rrd_value_t **);
int data_fetch( image_desc_t *);
long find_var(image_desc_t *, char *);
long find_var_wrapper(void *arg1, char *key);
long lcd(long *);
int data_calc( image_desc_t *);
int data_proc( image_desc_t *);
time_t find_first_time( time_t,  enum tmt_en,  long);
time_t find_next_time( time_t,  enum tmt_en,  long);
void gator( gdImagePtr, int, int);
int print_calc(image_desc_t *, char ***);
int leg_place(image_desc_t *);
int horizontal_grid(gdImagePtr, image_desc_t *);
int horizontal_log_grid(gdImagePtr, image_desc_t *);
void vertical_grid( gdImagePtr, image_desc_t *);
void axis_paint( image_desc_t *, gdImagePtr);
void grid_paint( image_desc_t *, gdImagePtr);
gdImagePtr MkLineBrush(image_desc_t *,long, enum gf_en);
int lazy_check(image_desc_t *);
int graph_paint(image_desc_t *, char ***);
int gdes_alloc(image_desc_t *);
int scan_for_col(char *, int, char *);
int rrd_graph(int, char **, char ***, int *, int *);
void rrd_graph_init(image_desc_t *);
void rrd_graph_options(int, char **, image_desc_t *);
void rrd_graph_script(int, char **, image_desc_t *);
int rrd_graph_check_vname(image_desc_t *, char *, char *);
int rrd_graph_check_CF(image_desc_t *, char *, char *);
int bad_format(char *);
int vdef_parse(struct graph_desc_t *,char *);
int vdef_calc(image_desc_t *, int);
int vdef_percent_compar(const void *,const void *);
