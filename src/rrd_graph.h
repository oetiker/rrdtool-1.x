#ifndef RRD_GRAPH_H_DBEDBFB6C5844ED9BEA6242F879CA284
#define RRD_GRAPH_H_DBEDBFB6C5844ED9BEA6242F879CA284

#define y0 cairo_y0
#define y1 cairo_y1
#define index cairo_index

/* this may configure __EXTENSIONS__ without which pango will fail to compile
   so load this early */
#include "rrd_config.h"

#include <cairo.h>
#ifdef CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif
#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif
#ifdef CAIRO_HAS_PS_SURFACE
#include <cairo-ps.h>
#endif

#include <pango/pangocairo.h>


#include "rrd_tool.h"
#include "rrd_rpncalc.h"
#include "mutex.h"

#include <glib.h>


#define ALTYGRID  	 0x01   /* use alternative y grid algorithm */
#define ALTAUTOSCALE	 0x02   /* use alternative algorithm to find lower and upper bounds */
#define ALTAUTOSCALE_MIN 0x04   /* use alternative algorithm to find lower bounds */
#define ALTAUTOSCALE_MAX 0x08   /* use alternative algorithm to find upper bounds */
#define NOLEGEND	 0x10   /* use no legend */
#define NOMINOR          0x20   /* Turn off minor gridlines */
#define ONLY_GRAPH       0x40   /* use only graph */
#define FORCE_RULES_LEGEND 0x80 /* force printing of HRULE and VRULE legend */

#define FORCE_UNITS 0x100   /* mask for all FORCE_UNITS_* flags */
#define FORCE_UNITS_SI 0x100    /* force use of SI units in Y axis (no effect in linear graph, SI instead of E in log graph) */

#define FULL_SIZE_MODE     0x200    /* -width and -height indicate the total size of the image */
#define NO_RRDTOOL_TAG 0x400  /* disable the rrdtool tag */
#define ALLOW_MISSING_DS 0x800  /* missing DS is not fatal */

#define gdes_fetch_key(x)  sprintf_alloc("%s:%s:%d:%d:%d:%d:%d:%d",x.rrd,x.daemon,x.cf,x.cf_reduce,x.start_orig,x.end_orig,x.step_orig,x.step)

enum tmt_en { TMT_SECOND = 0, TMT_MINUTE, TMT_HOUR, TMT_DAY,
    TMT_WEEK, TMT_MONTH, TMT_YEAR
};

enum grc_en { GRC_CANVAS = 0, GRC_BACK, GRC_SHADEA, GRC_SHADEB,
    GRC_GRID, GRC_MGRID, GRC_FONT, GRC_ARROW, GRC_AXIS, GRC_FRAME, __GRC_END__
};

#define MGRIDWIDTH 0.6
#define GRIDWIDTH  0.4

enum gf_en { GF_PRINT = 0, GF_GPRINT, GF_COMMENT, GF_HRULE, GF_VRULE, GF_LINE,
    GF_AREA,GF_STACK, GF_TICK, GF_TEXTALIGN,
    GF_DEF, GF_CDEF, GF_VDEF, GF_SHIFT,
    GF_XPORT, GF_XAXIS, GF_YAXIS
};

enum txa_en { TXA_LEFT = 0, TXA_RIGHT, TXA_CENTER, TXA_JUSTIFIED };

enum vdef_op_en {
    VDEF_MAXIMUM = 0    /* like the MAX in (G)PRINT */
        , VDEF_MINIMUM  /* like the MIN in (G)PRINT */
        , VDEF_AVERAGE  /* like the AVERAGE in (G)PRINT */
        , VDEF_STDEV    /* the standard deviation */
        , VDEF_PERCENT  /* Nth percentile */
        , VDEF_TOTAL    /* average multiplied by time */
        , VDEF_FIRST    /* first non-unknown value and time */
        , VDEF_LAST     /* last  non-unknown value and time */
        , VDEF_LSLSLOPE /* least squares line slope */
        , VDEF_LSLINT   /* least squares line y_intercept */
        , VDEF_LSLCORREL    /* least squares line correlation coefficient */
        , VDEF_PERCENTNAN  /* Nth percentile ignoring NAN*/
};
enum text_prop_en {
    TEXT_PROP_DEFAULT = 0,  /* default settings */
    TEXT_PROP_TITLE,    /* properties for the title */
    TEXT_PROP_AXIS,     /* for the numbers next to the axis */
    TEXT_PROP_UNIT,     /* for the vertical unit description */
    TEXT_PROP_LEGEND,   /* for the legend below the graph */
    TEXT_PROP_WATERMARK, /* for the little text to the side of the graph */
    TEXT_PROP_LAST
};

enum legend_pos{ NORTH = 0, WEST, SOUTH, EAST };
enum legend_direction { TOP_DOWN = 0, BOTTOM_UP, BOTTOM_UP2 };

enum gfx_if_en { IF_PNG = 0, IF_SVG, IF_EPS, IF_PDF,
		 IF_XML=128, IF_CSV=129, IF_TSV=130, IF_SSV=131, IF_JSON=132,
		 IF_XMLENUM=133, IF_JSONTIME=134
};
enum gfx_en { GFX_LINE = 0, GFX_AREA, GFX_TEXT };
enum gfx_h_align_en { GFX_H_NULL = 0, GFX_H_LEFT, GFX_H_RIGHT, GFX_H_CENTER };
enum gfx_v_align_en { GFX_V_NULL = 0, GFX_V_TOP, GFX_V_BOTTOM, GFX_V_CENTER };

enum gfx_type_en {GTYPE_TIME=0,GTYPE_XY=1};

/* cairo color components */
typedef struct gfx_color_t {
    double    red;
    double    green;
    double    blue;
    double    alpha;
} gfx_color_t;

typedef struct keyvalue_t {
  char* key;
  char* value;
  char* keyvalue;
  int pos;
  int flag;
} keyvalue_t;

typedef struct keyint_t {
  char*key;
  int value;
} keyint_t;

typedef struct parsedargs_t {
  char *arg; /* copy of the parsed string - rewritten*/
  const char *arg_orig; /* original argument */
  int kv_cnt; /* number of key/value arguments */
  keyvalue_t *kv_args; /* key value arguments */
} parsedargs_t;
void initParsedArguments(parsedargs_t*);
void resetParsedArguments(parsedargs_t*);
void freeParsedArguments(parsedargs_t*);
int addToArguments(parsedargs_t*, char*, char*, char*, int);
int parseArguments(const char*, parsedargs_t*);
void dumpKeyValue(char* ,keyvalue_t*);
void dumpArguments(parsedargs_t*);
char* getKeyValueArgument(const char*, int, parsedargs_t*);
int getMappedKeyValueArgument(const char*,int, parsedargs_t*,
			      int*,keyint_t**);
int getLong(const char*,long *,char**,int);
int getDouble(const char*,double *,char**);
keyvalue_t* getFirstUnusedArgument(int, parsedargs_t*);
char* checkUnusedValues(parsedargs_t*);

typedef struct text_prop_t {
    double    size;
    char      font[1024];
    PangoFontDescription *font_desc;
} text_prop_t;


typedef struct vdef_t {
    enum vdef_op_en op;
    double    param;    /* parameter for function, if applicable */
    double    val;      /* resulting value */
    time_t    when;     /* timestamp, if applicable */
    int       never;    /* boolean, indicate that when value mean never */
} vdef_t;

typedef struct xlab_t {
    double    minsec;   /* minimum sec per pix */
    long      length;   /* number of secs on the image */
    enum tmt_en gridtm; /* grid interval in what ? */
    long      gridst;   /* how many whats per grid */
    enum tmt_en mgridtm;    /* label interval in what ? */
    long      mgridst;  /* how many whats per label */
    enum tmt_en labtm;  /* label interval in what ? */
    long      labst;    /* how many whats per label */
    long      precis;   /* label precision -> label placement */
    char     *stst;     /* strftime string */
} xlab_t;

typedef struct ygrid_scale_t {  /* y axis grid scaling info */
    double    gridstep;
    int       labfact;
    char      labfmt[64];
} ygrid_scale_t;

/* sensible y label intervals ...*/

typedef struct ylab_t {
    double    grid;     /* grid spacing */
    int       lfac[4];  /* associated label spacing */
} ylab_t;

enum value_formatter_en {
    VALUE_FORMATTER_NUMERIC,    /* printf */
    VALUE_FORMATTER_TIMESTAMP,  /* strftime */
    VALUE_FORMATTER_DURATION,   /* strfduration */
};

/* this structure describes the elements which can make up a graph.
   because they are quite diverse, not all elements will use all the
   possible parts of the structure. */
#ifdef HAVE_SNPRINTF
#define FMT_LEG_LEN 200
#else
#define FMT_LEG_LEN 2000
#endif

# define MAX_AXIS 4
# define MAX_IMAGE_TITLE_LINES 3

typedef struct graph_desc_t {
    enum gf_en gf;       /* graphing function */
    int       stack;     /* boolean */
    int       debug;     /* boolean */
    int       skipscale; /* boolean */
    char      vname[MAX_VNAME_LEN + 1]; /* name of the variable */
    long      vidx;      /* gdes reference */
    char      rrd[1024];    /* name of the rrd_file containing data */
    char      ds_nam[DS_NAM_SIZE];  /* data source name */
    long      ds;       /* data source number */
    char      daemon[256];
    enum cf_en cf;      /* consolidation function */
    enum cf_en cf_reduce;   /* consolidation function for rrd_reduce_data() */
    int        cf_reduce_set; /* is the cf_reduce option set */
    struct gfx_color_t col, col2; /* graph color */
	double    gradheight;
    char      format[FMT_LEG_LEN + 5];  /* format for PRINT AND GPRINT */
    char      legend[FMT_LEG_LEN + 5];  /* legend */
    int       strftm;   /* should the VDEF legend be the time component formatted with strftime */
    enum value_formatter_en vformatter; /* what value formatter to use (if !strftm) */
    double    leg_x, leg_y; /* location of legend */
    double    yrule;    /* value for y rule line and for VDEF */
    time_t    xrule;    /* time for x rule line and for VDEF */
    vdef_t    vf;       /* instruction for VDEF function */
    rpnp_t   *rpnp;     /* instructions for CDEF function */
    char     *rpn;      /* string representation of rpn */

    /* SHIFT implementation */
    int       shidx;    /* gdes reference for offset (-1 --> constant) */
    time_t    shval;    /* offset if shidx is -1 */
    time_t    shift;    /* current shift applied */

    /* description of data fetched for the graph element */
    time_t    start, end;   /* timestamps for first and last data element */
    time_t    start_orig, end_orig; /* timestamps for first and last data element */
    unsigned long step; /* time between samples */
    unsigned long step_orig;    /* time between samples */
    unsigned long ds_cnt;   /* how many data sources are there in the fetch */
    long      data_first;   /* first pointer to this data */
    char    **ds_namv;  /* name of datasources  in the fetch. */
    rrd_value_t *data;  /* the raw data drawn from the rrd */
    rrd_value_t *p_data;    /* processed data, xsize elements */
    double    linewidth;    /* linewidth */

    /* dashed line stuff */
    int       dash;     /* boolean, draw dashed line? */
    double   *p_dashes; /* pointer do dash array which keeps the lengths of dashes */
    int       ndash;    /* number of dash segments */
    double    offset;   /* dash offset along the line */


    enum txa_en txtalign;   /* change default alignment strategy for text */

    /* the axis to use for this graph in x and y*/
    int xaxisidx;
    int yaxisidx;
} graph_desc_t;

enum image_init_en { IMAGE_INIT_NO_CAIRO, IMAGE_INIT_CAIRO };

typedef struct image_desc_t {

    /* configuration of graph */
    char      *graphfile;   /* filename for graphic */
    enum      gfx_type_en graph_type; /* type of the graph */
    long      xsize, ysize; /* graph area size in pixels */
    struct gfx_color_t graph_col[__GRC_END__];  /* real colors for the graph */
    text_prop_t text_prop[TEXT_PROP_LAST];  /* text properties */
    char      *ylegend; /* legend along the yaxis */
    char      *title;   /* title for graph */
    char      *watermark;   /* watermark for graph */
    int       draw_x_grid;  /* no x-grid at all */
    int       draw_y_grid;  /* no y-grid at all */
    unsigned int draw_3d_border; /* size of border in pixels, 0 for off */
    unsigned int dynamic_labels; /* pick the label shape according to the line drawn */
    double    grid_dash_on, grid_dash_off;
    xlab_t    xlab_user;    /* user defined labeling for xaxis */
    char      *xlab_form;   /* format for the label on the xaxis */
    double    second_axis_scale; /* relative to the first axis (0 to disable) */
    double    second_axis_shift; /* how much is it shifted vs the first axis */
    char      *second_axis_legend; /* label to put on the second axis */
    char      *second_axis_format; /* format for the numbers on the scond axis */
    enum value_formatter_en second_axis_formatter;  /* How to format axis values */
    char      *primary_axis_format; /* format for the numbers on the primary axis */
    enum value_formatter_en primary_axis_formatter; /* How to format axis values */
    double    ygridstep;    /* user defined step for y grid */
    int       ylabfact; /* every how many y grid shall a label be written ? */
    double    tabwidth; /* tabwidth */
    time_t    start, end;   /* what time does the graph cover */
    unsigned long step; /* any preference for the default step ? */
    rrd_value_t minval, maxval; /* extreme values in the data */
    int       rigid;    /* do not expand range even with
                           values outside */
    int       allow_shrink; /* less "rigid" --rigid */
    ygrid_scale_t ygrid_scale;  /* calculated y axis grid info */
    int       gridfit;  /* adjust y-axis range etc so all
                           grindlines falls in integer pixel values */
    char     *imginfo;  /* construct an <IMG ... tag and return
                           as first retval */
    enum gfx_if_en imgformat;   /* image format */
    char     *daemon_addr;  /* rrdcached connection string */
    int       lazy;     /* only update the image if there is
                           reasonable probability that the
                           existing one is out of date */
    int       slopemode;    /* connect the dots of the curve directly, not using a stair */
    enum legend_pos legendposition; /* the position of the legend: north, west, south or east */
    enum legend_direction legenddirection; /* The direction of the legend topdown or bottomup */
    int       logarithmic;  /* scale the yaxis logarithmic */
    double    force_scale_min;  /* Force a scale--min */
    double    force_scale_max;  /* Force a scale--max */

    /* status information */
    int       with_markup;
    long      xorigin, yorigin; /* where is (0,0) of the graph */
    long      xOriginTitle, yOriginTitle; /* where is the origin of the title */
    long      xOriginLegendY, yOriginLegendY; /* where is the origin of the y legend */
    long      xOriginLegendY2, yOriginLegendY2; /* where is the origin of the second y legend */
    long      xOriginLegend, yOriginLegend; /* where is the origin of the legend */
    long      ximg, yimg;   /* total size of the image */
    long      legendwidth, legendheight; /* the calculated height and width of the legend */
    size_t    rendered_image_size;
    double    zoom;
    double    magfact;  /* numerical magnitude */
    long      base;     /* 1000 or 1024 depending on what we graph */
    char      symbol;   /* magnitude symbol for y-axis */
    float     viewfactor;   /* how should the numbers on the y-axis be scaled for viewing ? */
    int       unitsexponent;    /* 10*exponent for units on y-axis */
    int       unitslength;  /* width of the yaxis labels */
    int       forceleftspace;   /* do not kill the space to the left of the y-axis if there is no grid */

    int       extra_flags;  /* flags for boolean options */
    /* data elements */

    unsigned char *rendered_image;
    long      prt_c;    /* number of print elements */
    long      gdes_c;   /* number of graphics elements */
    graph_desc_t *gdes; /* points to an array of graph elements */
    cairo_surface_t *surface;   /* graphics library */
    cairo_t  *cr;       /* drawing context */
    cairo_font_options_t *font_options; /* cairo font options */
    cairo_antialias_t graph_antialias;  /* antialiasing for the graph */
    PangoLayout *layout; /* the pango layout we use for writing fonts */
    rrd_info_t *grinfo; /* root pointer to extra graph info */
    rrd_info_t *grinfo_current; /* pointing to current entry */
    GHashTable* gdef_map;  /* a map of all *def gdef entries for quick access */
    GHashTable* rrd_map;  /* a map of all rrd files in use for gdef entries */
    mutex_t *fontmap_mutex; /* Mutex for locking the global fontmap */
    enum image_init_en init_mode; /* do we need Cairo/Pango? */
    double x_pixie; /* scale for X (see xtr() for reference) */
    double y_pixie; /* scale for Y (see ytr() for reference) */
    double last_tabwidth; /* (see gfx_prep_text() for reference) */
} image_desc_t;

typedef struct image_title_t
{
    char **lines;
    int count;
} image_title_t;

/* Prototypes */
int       xtr(
    image_desc_t *,
    time_t);
double    ytr(
    image_desc_t *,
    double);
enum gf_en gf_conv(
    const char *);
enum gfx_if_en if_conv(
    const char *);
enum gfx_type_en type_conv(
    const char *);
enum tmt_en tmt_conv(
    const char *);
enum grc_en grc_conv(
    const char *);
enum text_prop_en text_prop_conv(
    const char *);
int       im_free(
    image_desc_t *);
void      auto_scale(
    image_desc_t *,
    double *,
    char **,
    double *);
void      si_unit(
    image_desc_t *);
void      expand_range(
    image_desc_t *);
void      apply_gridfit(
    image_desc_t *);
int     rrd_reduce_data(
    enum cf_en,
    unsigned long,
    time_t *,
    time_t *,
    unsigned long *,
    unsigned long *,
    rrd_value_t **);
int       data_fetch(
    image_desc_t *);
long      rrd_lcd(
    long *);
int       data_calc(
    image_desc_t *);
int       data_proc(
    image_desc_t *);
time_t    find_first_time(
    time_t,
    enum tmt_en,
    long);
time_t    find_next_time(
    time_t,
    enum tmt_en,
    long);
int       print_calc(
    image_desc_t *);
int       leg_place(
    image_desc_t *,
    int);
int       calc_horizontal_grid(
    image_desc_t *);
int       draw_horizontal_grid(
    image_desc_t *);
int       horizontal_log_grid(
    image_desc_t *);
void      vertical_grid(
    image_desc_t *);
void      axis_paint(
    image_desc_t *);
int      grid_paint(
    image_desc_t *);
int       lazy_check(
    image_desc_t *);
int       graph_paint(
    image_desc_t *);
int       graph_paint_timestring(
                                image_desc_t *,int,int);
int       graph_paint_xy(
                        image_desc_t *,int,int);
image_title_t graph_title_split(
    const char *);
int       rrd_graph_xport(
    image_desc_t *);

int       graph_cairo_setup(
    image_desc_t *);
int       graph_cairo_finish(
    image_desc_t *);

int       gdes_alloc(
    image_desc_t *);
int       scan_for_col(
    const char *const,
    int,
    char *const);
void      rrd_graph_init(
    image_desc_t *,
    enum image_init_en);

void      time_clean(
    char *result,
    char *format);

void      rrd_graph_options(
    int,
    char **,
    struct optparse *,
    image_desc_t *);
void      rrd_graph_script(
    int,
    char **,
    image_desc_t *const,
    int);
int       rrd_graph_color(
    image_desc_t *,
    char *,
    char *,
    int);
int       bad_format_axis(
    char *);
int       bad_format_print(
    char *);
int       bad_format_imginfo(
    char *);
int       vdef_parse(
    struct graph_desc_t *,
    const char *const);
int       vdef_calc(
    image_desc_t *,
    int);
int       vdef_percent_compar(
    const void *,
    const void *);
int       graph_size_location(
    image_desc_t *,
    int);


/* create a new line */
void      gfx_line(
    image_desc_t *im,
    double X0,
    double Y0,
    double X1,
    double Y1,
    double width,
    gfx_color_t color);

void      gfx_dashed_line(
    image_desc_t *im,
    double X0,
    double Y0,
    double X1,
    double Y1,
    double width,
    gfx_color_t color,
    double dash_on,
    double dash_off);

/* create a new area */
void      gfx_new_area(
    image_desc_t *im,
    double X0,
    double Y0,
    double X1,
    double Y1,
    double X2,
    double Y2,
    gfx_color_t color);

/* add a point to a line or to an area */
void      gfx_add_point(
    image_desc_t *im,
    double x,
    double y);

/* create a rect that has a gradient from color1 to color2 in height pixels
 * height > 0:
 * 		gradient starts at top and goes down a fixed number of pixels (fire style)
 * height < 0:
 * 		gradient starts at bottom and goes up a fixed number of pixels (constant style)
 * height == 0:
 * 		gradient is stretched between two points
 */
void gfx_add_rect_fadey(
    image_desc_t *im,
    double x1,double y1,
    double x2,double y2,
	double py,
    gfx_color_t color1,
	gfx_color_t color2,
	double height);



/* close current path so it ends at the same point as it started */
void      gfx_close_path(
    image_desc_t *im);


/* create a text node */
void      gfx_text(
    image_desc_t *im,
    double x,
    double y,
    gfx_color_t color,
    PangoFontDescription *font_desc,
    double tabwidth,
    double angle,
    enum gfx_h_align_en h_align,
    enum gfx_v_align_en v_align,
    const char *text);

/* measure width of a text string */
double    gfx_get_text_width(
    image_desc_t *im,
    double start,
    PangoFontDescription *font_desc,
    double tabwidth,
    char *text);

/* measure height of a text string */
double    gfx_get_text_height(
    image_desc_t *im,
    double start,
    PangoFontDescription *font_desc,
    double tabwidth,
    char *text);


/* convert color */
gfx_color_t gfx_hex_to_col(
    long unsigned int);

void      gfx_line_fit(
    image_desc_t *im,
    double *x,
    double *y);

void      gfx_area_fit(
    image_desc_t *im,
    double *x,
    double *y);

#endif

void      grinfo_push(
    image_desc_t *im,
    char *key,
    rrd_info_type_t type,    rrd_infoval_t value);

