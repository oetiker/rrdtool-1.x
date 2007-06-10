/****************************************************************************
 * RRDtool 1.2.23  Copyright by Tobi Oetiker, 1997-2007
 ****************************************************************************
 * rrd_gfx.h generic graphics adapter library
 ****************************************************************************/

#ifndef RRD_GFX_H
#define RRD_GFX_H

#define y0 cairo_y0
#define y1 cairo_y1
#define index cairo_index

#include <cairo-pdf.h>
#include <cairo-svg.h>
#include <cairo-ps.h>
#include <pango/pangocairo.h>

enum gfx_if_en { IF_PNG = 0, IF_SVG, IF_EPS, IF_PDF };
enum gfx_en { GFX_LINE = 0, GFX_AREA, GFX_TEXT };
enum gfx_h_align_en { GFX_H_NULL = 0, GFX_H_LEFT, GFX_H_RIGHT, GFX_H_CENTER };
enum gfx_v_align_en { GFX_V_NULL = 0, GFX_V_TOP, GFX_V_BOTTOM, GFX_V_CENTER };

/* cairo color components */
typedef struct gfx_color_t {
    double    red;
    double    green;
    double    blue;
    double    alpha;
} gfx_color_t;


/* create a new line */
void      gfx_line(
    cairo_t * cr,
    double X0,
    double Y0,
    double X1,
    double Y1,
    double width,
    gfx_color_t color);

void      gfx_dashed_line(
    cairo_t * cr,
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
    cairo_t * cr,
    double X0,
    double Y0,
    double X1,
    double Y1,
    double X2,
    double Y2,
    gfx_color_t color);

/* add a point to a line or to an area */
void      gfx_add_point(
    cairo_t * cr,
    double x,
    double y);

/* close current path so it ends at the same point as it started */
void      gfx_close_path(
    cairo_t * cr);


/* create a text node */
void      gfx_text(
    cairo_t * cr,
    double x,
    double y,
    gfx_color_t color,
    char *font,
    double size,
    double tabwidth,
    double angle,
    enum gfx_h_align_en h_align,
    enum gfx_v_align_en v_align,
    const char *text);

/* measure width of a text string */
double    gfx_get_text_width(
    cairo_t * cr,
    double start,
    char *font,
    double size,
    double tabwidth,
    char *text);


/* convert color */
gfx_color_t gfx_hex_to_col(
    long unsigned int);
void      gfx_line_fit(
    cairo_t *,
    double *,
    double *);
void      gfx_area_fit(
    cairo_t *,
    double *,
    double *);
#endif
