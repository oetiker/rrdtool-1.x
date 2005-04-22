/****************************************************************************
 * RRDtool 1.2rc9  Copyright by Tobi Oetiker, 1997-2005
 ****************************************************************************
 * rrd_gfx.h generic graphics adapter library
 ****************************************************************************/

#ifndef  RRD_GFX_H
#define RRD_GFX_H
#define LIBART_COMPILATION

#include <libart_lgpl/libart.h>
#include <libart_lgpl/art_rgba.h>
#include "art_rgba_svp.h"

enum gfx_if_en {IF_PNG=0,IF_SVG,IF_EPS,IF_PDF};
enum gfx_en { GFX_LINE=0,GFX_AREA,GFX_TEXT };
enum gfx_h_align_en { GFX_H_NULL=0, GFX_H_LEFT, GFX_H_RIGHT, GFX_H_CENTER };
enum gfx_v_align_en { GFX_V_NULL=0, GFX_V_TOP,  GFX_V_BOTTOM, GFX_V_CENTER };
typedef unsigned long gfx_color_t;

typedef struct  gfx_node_t {
  enum gfx_en   type;         /* type of graph element */
  gfx_color_t   color;        /* color of element  0xRRGGBBAA  alpha 0xff is solid*/
  double        size;         /* font size, line width */
  double        dash_on, dash_off; /* dash line fragments lengths */
  int           closed_path;
  int           points;
  int           points_max;
  char *filename;             /* font or image filename */
  char *text;
  ArtVpath      *path;        /* path */
  double        x,y;          /* position */
  double        angle;        /* text angle */
  enum gfx_h_align_en halign; /* text alignement */
  enum gfx_v_align_en valign; /* text alignement */
  double        tabwidth; 
  struct gfx_node_t  *next; 
} gfx_node_t;


typedef struct gfx_canvas_t 
{
    struct gfx_node_t *firstnode;
    struct gfx_node_t *lastnode;
    enum gfx_if_en imgformat;      /* image format */
    int            interlaced;     /* will the graph be interlaced? */
    double         zoom;           /* zoom for graph */
} gfx_canvas_t;

gfx_canvas_t *gfx_new_canvas (void);

/* create a new line */
gfx_node_t   *gfx_new_line   (gfx_canvas_t *canvas, 
			      double X0, double Y0, 
	 		      double X1, double Y1,
 			      double width, gfx_color_t color);

gfx_node_t   *gfx_new_dashed_line   (gfx_canvas_t *canvas, 
			      double X0, double Y0, 
	 		      double X1, double Y1,
 			      double width, gfx_color_t color,
			      double dash_on, double dash_off);

/* create a new area */
gfx_node_t   *gfx_new_area   (gfx_canvas_t *canvas, 
			      double X0, double Y0,
			      double X1, double Y1,
			      double X2, double Y2,
			      gfx_color_t  color);

/* add a point to a line or to an area */
int           gfx_add_point  (gfx_node_t *node, double x, double y);

/* close current path so it ends at the same point as it started */
void          gfx_close_path  (gfx_node_t *node);


/* create a text node */
gfx_node_t   *gfx_new_text   (gfx_canvas_t *canvas,  
			      double x, double y, gfx_color_t color,
			      char* font, double size, 			      
			      double tabwidth, double angle,
			      enum gfx_h_align_en h_align,
			      enum gfx_v_align_en v_align,
                              char* text);

/* measure width of a text string */
double gfx_get_text_width ( gfx_canvas_t *canvas,
			    double start, char* font, double size,
			    double tabwidth, char* text, int rotation);

/* save image to file */
int       gfx_render (gfx_canvas_t *canvas,
                              art_u32 width, art_u32 height,
                              gfx_color_t background, FILE *fo);

/* free memory used by nodes this will also remove memory required for
   node chain and associated material */
int           gfx_destroy    (gfx_canvas_t *canvas); 


/* PNG support*/
int       gfx_render_png (gfx_canvas_t *canvas,
                              art_u32 width, art_u32 height,
                              gfx_color_t background, FILE *fo);
double gfx_get_text_width_libart ( double start, char* font, double size,
			    double tabwidth, char* text, int rotation);

/* SVG support */
int       gfx_render_svg (gfx_canvas_t *canvas,
                              art_u32 width, art_u32 height,
                              gfx_color_t background, FILE *fo);

/* EPS support */
int       gfx_render_eps (gfx_canvas_t *canvas,
                              art_u32 width, art_u32 height,
                              gfx_color_t background, FILE *fo);

/* PDF support */
int       gfx_render_pdf (gfx_canvas_t *canvas,
                              art_u32 width, art_u32 height,
                              gfx_color_t background, FILE *fo);

#endif
