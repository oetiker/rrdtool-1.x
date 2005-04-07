/****************************************************************************
 * RRDtool 1.2rc5  Copyright by Tobi Oetiker, 1997-2005
 ****************************************************************************
 * rrd_gfx.c  graphics wrapper for rrdtool
  **************************************************************************/

/* #define DEBUG */

#ifdef DEBUG
# define DPRINT(x)    (void)(printf x, printf("\n"))
#else
# define DPRINT(x)
#endif
#include "rrd_tool.h"
#include <png.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "rrd_gfx.h"
#include "rrd_afm.h"

/* lines are better drawn on the pixle than between pixles */
#define LINEOFFSET 0.5

typedef struct gfx_char_s *gfx_char;
struct gfx_char_s {
  FT_UInt     index;    /* glyph index */
  FT_Vector   pos;      /* location from baseline in 26.6 */
  FT_Glyph    image;    /* glyph bitmap */
};

typedef struct gfx_string_s *gfx_string;
struct gfx_string_s {
  unsigned int    width;
  unsigned int    height;
  size_t          count;  /* number of characters */
  gfx_char        glyphs;
  size_t          num_glyphs;
  FT_BBox         bbox;
  FT_Matrix       transform;
};

/* compute string bbox */
static void compute_string_bbox(gfx_string string);

/* create a freetype glyph string */
gfx_string gfx_string_create ( FT_Face face,
                               const char *text, int rotation, double tabwidth);

/* create a freetype glyph string */
static void gfx_string_destroy ( gfx_string string );

static
gfx_node_t *gfx_new_node( gfx_canvas_t *canvas,enum gfx_en type){
  gfx_node_t *node = art_new(gfx_node_t,1);
  if (node == NULL) return NULL;
  node->type = type;
  node->color = 0x0;        /* color of element  0xRRGGBBAA  alpha 0xff is solid*/
  node->size =0.0;         /* font size, line width */
  node->path = NULL;        /* path */
  node->points = 0;
  node->points_max =0;
  node->closed_path = 0;
  node->svp = NULL;         /* svp */
  node->filename = NULL;             /* font or image filename */
  node->text = NULL;
  node->x = 0.0;
  node->y = 0.0;          /* position */
  node->angle = 0;  
  node->halign = GFX_H_NULL; /* text alignement */
  node->valign = GFX_V_NULL; /* text alignement */
  node->tabwidth = 0.0; 
  node->next = NULL; 
  if (canvas->lastnode != NULL){
      canvas->lastnode->next = node;
  }
  if (canvas->firstnode == NULL){
      canvas->firstnode = node;
  }  
  canvas->lastnode = node;
  return node;
}

gfx_canvas_t *gfx_new_canvas (void) {
    gfx_canvas_t *canvas = art_new(gfx_canvas_t,1);
    canvas->firstnode = NULL;
    canvas->lastnode = NULL;
    canvas->imgformat = IF_PNG; /* we default to PNG output */
    canvas->interlaced = 0;
    canvas->zoom = 1.0;
    return canvas;    
}

/* create a new line */
gfx_node_t  *gfx_new_line(gfx_canvas_t *canvas, 
			   double X0, double Y0, 
	 		   double X1, double Y1,
 			   double width, gfx_color_t color){
  return gfx_new_dashed_line(canvas, X0, Y0, X1, Y1, width, color, 0, 0);
}

gfx_node_t  *gfx_new_dashed_line(gfx_canvas_t *canvas, 
			   double X0, double Y0, 
	 		   double X1, double Y1,
 			   double width, gfx_color_t color,
			   double dash_on, double dash_off){

  gfx_node_t *node;
  ArtVpath *vec;
  node = gfx_new_node(canvas,GFX_LINE);
  if (node == NULL) return NULL;
  vec = art_new(ArtVpath, 3);
  if (vec == NULL) return NULL;
  vec[0].code = ART_MOVETO_OPEN; vec[0].x=X0+LINEOFFSET; vec[0].y=Y0+LINEOFFSET;
  vec[1].code = ART_LINETO; vec[1].x=X1+LINEOFFSET; vec[1].y=Y1+LINEOFFSET;
  vec[2].code = ART_END;
  
  node->points = 3;
  node->points_max = 3;
  node->color = color;
  node->size  = width;
  node->dash_on = dash_on;
  node->dash_off = dash_off;
  node->path  = vec;
  return node;
}

/* create a new area */
gfx_node_t   *gfx_new_area   (gfx_canvas_t *canvas, 
			      double X0, double Y0,
			      double X1, double Y1,
			      double X2, double Y2,
			      gfx_color_t color) {

  gfx_node_t *node;
  ArtVpath *vec;
  node = gfx_new_node(canvas,GFX_AREA);
  if (node == NULL) return NULL;
  vec = art_new(ArtVpath, 5);
  if (vec == NULL) return NULL;
  vec[0].code = ART_MOVETO; vec[0].x=X0; vec[0].y=Y0;
  vec[1].code = ART_LINETO; vec[1].x=X1; vec[1].y=Y1;
  vec[2].code = ART_LINETO; vec[2].x=X2; vec[2].y=Y2;
  vec[3].code = ART_LINETO; vec[3].x=X0; vec[3].y=Y0;
  vec[4].code = ART_END;
  
  node->points = 5;
  node->points_max = 5;
  node->color = color;
  node->path  = vec;

  return node;
}

/* add a point to a line or to an area */
int           gfx_add_point  (gfx_node_t *node, 
			      double x, double y){
  if (node == NULL) return 1;
  if (node->type == GFX_AREA) {
    double X0 = node->path[0].x;
    double Y0 = node->path[0].y;
    node->points -= 2;
    art_vpath_add_point (&(node->path),
                         &(node->points),
                         &(node->points_max),
                         ART_LINETO,
                         x,y);
    art_vpath_add_point (&(node->path),
                         &(node->points),
                         &(node->points_max),
                         ART_LINETO,
                         X0,Y0);
    art_vpath_add_point (&(node->path),
                         &(node->points),
                         &(node->points_max),
                         ART_END,
                         0,0);
  } else if (node->type == GFX_LINE) {
    node->points -= 1;
    art_vpath_add_point (&(node->path),
                         &(node->points),
                         &(node->points_max),
                         ART_LINETO,
                         x+LINEOFFSET,y+LINEOFFSET);
    art_vpath_add_point (&(node->path),
                         &(node->points),
                         &(node->points_max),
                         ART_END,
                         0,0);
    
  } else {
    /* can only add point to areas and lines */
    return 1;
  }
  return 0;
}

void           gfx_close_path  (gfx_node_t *node) {
    node->closed_path = 1;
    if (node->path[0].code == ART_MOVETO_OPEN)
	node->path[0].code = ART_MOVETO;
}

/* create a text node */
gfx_node_t   *gfx_new_text   (gfx_canvas_t *canvas,  
			      double x, double y, gfx_color_t color,
			      char* font, double size, 			      
			      double tabwidth, double angle,
			      enum gfx_h_align_en h_align,
			      enum gfx_v_align_en v_align,
                              char* text){
   gfx_node_t *node = gfx_new_node(canvas,GFX_TEXT);
   
   node->text = strdup(text);
   node->size = size;
   node->filename = strdup(font);
   node->x = x;
   node->y = y;
   node->angle = angle;   
   node->color = color;
   node->tabwidth = tabwidth;
   node->halign = h_align;
   node->valign = v_align;
#if 0
  /* debugging: show text anchor
     green is along x-axis, red is downward y-axis */
   if (1) {
     double a = 2 * M_PI * -node->angle / 360.0;
     double cos_a = cos(a);
     double sin_a = sin(a);
     double len = 3;
     gfx_new_line(canvas,
	 x, y,
	 x + len * cos_a, y - len * sin_a,
	 0.2, 0x00FF0000);
     gfx_new_line(canvas,
	 x, y,
	 x + len * sin_a, y + len * cos_a,
	 0.2, 0xFF000000);
   }
#endif
   return node;
}

int           gfx_render(gfx_canvas_t *canvas, 
			      art_u32 width, art_u32 height, 
			      gfx_color_t background, FILE *fp){
  switch (canvas->imgformat) {
  case IF_PNG: 
    return gfx_render_png (canvas, width, height, background, fp);
  case IF_SVG: 
    return gfx_render_svg (canvas, width, height, background, fp);
  case IF_EPS:
    return gfx_render_eps (canvas, width, height, background, fp);
  case IF_PDF:
    return gfx_render_pdf (canvas, width, height, background, fp);
  default:
    return -1;
  }
}

static void gfx_string_destroy ( gfx_string string ) {
  unsigned int n;
  if (string->glyphs) {
    for (n=0; n<string->num_glyphs; ++n)
      FT_Done_Glyph (string->glyphs[n].image);
    free (string->glyphs);
  }
  free (string);
}


double gfx_get_text_width ( gfx_canvas_t *canvas,
			    double start, char* font, double size,
			    double tabwidth, char* text, int rotation){
  switch (canvas->imgformat) {
  case IF_PNG: 
    return gfx_get_text_width_libart (start, font, size, tabwidth, text, rotation);
  case IF_SVG: /* fall through */ 
  case IF_EPS:
  case IF_PDF:
    return afm_get_text_width(start, font, size, tabwidth, text);
  default:
    return size * strlen(text);
  }
}

double gfx_get_text_width_libart (
			    double start, char* font, double size,
			    double tabwidth, char* text, int rotation){

  int           error;
  double        text_width=0;
  FT_Face       face;
  FT_Library    library=NULL;  
  gfx_string    string;

  FT_Init_FreeType( &library );
  error = FT_New_Face( library, font, 0, &face );
  if ( error ) return -1;
  error = FT_Set_Char_Size(face,  size*64,size*64,  100,100);
  if ( error ) return -1;

  string = gfx_string_create( face, text, rotation,tabwidth);
  text_width = string->width;
  gfx_string_destroy(string);
  FT_Done_FreeType(library);
  return text_width/64;
}

static void gfx_libart_close_path(gfx_node_t *node, ArtVpath **vec)
{
    /* libart must have end==start for closed paths,
       even if using ART_MOVETO and not ART_MOVETO_OPEN
       so add extra point which is the same as the starting point */
    int points_max = node->points; /* scaled array has exact size */
    int points = node->points - 1;
    art_vpath_add_point (vec, &points, &points_max, ART_LINETO,
	    (**vec).x, (**vec).y);
    art_vpath_add_point (vec, &points, &points_max, ART_END, 0, 0);
}

static void gfx_round_scaled_coordinates(ArtVpath *vec)
{
    while (vec->code != ART_END) {
	vec->x = floor(vec->x - LINEOFFSET + 0.5) + LINEOFFSET;
	vec->y = floor(vec->y - LINEOFFSET + 0.5) + LINEOFFSET;
	vec++;
    }
}

/* find bbox of a string */
static void compute_string_bbox(gfx_string string) {
    unsigned int n;
    FT_BBox bbox;

    bbox.xMin = bbox.yMin = 32000;
    bbox.xMax = bbox.yMax = -32000;
    for ( n = 0; n < string->num_glyphs; n++ ) {
      FT_BBox glyph_bbox;
      FT_Glyph_Get_CBox( string->glyphs[n].image, ft_glyph_bbox_gridfit,
       &glyph_bbox );
      if (glyph_bbox.xMin < bbox.xMin) {
         bbox.xMin = glyph_bbox.xMin;
      }
      if (glyph_bbox.yMin < bbox.yMin) {
        bbox.yMin = glyph_bbox.yMin;
      }
      if (glyph_bbox.xMax > bbox.xMax) {
         bbox.xMax = glyph_bbox.xMax;
      }
      if (glyph_bbox.yMax > bbox.yMax) {
         bbox.yMax = glyph_bbox.yMax;
      }
    }
    if ( bbox.xMin > bbox.xMax ) { 
      bbox.xMin = 0;
      bbox.yMin = 0;
      bbox.xMax = 0;
      bbox.yMax = 0;
    }
    string->bbox.xMin = bbox.xMin;
    string->bbox.xMax = bbox.xMax;
    string->bbox.yMin = bbox.yMin;
    string->bbox.yMax = bbox.yMax;
} 

/* create a free type glyph string */
gfx_string gfx_string_create(FT_Face face,const char *text,
        int rotation, double tabwidth)
{

  FT_GlyphSlot  slot = face->glyph;  /* a small shortcut */
  FT_Bool       use_kerning;
  FT_UInt       previous;
  FT_Vector     ft_pen;

  gfx_string    string;
  gfx_char      glyph;          /* current glyph in table */
  unsigned int  n;
  int           error;
  int        gottab = 0;    

  ft_pen.x = 0;   /* start at (0,0) !! */
  ft_pen.y = 0;

  string = (gfx_string) malloc (sizeof(struct gfx_string_s));
  string->width = 0;
  string->height = 0;
  string->count = strlen (text);
  string->glyphs = (gfx_char) calloc (string->count,sizeof(struct gfx_char_s));
  string->num_glyphs = 0;
  string->transform.xx = (FT_Fixed)( cos(M_PI*(rotation)/180.0)*0x10000);
  string->transform.xy = (FT_Fixed)(-sin(M_PI*(rotation)/180.0)*0x10000);
  string->transform.yx = (FT_Fixed)( sin(M_PI*(rotation)/180.0)*0x10000);
  string->transform.yy = (FT_Fixed)( cos(M_PI*(rotation)/180.0)*0x10000);

  use_kerning = FT_HAS_KERNING(face);
  previous    = 0;
  glyph = string->glyphs;
  for (n=0; n<string->count; n++, glyph++) {
    FT_Vector   vec;
    /* handle the tabs ...
       have a witespace glyph inserted, but set its width such that the distance
    of the new right edge is x times tabwidth from 0,0 where x is an integer. */    
    char letter = text[n];
    gottab = 0;
    if (letter == '\\' && n+1 < string->count && text[n+1] == 't'){
            /* we have a tab here so skip the backslash and
               set t to ' ' so that we get a white space */
            gottab = 1;
            n++;
            letter  = ' ';            
    }            
    if (letter == '\t'){
	letter = ' ';
        gottab = 1 ;
    }            
    /* initialize each struct gfx_char_s */
    glyph->index = 0;
    glyph->pos.x = 0;
    glyph->pos.y = 0;
    glyph->image = NULL;

    glyph->index = FT_Get_Char_Index( face, letter );

    /* compute glyph origin */
    if ( use_kerning && previous && glyph->index ) {
      FT_Vector kerning;
      FT_Get_Kerning (face, previous, glyph->index,
          ft_kerning_default, &kerning);
      ft_pen.x += kerning.x;
      ft_pen.y += kerning.y;
    }

    /* load the glyph image (in its native format) */
    /* for now, we take a monochrome glyph bitmap */
    error = FT_Load_Glyph (face, glyph->index, FT_LOAD_DEFAULT);
    if (error) {
      fprintf (stderr, "couldn't load glyph:  %c\n", letter);
      continue;
    }
    error = FT_Get_Glyph (slot, &glyph->image);
    if (error) {
      fprintf (stderr, "couldn't get glyph from slot:  %c\n", letter);
      continue;
    }
    /* if we are in tabbing mode, we replace the tab with a space and shift the position
       of the space so that its left edge is where the tab was supposed to land us */
    if (gottab){
       /* we are in gridfitting mode so the calculations happen in 1/64 pixles */
        ft_pen.x = tabwidth*64.0 * (float)(1 + (long)(ft_pen.x / (tabwidth * 64.0))) - slot->advance.x;
    }
    /* store current pen position */
    glyph->pos.x = ft_pen.x;
    glyph->pos.y = ft_pen.y;


    ft_pen.x   += slot->advance.x;    
    ft_pen.y   += slot->advance.y;

    /* rotate glyph */
    vec = glyph->pos;
    FT_Vector_Transform (&vec, &string->transform);
    error = FT_Glyph_Transform (glyph->image, &string->transform, &vec);
    if (error) {
      fprintf (stderr, "couldn't transform glyph\n");
      continue;
    }

    /* convert to a bitmap - destroy native image */
    error = FT_Glyph_To_Bitmap (&glyph->image, FT_RENDER_MODE_NORMAL, 0, 1);
    if (error) {
      fprintf (stderr, "couldn't convert glyph to bitmap\n");
      continue;
    }

    /* increment number of glyphs */
    previous = glyph->index;
    string->num_glyphs++;
  }
/*  printf ("number of glyphs = %d\n", string->num_glyphs);*/
  compute_string_bbox( string );
  /* the last character was a tab */  
  if (gottab) {
      string->width = ft_pen.x;
  } else {
      string->width = string->bbox.xMax - string->bbox.xMin;
  }
  string->height = string->bbox.yMax - string->bbox.yMin;
  return string;
}


static int gfx_save_png (art_u8 *buffer, FILE *fp,
                     long width, long height, long bytes_per_pixel);
/* render grafics into png image */

int           gfx_render_png (gfx_canvas_t *canvas, 
			      art_u32 width, art_u32 height, 
			      gfx_color_t background, FILE *fp){
    
    
    FT_Library    library;
    gfx_node_t *node = canvas->firstnode;    
    art_u8 red = background >> 24, green = (background >> 16) & 0xff;
    art_u8 blue = (background >> 8) & 0xff, alpha = ( background & 0xff );
    unsigned long pys_width = width * canvas->zoom;
    unsigned long pys_height = height * canvas->zoom;
    const int bytes_per_pixel = 3;
    unsigned long rowstride = pys_width*bytes_per_pixel; /* bytes per pixel */
    art_u8 *buffer = art_new (art_u8, rowstride*pys_height);
    art_rgb_run_alpha (buffer, red, green, blue, alpha, pys_width*pys_height);
    FT_Init_FreeType( &library );
    while(node){
        switch (node->type) {
        case GFX_LINE:
        case GFX_AREA: {   
            ArtVpath *vec,*pvec;
            double dst[6];     
            ArtSVP *svp,*svpt;
            art_affine_scale(dst,canvas->zoom,canvas->zoom);
            vec = art_vpath_affine_transform(node->path,dst);
	    if (node->closed_path)
		gfx_libart_close_path(node, &vec);
	    /* gfx_round_scaled_coordinates(vec); */
            pvec = art_vpath_perturb(vec);
	    art_free(vec);
            if(node->type == GFX_LINE){
                svp = art_svp_vpath_stroke ( pvec, ART_PATH_STROKE_JOIN_ROUND,
                                             ART_PATH_STROKE_CAP_ROUND,
                                             node->size*canvas->zoom,4,0.25);
            } else {
                svp  = art_svp_from_vpath ( pvec );
                svpt = art_svp_uncross( svp );
                art_free(svp);
	        svp  = art_svp_rewind_uncrossed(svpt,ART_WIND_RULE_NONZERO); 
                art_free(svpt);
            }
            art_free(pvec);
            art_rgb_svp_alpha (svp ,0,0, pys_width, pys_height,
                               node->color, buffer, rowstride, NULL);
            art_free(svp);
            break;
        }
        case GFX_TEXT: {
            unsigned int  n;
            int  error;
            art_u8 fcolor[3],falpha;
            FT_Face       face;
            gfx_char      glyph;
            gfx_string    string;
            FT_Vector     vec;  /* 26.6 */

            float pen_x = 0.0 , pen_y = 0.0;
            /* double x,y; */
            long   ix,iy,iz;
            
            fcolor[0] = node->color >> 24;
            fcolor[1] = (node->color >> 16) & 0xff;
            fcolor[2] = (node->color >> 8) & 0xff;
            falpha = node->color & 0xff;
            error = FT_New_Face( library,
                                 (char *)node->filename,
                                 0,
                                 &face );
	    if ( error ) break;

            error = FT_Set_Char_Size(face,   /* handle to face object            */
                                     (long)(node->size*64),
                                     (long)(node->size*64),
                                     (long)(100*canvas->zoom),
                                     (long)(100*canvas->zoom));
            if ( error ) break;
            pen_x = node->x * canvas->zoom;
            pen_y = node->y * canvas->zoom;

            string = gfx_string_create (face, node->text, node->angle, node->tabwidth);
            switch(node->halign){
            case GFX_H_RIGHT:  vec.x = -string->bbox.xMax;
                               break;          
            case GFX_H_CENTER: vec.x = abs(string->bbox.xMax) >= abs(string->bbox.xMin) ?
                                       -string->bbox.xMax/2:-string->bbox.xMin/2;
                               break;          
            case GFX_H_LEFT:   vec.x = -string->bbox.xMin;
			       break;
            case GFX_H_NULL:   vec.x = 0;
                               break;          
            }

            switch(node->valign){
            case GFX_V_TOP:    vec.y = string->bbox.yMax;
                               break;
            case GFX_V_CENTER: vec.y = abs(string->bbox.yMax) >= abs(string->bbox.yMin) ?
                                       string->bbox.yMax/2:string->bbox.yMin/2;
                               break;
            case GFX_V_BOTTOM: vec.y = 0;
                               break;
            case GFX_V_NULL:   vec.y = 0;
                               break;
            }
	    pen_x += vec.x/64;
	    pen_y += vec.y/64;
            glyph = string->glyphs;
            for(n=0; n<string->num_glyphs; ++n, ++glyph) {
                int gr;
                FT_Glyph        image;
                FT_BitmapGlyph  bit;

	        /* make copy to transform */
                if (! glyph->image) {
                  fprintf (stderr, "no image\n");
                  continue;
                }
                error = FT_Glyph_Copy (glyph->image, &image);
                if (error) {
                  fprintf (stderr, "couldn't copy image\n");
                  continue;
                }

                /* transform it */
                vec = glyph->pos;
                FT_Vector_Transform (&vec, &string->transform);

                bit = (FT_BitmapGlyph) image;

                gr = bit->bitmap.num_grays -1;
                for (iy=0; iy < bit->bitmap.rows; iy++){
                    long buf_y = iy+(pen_y+0.5)-bit->top;
                    if (buf_y < 0 || buf_y >= (long)pys_height) continue;
                    buf_y *= rowstride;
                    for (ix=0;ix < bit->bitmap.width;ix++){
                        long buf_x = ix + (pen_x + 0.5) + (double)bit->left ;
                        art_u8 font_alpha;
                        
                        if (buf_x < 0 || buf_x >= (long)pys_width) continue;
                        buf_x *=  bytes_per_pixel ;
                        font_alpha =  *(bit->bitmap.buffer + iy * bit->bitmap.width + ix);
                        font_alpha =  (art_u8)((double)font_alpha / gr * falpha);
                        for (iz = 0; iz < 3; iz++){
                            art_u8 *orig = buffer + buf_y + buf_x + iz;
                            *orig =  (art_u8)((double)*orig / gr * ( gr - font_alpha) +
                                              (double)fcolor[iz] / gr * (font_alpha));
                        }
                    }
                }
                FT_Done_Glyph (image);
            }
            gfx_string_destroy(string);
        }
        }
        node = node->next;
    }  
    gfx_save_png(buffer,fp , pys_width,pys_height,bytes_per_pixel);
    art_free(buffer);
    FT_Done_FreeType( library );
    return 0;    
}

/* free memory used by nodes this will also remove memory required for
   associated paths and svcs ... but not for text strings */
int
gfx_destroy    (gfx_canvas_t *canvas){  
  gfx_node_t *next,*node = canvas->firstnode;
  while(node){
    next = node->next;
    art_free(node->path);
    art_free(node->svp);
    free(node->text);
    free(node->filename);
    art_free(node);
    node = next;
  }
  return 0;
}
 
static int gfx_save_png (art_u8 *buffer, FILE *fp,  long width, long height, long bytes_per_pixel){
  png_structp png_ptr = NULL;
  png_infop   info_ptr = NULL;
  int i;
  png_bytep *row_pointers;
  int rowstride = width * bytes_per_pixel;
  png_text text[2];
  
  if (fp == NULL)
    return (1);

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
  if (png_ptr == NULL)
   {
      return (1);
   }
   row_pointers = (png_bytepp)png_malloc(png_ptr,
                                     height*sizeof(png_bytep));

  info_ptr = png_create_info_struct(png_ptr);

  if (info_ptr == NULL)
    {
      png_destroy_write_struct(&png_ptr,  (png_infopp)NULL);
      return (1);
    }

  if (setjmp(png_jmpbuf(png_ptr)))
    {
      /* If we get here, we had a problem writing the file */
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return (1);
    }

  png_init_io(png_ptr, fp);
  png_set_IHDR (png_ptr, info_ptr,width, height,
                8, PNG_COLOR_TYPE_RGB,
                PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);

  text[0].key = "Software";
  text[0].text = "RRDtool, Tobias Oetiker <tobi@oetike.ch>, http://tobi.oetiker.ch";
  text[0].compression = PNG_TEXT_COMPRESSION_NONE;
  png_set_text (png_ptr, info_ptr, text, 1);

  /* lets make this fast */
  png_set_compression_level(png_ptr,1);
  png_set_filter(png_ptr,PNG_FILTER_TYPE_BASE,PNG_NO_FILTERS);
  /* 
  png_set_filter(png_ptr,PNG_FILTER_TYPE_BASE,PNG_FILTER_SUB);
  png_set_compression_strategy(png_ptr,Z_HUFFMAN_ONLY);
  png_set_compression_level(png_ptr,Z_BEST_SPEED); */
  
  /* Write header data */
  png_write_info (png_ptr, info_ptr);
  for (i = 0; i < height; i++)
    row_pointers[i] = (png_bytep) (buffer + i*rowstride);
  
  png_write_image(png_ptr, row_pointers);
  png_write_end(png_ptr, info_ptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  return 1;
}

 
/* ------- SVG -------
   SVG reference:
   http://www.w3.org/TR/SVG/
*/
static int svg_indent = 0;
static int svg_single_line = 0;
static const char *svg_default_font = "Helvetica";
typedef struct svg_dash
{
  int dash_enable;
  double dash_adjust, dash_len, dash_offset;
  double adjusted_on, adjusted_off;
} svg_dash;


static void svg_print_indent(FILE *fp)
{
  int i;
   for (i = svg_indent - svg_single_line; i > 0; i--) {
     putc(' ', fp);
     putc(' ', fp);
   }
}
 
static void svg_start_tag(FILE *fp, const char *name)
{
   svg_print_indent(fp);
   putc('<', fp);
   fputs(name, fp);
   svg_indent++;
}
 
static void svg_close_tag_single_line(FILE *fp)
{
   svg_single_line++;
   putc('>', fp);
}
 
static void svg_close_tag(FILE *fp)
{
   putc('>', fp);
   if (!svg_single_line)
     putc('\n', fp);
}
 
static void svg_end_tag(FILE *fp, const char *name)
{
   /* name is NULL if closing empty-node tag */
   svg_indent--;
   if (svg_single_line)
     svg_single_line--;
   else if (name)
     svg_print_indent(fp);
   if (name != NULL) {
     fputs("</", fp);
     fputs(name, fp);
   } else {
     putc('/', fp);
   }
   svg_close_tag(fp);
}
 
static void svg_close_tag_empty_node(FILE *fp)
{
   svg_end_tag(fp, NULL);
}
 
static void svg_write_text(FILE *fp, const char *text)
{
   const unsigned char *p, *start, *last;
   unsigned int ch;
   p = (const unsigned char*)text;
   if (!p)
     return;
   /* trim leading spaces */
   while (*p == ' ')
     p++;
   start = p;
   /* trim trailing spaces */
   last = p - 1;
   while ((ch = *p) != 0) {
     if (ch != ' ')
       last = p;
     p++;
  }
  /* encode trimmed text */
  p = start;
  while (p <= last) {
    ch = *p++;
    ch = afm_host2unicode(ch); /* unsafe macro */
    switch (ch) {
    case '&': fputs("&amp;", fp); break;
    case '<': fputs("&lt;", fp); break;
    case '>': fputs("&gt;", fp); break;
    case '"': fputs("&quot;", fp); break;
    default:
      if (ch >= 127)
	fprintf(fp, "&#%d;", ch);
      else
	putc(ch, fp);
     }
   }
}
 
static void svg_format_number(char *buf, int bufsize, double d)
{
   /* omit decimals if integer to reduce filesize */
   char *p;
   snprintf(buf, bufsize, "%.2f", d);
   p = buf; /* doesn't trust snprintf return value */
   while (*p)
     p++;
   while (--p > buf) {
     char ch = *p;
     if (ch == '0') {
       *p = '\0'; /* zap trailing zeros */
       continue;
     }
     if (ch == '.')
       *p = '\0'; /* zap trailing dot */
     break;
   }
}
 
static void svg_write_number(FILE *fp, double d)
{
   char buf[60];
   svg_format_number(buf, sizeof(buf), d);
   fputs(buf, fp);
}

static int svg_color_is_black(int c)
{
  /* gfx_color_t is RRGGBBAA */
  return c == 0x000000FF;
}
 
static void svg_write_color(FILE *fp, gfx_color_t c, const char *attr)
{
  /* gfx_color_t is RRGGBBAA, svg can use #RRGGBB and #RGB like html */
  gfx_color_t rrggbb = (int)((c >> 8) & 0xFFFFFF);
  gfx_color_t opacity = c & 0xFF;
  fprintf(fp, " %s=\"", attr);
  if ((rrggbb & 0x0F0F0F) == ((rrggbb >> 4) & 0x0F0F0F)) {
     /* css2 short form, #rgb is #rrggbb, not #r0g0b0 */
    fprintf(fp, "#%03lX",
          ( ((rrggbb >> 8) & 0xF00)
          | ((rrggbb >> 4) & 0x0F0)
          | ( rrggbb       & 0x00F)));
   } else {
    fprintf(fp, "#%06lX", rrggbb);
   }
  fputs("\"", fp);
  if (opacity != 0xFF) {
    fprintf(fp, " stroke-opacity=\"");
    svg_write_number(fp, opacity / 255.0);
    fputs("\"", fp);
 }
}
 
static void svg_get_dash(gfx_node_t *node, svg_dash *d)
{
  double offset;
  int mult;
  if (node->dash_on <= 0 || node->dash_off <= 0) {
    d->dash_enable = 0;
    return;
  }
  d->dash_enable = 1;
  d->dash_len = node->dash_on + node->dash_off;
  /* dash on/off adjustment due to round caps */
  d->dash_adjust = 0.8 * node->size;
  d->adjusted_on = node->dash_on - d->dash_adjust;
  if (d->adjusted_on < 0.01)
      d->adjusted_on = 0.01;
  d->adjusted_off = d->dash_len - d->adjusted_on;
  /* dash offset calc */
  if (node->path[0].x == node->path[1].x) /* only good for horz/vert lines */
    offset = node->path[0].y;
  else
    offset = node->path[0].x;
  mult = (int)fabs(offset / d->dash_len);
  d->dash_offset = offset - mult * d->dash_len;
  if (node->path[0].x < node->path[1].x || node->path[0].y < node->path[1].y)
    d->dash_offset = d->dash_len - d->dash_offset;
}

static int svg_dash_equal(svg_dash *a, svg_dash *b)
{
  if (a->dash_enable != b->dash_enable)
    return 0;
  if (a->adjusted_on != b->adjusted_on)
    return 0;
  if (a->adjusted_off != b->adjusted_off)
    return 0;
  /* rest of properties will be the same when on+off are */
  return 1;
}

static void svg_common_path_attributes(FILE *fp, gfx_node_t *node)
{
  svg_dash dash_info;
  svg_get_dash(node, &dash_info);
  fputs(" stroke-width=\"", fp);
  svg_write_number(fp, node->size);
  fputs("\"", fp);
  svg_write_color(fp, node->color, "stroke");
  fputs(" fill=\"none\"", fp);
  if (dash_info.dash_enable) {
    if (dash_info.dash_offset != 0) {
      fputs(" stroke-dashoffset=\"", fp);
      svg_write_number(fp, dash_info.dash_offset);
      fputs("\"", fp);
    }
    fputs(" stroke-dasharray=\"", fp);
    svg_write_number(fp, dash_info.adjusted_on);
    fputs(",", fp);
    svg_write_number(fp, dash_info.adjusted_off);
    fputs("\"", fp);
  }
}

static int svg_is_int_step(double a, double b)
{
   double diff = fabs(a - b);
   return floor(diff) == diff;
}
 
static int svg_path_straight_segment(FILE *fp,
     double lastA, double currentA, double currentB,
     gfx_node_t *node,
     int segment_idx, int isx, char absChar, char relChar)
{
   if (!svg_is_int_step(lastA, currentA)) {
     putc(absChar, fp);
     svg_write_number(fp, currentA);
     return 0;
   }
   if (segment_idx < node->points - 1) {
     ArtVpath *vec = node->path + segment_idx + 1;
     if (vec->code == ART_LINETO) {
       double nextA = (isx ? vec->x : vec->y) - LINEOFFSET;
       double nextB = (isx ? vec->y : vec->x) - LINEOFFSET;
       if (nextB == currentB
           && ((currentA >= lastA) == (nextA >= currentA))
           && svg_is_int_step(currentA, nextA)) {
         return 1; /* skip to next as it is a straight line  */
       }
     }
   }
   putc(relChar, fp);
   svg_write_number(fp, currentA - lastA);
   return 0;
}
 
static void svg_path(FILE *fp, gfx_node_t *node, int multi)
{
   int i;
   double lastX = 0, lastY = 0;
   /* for straight lines <path..> tags take less space than
      <line..> tags because of the efficient packing
      in the 'd' attribute */
   svg_start_tag(fp, "path");
  if (!multi)
    svg_common_path_attributes(fp, node);
   fputs(" d=\"", fp);
   /* specification of the 'd' attribute: */
   /* http://www.w3.org/TR/SVG/paths.html#PathDataGeneralInformation */
   for (i = 0; i < node->points; i++) {
     ArtVpath *vec = node->path + i;
     double x = vec->x - LINEOFFSET;
     double y = vec->y - LINEOFFSET;
     switch (vec->code) {
     case ART_MOVETO_OPEN: /* fall-through */
     case ART_MOVETO:
       putc('M', fp);
       svg_write_number(fp, x);
       putc(',', fp);
       svg_write_number(fp, y);
       break;
     case ART_LINETO:
       /* try optimize filesize by using minimal lineto commands */
       /* without introducing rounding errors. */
       if (x == lastX) {
         if (svg_path_straight_segment(fp, lastY, y, x, node, i, 0, 'V', 'v'))
           continue;
       } else if (y == lastY) {
         if (svg_path_straight_segment(fp, lastX, x, y, node, i, 1, 'H', 'h'))
           continue;
       } else {
         putc('L', fp);
         svg_write_number(fp, x);
         putc(',', fp);
         svg_write_number(fp, y);
       }
       break;
     case ART_CURVETO: break; /* unsupported */
     case ART_END: break; /* nop */
     }
     lastX = x;
     lastY = y;
   }
  if (node->closed_path)
    fputs(" Z", fp);
   fputs("\"", fp);
   svg_close_tag_empty_node(fp);
}
 
static void svg_multi_path(FILE *fp, gfx_node_t **nodeR)
{
   /* optimize for multiple paths with the same color, penwidth, etc. */
   int num = 1;
   gfx_node_t *node = *nodeR;
   gfx_node_t *next = node->next;
   while (next) {
     if (next->type != node->type
         || next->size != node->size
        || next->color != node->color
        || next->dash_on != node->dash_on
        || next->dash_off != node->dash_off)
       break;
     next = next->next;
     num++;
   }
   if (num == 1) {
     svg_path(fp, node, 0);
     return;
   }
   svg_start_tag(fp, "g");
  svg_common_path_attributes(fp, node);
   svg_close_tag(fp);
   while (num && node) {
     svg_path(fp, node, 1);
     if (!--num)
       break;
     node = node->next;
     *nodeR = node;
   }
   svg_end_tag(fp, "g");
}
 
static void svg_area(FILE *fp, gfx_node_t *node)
{
   int i;
   double startX = 0, startY = 0;
   svg_start_tag(fp, "polygon");
  fputs(" ", fp);
  svg_write_color(fp, node->color, "fill");
  fputs(" points=\"", fp);
   for (i = 0; i < node->points; i++) {
     ArtVpath *vec = node->path + i;
     double x = vec->x - LINEOFFSET;
     double y = vec->y - LINEOFFSET;
     switch (vec->code) {
       case ART_MOVETO_OPEN: /* fall-through */
       case ART_MOVETO:
         svg_write_number(fp, x);
         putc(',', fp);
         svg_write_number(fp, y);
         startX = x;
         startY = y;
         break;
       case ART_LINETO:
         if (i == node->points - 2
 			&& node->path[i + 1].code == ART_END
             && fabs(x - startX) < 0.001 && fabs(y - startY) < 0.001) {
           break; /* poly area always closed, no need for last point */
         }
         putc(' ', fp);
         svg_write_number(fp, x);
         putc(',', fp);
         svg_write_number(fp, y);
         break;
       case ART_CURVETO: break; /* unsupported */
       case ART_END: break; /* nop */
     }
   }
   fputs("\"", fp);
   svg_close_tag_empty_node(fp);
}
 
static void svg_text(FILE *fp, gfx_node_t *node)
{
   double x = node->x - LINEOFFSET;
   double y = node->y - LINEOFFSET;
   if (node->angle != 0) {
     svg_start_tag(fp, "g");
     fputs(" transform=\"translate(", fp);
     svg_write_number(fp, x);
     fputs(",", fp);
     svg_write_number(fp, y);
     fputs(") rotate(", fp);
     svg_write_number(fp, node->angle);
     fputs(")\"", fp);
     x = y = 0;
     svg_close_tag(fp);
   }
   switch (node->valign) {
   case GFX_V_TOP:  y += node->size; break;
   case GFX_V_CENTER: y += node->size / 3; break;
   case GFX_V_BOTTOM: break;
   case GFX_V_NULL: break;
   }
   svg_start_tag(fp, "text");
   fputs(" x=\"", fp);
   svg_write_number(fp, x);
   fputs("\" y=\"", fp);
   svg_write_number(fp, y);

/*  if (strcmp(node->filename, svg_default_font))
    fprintf(fp, " font-family=\"%s\"", node->filename);
    */
   fputs("\" font-family=\"Helvetica", fp);
   fputs("\" font-size=\"", fp);
   svg_write_number(fp, node->size);
   fputs("\"", fp);
  if (!svg_color_is_black(node->color))
    svg_write_color(fp, node->color, "fill");
   switch (node->halign) {
   case GFX_H_RIGHT:  fputs(" text-anchor=\"end\"", fp); break;
   case GFX_H_CENTER: fputs(" text-anchor=\"middle\"", fp); break;
   case GFX_H_LEFT: break;
   case GFX_H_NULL: break;
   }
   svg_close_tag_single_line(fp);
   /* support for node->tabwidth missing */
   svg_write_text(fp, node->text);
   svg_end_tag(fp, "text");
   if (node->angle != 0)
     svg_end_tag(fp, "g");
}
 
int       gfx_render_svg (gfx_canvas_t *canvas,
                 art_u32 width, art_u32 height,
                 gfx_color_t background, FILE *fp){
   gfx_node_t *node = canvas->firstnode;
   fputs(
"<?xml version=\"1.0\" standalone=\"no\"?>\n"
"<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\"\n"
"   \"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\">\n"
"<!--\n"
"   SVG file created by RRDtool,\n"
"   Tobias Oetiker <tobi@oetike.ch>, http://tobi.oetiker.ch\n"
"\n"
"   The width/height attributes in the outhermost svg node\n"
"   are just default sizes for the browser which is used\n"
"   if the svg file is openened directly without being\n"
"   embedded in an html file.\n"
"   The viewBox is the local coord system for rrdtool.\n"
"-->\n", fp);
   svg_start_tag(fp, "svg");
   fputs(" width=\"", fp);
  svg_write_number(fp, width * canvas->zoom);
   fputs("\" height=\"", fp);
  svg_write_number(fp, height * canvas->zoom);
   fputs("\" x=\"0\" y=\"0\" viewBox=\"", fp);
   svg_write_number(fp, -LINEOFFSET);
   fputs(" ", fp);
   svg_write_number(fp, -LINEOFFSET);
   fputs(" ", fp);
   svg_write_number(fp, width - LINEOFFSET);
   fputs(" ", fp);
   svg_write_number(fp, height - LINEOFFSET);
   fputs("\" preserveAspectRatio=\"xMidYMid\"", fp);
  fprintf(fp, " font-family=\"%s\"", svg_default_font); /* default font */
  fputs(" stroke-linecap=\"round\" stroke-linejoin=\"round\"", fp);
   svg_close_tag(fp);
   svg_start_tag(fp, "rect");
   fprintf(fp, " x=\"0\" y=\"0\" width=\"%d\" height=\"%d\"", width, height);
  svg_write_color(fp, background, "fill");
   svg_close_tag_empty_node(fp);
   while (node) {
     switch (node->type) {
     case GFX_LINE:
       svg_multi_path(fp, &node);
       break;
     case GFX_AREA:
       svg_area(fp, node);
       break;
     case GFX_TEXT:
       svg_text(fp, node);
     }
     node = node->next;
   }
   svg_end_tag(fp, "svg");
   return 0;
}

/* ------- EPS -------
   EPS and Postscript references:
   http://partners.adobe.com/asn/developer/technotes/postscript.html
*/

typedef struct eps_font
{
  const char *ps_font;
  int id;
  struct eps_font *next;
} eps_font;

typedef struct eps_state
{
  FILE *fp;
  gfx_canvas_t *canvas;
  art_u32 page_width, page_height;
  eps_font *font_list;
  /*--*/
  gfx_color_t color;
  const char *font;
  double font_size;
  double line_width;
  int linecap, linejoin;
  int has_dash;
} eps_state;

static void eps_set_color(eps_state *state, gfx_color_t color)
{
   /* gfx_color_t is RRGGBBAA */
  if (state->color == color)
    return;
  fprintf(state->fp, "%d %d %d Rgb\n",
      (int)((color >> 24) & 255),
      (int)((color >> 16) & 255),
      (int)((color >>  8) & 255));
  state->color = color;
}

static int eps_add_font(eps_state *state, gfx_node_t *node)
{
  /* The fonts list could be postponed to the end using
     (atend), but let's be nice and have them in the header. */
  const char *ps_font = afm_get_font_postscript_name(node->filename);
  eps_font *ef;
  for (ef = state->font_list; ef; ef = ef->next) {
    if (!strcmp(ps_font, ef->ps_font))
      return 0;
  }
  ef = malloc(sizeof(eps_font));
  if (ef == NULL) {
    rrd_set_error("malloc for eps_font");
    return -1;
  }
  ef->next = state->font_list;
  ef->ps_font = ps_font;
  state->font_list = ef;
  return 0;
}

static void eps_list_fonts(eps_state *state, const char *dscName)
{
  eps_font *ef;
  int lineLen = strlen(dscName);
  if (!state->font_list)
    return;
  fputs(dscName, state->fp);
  for (ef = state->font_list; ef; ef = ef->next) {
    int nameLen = strlen(ef->ps_font);
    if (lineLen + nameLen > 100 && lineLen) {
      fputs("\n", state->fp);
      fputs("%%- \n", state->fp);
      lineLen = 5;
    } else {
      fputs(" ", state->fp);
      lineLen++;
    }
    fputs(ef->ps_font, state->fp);
    lineLen += nameLen;
  }
  fputs("\n", state->fp);
}

static void eps_define_fonts(eps_state *state)
{
  eps_font *ef;
  if (!state->font_list)
    return;
  for (ef = state->font_list; ef; ef = ef->next) {
    /* PostScript¨ LANGUAGE REFERENCE third edition
       page 349 */
    fprintf(state->fp,
        "%%\n"
        "/%s findfont dup length dict begin\n"
        "{ 1 index /FID ne {def} {pop pop} ifelse } forall\n"
        "/Encoding ISOLatin1Encoding def\n"
        "currentdict end\n"
        "/%s-ISOLatin1 exch definefont pop\n"
        "/SetFont-%s { /%s-ISOLatin1 findfont exch scalefont setfont } bd\n",
        ef->ps_font, ef->ps_font, ef->ps_font, ef->ps_font);
  }
}

static int eps_prologue(eps_state *state)
{
  gfx_node_t *node;
  fputs(
    "%!PS-Adobe-3.0 EPSF-3.0\n"
    "%%Creator: RRDtool 1.2rc5 Tobias Oetiker, http://tobi.oetiker.ch\n"
    /* can't like weird chars here */
    "%%Title: (RRDtool output)\n"
    "%%DocumentData: Clean7Bit\n"
    "", state->fp);
  fprintf(state->fp, "%%%%BoundingBox: 0 0 %d %d\n",
    state->page_width, state->page_height);
  for (node = state->canvas->firstnode; node; node = node->next) {
    if (node->type == GFX_TEXT && eps_add_font(state, node) == -1)
      return -1;
  }
  eps_list_fonts(state, "%%DocumentFonts:");
  eps_list_fonts(state, "%%DocumentNeededFonts:");
  fputs(
      "%%EndComments\n"
      "%%BeginProlog\n"
      "%%EndProlog\n" /* must have, or BoundingBox is ignored */
      "/bd { bind def } bind def\n"
      "", state->fp);
  fprintf(state->fp, "/X { %.2f add } bd\n", LINEOFFSET);
  fputs(
      "/X2 {X exch X exch} bd\n"
      "/M {X2 moveto} bd\n"
      "/L {X2 lineto} bd\n"
      "/m {moveto} bd\n"
      "/l {lineto} bd\n"
      "/S {stroke} bd\n"
      "/CP {closepath} bd\n"
      "/WS {setlinewidth stroke} bd\n"
      "/F {fill} bd\n"
      "/TaL { } bd\n"
      "/TaC {dup stringwidth pop neg 2 div 0 rmoveto } bd\n"
      "/TaR {dup stringwidth pop neg 0 rmoveto } bd\n"
      "/TL {moveto TaL show} bd\n"
      "/TC {moveto TaC show} bd\n"
      "/TR {moveto TaR show} bd\n"
      "/Rgb { 255.0 div 3 1 roll\n"
      "       255.0 div 3 1 roll \n"
      "       255.0 div 3 1 roll setrgbcolor } bd\n"
      "", state->fp);
  eps_define_fonts(state);
  return 0;
}

static void eps_clear_dash(eps_state *state)
{
  if (!state->has_dash)
    return;
  state->has_dash = 0;
  fputs("[1 0] 0 setdash\n", state->fp);
}

static void eps_write_linearea(eps_state *state, gfx_node_t *node)
{
  int i;
  FILE *fp = state->fp;
  int useOffset = 0;
  int clearDashIfAny = 1;
  eps_set_color(state, node->color);
  if (node->type == GFX_LINE) {
    svg_dash dash_info;
    if (state->linecap != 1) {
      fputs("1 setlinecap\n", fp);
      state->linecap = 1;
    }
    if (state->linejoin != 1) {
      fputs("1 setlinejoin\n", fp);
      state->linejoin = 1;
    }
    svg_get_dash(node, &dash_info);
    if (dash_info.dash_enable) {
      clearDashIfAny = 0;
      state->has_dash = 1;
      fputs("[", fp);
      svg_write_number(fp, dash_info.adjusted_on);
      fputs(" ", fp);
      svg_write_number(fp, dash_info.adjusted_off);
      fputs("] ", fp);
      svg_write_number(fp, dash_info.dash_offset);
      fputs(" setdash\n", fp);
    }
  }
  if (clearDashIfAny)
    eps_clear_dash(state);
  for (i = 0; i < node->points; i++) {
    ArtVpath *vec = node->path + i;
    double x = vec->x;
    double y = state->page_height - vec->y;
    if (vec->code == ART_MOVETO_OPEN || vec->code == ART_MOVETO)
      useOffset = (fabs(x - floor(x) - 0.5) < 0.01 && fabs(y - floor(y) - 0.5) < 0.01);
    if (useOffset) {
      x -= LINEOFFSET;
      y -= LINEOFFSET;
    }
    switch (vec->code) {
    case ART_MOVETO_OPEN: /* fall-through */
    case ART_MOVETO:
      svg_write_number(fp, x);
      fputc(' ', fp);
      svg_write_number(fp, y);
      fputc(' ', fp);
      fputs(useOffset ? "M\n" : "m\n", fp);
      break;
    case ART_LINETO:
      svg_write_number(fp, x);
      fputc(' ', fp);
      svg_write_number(fp, y);
      fputc(' ', fp);
      fputs(useOffset ? "L\n" : "l\n", fp);
      break;
    case ART_CURVETO: break; /* unsupported */
    case ART_END: break; /* nop */
    }
  }
  if (node->type == GFX_LINE) {
    if (node->closed_path)
      fputs("CP ", fp);
    if (node->size != state->line_width) {
      state->line_width = node->size;
      svg_write_number(fp, state->line_width);
      fputs(" WS\n", fp);
    } else {
      fputs("S\n", fp);
    }
   } else {
    fputs("F\n", fp);
   }
}

static void eps_write_text(eps_state *state, gfx_node_t *node)
{
  FILE *fp = state->fp;
  const unsigned char *p;
  const char *ps_font = afm_get_font_postscript_name(node->filename);
  char align = 'L';
  double x = node->x;
  double y = state->page_height - node->y, ydelta = 0;
  int lineLen = 0;
  eps_set_color(state, node->color);
  if (strcmp(ps_font, state->font) || node->size != state->font_size) {
    state->font = ps_font;
    state->font_size = node->size;
    svg_write_number(fp, state->font_size);
    fprintf(fp, " SetFont-%s\n", state->font);
  }
  fputs("(", fp);
  lineLen = 20;
  for (p = (const unsigned char*)node->text; *p; p++) {
    if (lineLen > 70) {
      fputs("\\\n", fp); /* backslash and \n */
      lineLen = 0;
    }
    switch (*p) {
      case '(':
      case ')':
      case '\\':
      case '\n':
      case '\r':
      case '\t':
        fputc('\\', fp);
        lineLen++;
        /* fall-through */
      default:
        if (*p >= 126)
          fprintf(fp, "\\%03o", *p);
        else
          fputc(*p, fp);
        lineLen++;
    }
  }
  fputs(") ", fp);
  switch(node->valign){
  case GFX_V_TOP:    ydelta = -node->size; break;
  case GFX_V_CENTER: ydelta = -node->size / 3.0; break;          
  case GFX_V_BOTTOM: break;          
  case GFX_V_NULL: break;          
  }
  if (node->angle == 0)
    y += ydelta;
  switch (node->halign) {
  case GFX_H_RIGHT:  align = 'R'; break;
  case GFX_H_CENTER: align = 'C'; break;
  case GFX_H_LEFT: align= 'L'; break;
  case GFX_H_NULL: align= 'L'; break;
  }
  if (node->angle != 0) {
    fputs("\n", fp);
    fputs("  gsave ", fp);
    svg_write_number(fp, x);
    fputc(' ', fp);
    svg_write_number(fp, y);
    fputs(" translate ", fp);
    svg_write_number(fp, -node->angle);
    fputs(" rotate 0 ", fp);
    svg_write_number(fp, ydelta);
    fputs(" moveto ", fp);
    fprintf(fp, "Ta%c", align);
    fputs(" show grestore\n", fp);
  } else {
    svg_write_number(fp, x);
    fputc(' ', fp);
    svg_write_number(fp, y);
    fputs(" T", fp);
    fputc(align, fp);
    fputc('\n', fp);
  }
}

static int eps_write_content(eps_state *state)
{
  gfx_node_t *node;
  fputs("%\n", state->fp);
  for (node = state->canvas->firstnode; node; node = node->next) {
    switch (node->type) {
    case GFX_LINE:
    case GFX_AREA:
      eps_write_linearea(state, node);
      break;
    case GFX_TEXT:
      eps_write_text(state, node);
      break;
    }
  }
  return 0;
}

int       gfx_render_eps (gfx_canvas_t *canvas,
                 art_u32 width, art_u32 height,
                 gfx_color_t background, FILE *fp){
  struct eps_state state;
  state.fp = fp;
  state.canvas = canvas;
  state.page_width = width;
  state.page_height = height;
  state.font = "no-default-font";
  state.font_size = -1;
  state.color = 0; /* black */
  state.font_list = NULL;
  state.linecap = -1;
  state.linejoin = -1;
  state.has_dash = 0;
  if (eps_prologue(&state) == -1)
    return -1;
  eps_set_color(&state, background);
  fprintf(fp, "0 0 M 0 %d L %d %d L %d 0 L fill\n",
      height, width, height, width);
  if (eps_write_content(&state) == -1)
    return 0;
  fputs("showpage\n", fp);
  fputs("%%EOF\n", fp);
  while (state.font_list) {
    eps_font *next = state.font_list->next;
    free(state.font_list);
    state.font_list = next;
  }
  return 0;
}

/* ------- PDF -------
   PDF references page:
   http://partners.adobe.com/asn/developer/technotes/acrobatpdf.html
*/

typedef struct pdf_buffer
{
  int id, is_obj, is_dict, is_stream, pdf_file_pos;
  char *data;
  int alloc_size, current_size;
  struct pdf_buffer *previous_buffer, *next_buffer;
  struct pdf_state *state;
} pdf_buffer;

typedef struct pdf_font
{
  const char *ps_font;
  pdf_buffer obj;
  struct pdf_font *next;
} pdf_font;

typedef struct pdf_state
{
  FILE *fp;
  gfx_canvas_t *canvas;
  art_u32 page_width, page_height;
  pdf_font *font_list;
  pdf_buffer *first_buffer, *last_buffer;
  int pdf_file_pos;
  int has_failed;
  /*--*/
  gfx_color_t stroke_color, fill_color;
  int font_id;
  double font_size;
  double line_width;
  svg_dash dash;
  int linecap, linejoin;
  int last_obj_id;
  /*--*/
  pdf_buffer pdf_header;
  pdf_buffer catalog_obj, pages_obj, page1_obj;
  pdf_buffer fontsdict_obj;
  pdf_buffer graph_stream;
} pdf_state;

static void pdf_init_buffer(pdf_state *state, pdf_buffer *buf)
{
  int initial_size = 32;
  buf->state = state;
  buf->id = -42;
  buf->alloc_size = 0;
  buf->current_size = 0;
  buf->data = (char*)malloc(initial_size);
  buf->is_obj = 0;
  buf->previous_buffer = NULL;
  buf->next_buffer = NULL;
  if (buf->data == NULL) {
    rrd_set_error("malloc for pdf_buffer data");
    state->has_failed = 1;
    return;
  }
  buf->alloc_size = initial_size;
  if (state->last_buffer)
    state->last_buffer->next_buffer = buf;
  if (state->first_buffer == NULL)
    state->first_buffer = buf;
  buf->previous_buffer = state->last_buffer;
  state->last_buffer = buf;
}

static void pdf_put(pdf_buffer *buf, const char *text, int len)
{
  if (len <= 0)
    return;
  if (buf->alloc_size < buf->current_size + len) {
    int new_size = buf->alloc_size;
    char *new_buf;
    while (new_size < buf->current_size + len)
      new_size *= 4;
    new_buf = (char*)malloc(new_size);
    if (new_buf == NULL) {
      rrd_set_error("re-malloc for pdf_buffer data");
      buf->state->has_failed = 1;
      return;
    }
    memcpy(new_buf, buf->data, buf->current_size);
    free(buf->data);
    buf->data = new_buf;
    buf->alloc_size = new_size;
  }
  memcpy(buf->data + buf->current_size, text, len);
  buf->current_size += len;
}

static void pdf_puts(pdf_buffer *buf, const char *text)
{
  pdf_put(buf, text, strlen(text));
}

static void pdf_indent(pdf_buffer *buf)
{
  pdf_puts(buf, "\t");
}

static void pdf_putsi(pdf_buffer *buf, const char *text)
{
  pdf_indent(buf);
  pdf_puts(buf, text);
}

static void pdf_putint(pdf_buffer *buf, int i)
{
  char tmp[20];
  sprintf(tmp, "%d", i);
  pdf_puts(buf, tmp);
}

static void pdf_putnumber(pdf_buffer *buf, double d)
{
  char tmp[50];
  svg_format_number(tmp, sizeof(tmp), d);
  pdf_puts(buf, tmp);
}

static void pdf_init_object(pdf_state *state, pdf_buffer *buf)
{
  pdf_init_buffer(state, buf);
  buf->id = ++state->last_obj_id;
  buf->is_obj = 1;
  buf->is_stream = 0;
}

static void pdf_init_dict(pdf_state *state, pdf_buffer *buf)
{
  pdf_init_object(state, buf);
  buf->is_dict = 1;
}

static void pdf_set_color(pdf_buffer *buf, gfx_color_t color,
	gfx_color_t *current_color, const char *op)
{
   /* gfx_color_t is RRGGBBAA */
  if (*current_color == color)
    return;
  pdf_putnumber(buf, ((color >> 24) & 255) / 255.0);
  pdf_puts(buf, " ");
  pdf_putnumber(buf, ((color >> 16) & 255) / 255.0);
  pdf_puts(buf, " ");
  pdf_putnumber(buf, ((color >>  8) & 255) / 255.0);
  pdf_puts(buf, " ");
  pdf_puts(buf, op);
  pdf_puts(buf, "\n");
  *current_color = color;
}

static void pdf_set_stroke_color(pdf_buffer *buf, gfx_color_t color)
{
    pdf_set_color(buf, color, &buf->state->stroke_color, "RG");
}

static void pdf_set_fill_color(pdf_buffer *buf, gfx_color_t color)
{
    pdf_set_color(buf, color, &buf->state->fill_color, "rg");
}

static pdf_font *pdf_find_font(pdf_state *state, gfx_node_t *node)
{
  const char *ps_font = afm_get_font_postscript_name(node->filename);
  pdf_font *ef;
  for (ef = state->font_list; ef; ef = ef->next) {
    if (!strcmp(ps_font, ef->ps_font))
      return ef;
  }
  return NULL;
}

static void pdf_add_font(pdf_state *state, gfx_node_t *node)
{
  pdf_font *ef = pdf_find_font(state, node);
  if (ef)
    return;
  ef = malloc(sizeof(pdf_font));
  if (ef == NULL) {
    rrd_set_error("malloc for pdf_font");
    state->has_failed = 1;
    return;
  }
  pdf_init_dict(state, &ef->obj);
  ef->next = state->font_list;
  ef->ps_font = afm_get_font_postscript_name(node->filename);
  state->font_list = ef;
  /* fonts dict */
  pdf_putsi(&state->fontsdict_obj, "/F");
  pdf_putint(&state->fontsdict_obj, ef->obj.id);
  pdf_puts(&state->fontsdict_obj, " ");
  pdf_putint(&state->fontsdict_obj, ef->obj.id);
  pdf_puts(&state->fontsdict_obj, " 0 R\n");
  /* fonts def */
  pdf_putsi(&ef->obj, "/Type /Font\n");
  pdf_putsi(&ef->obj, "/Subtype /Type1\n");
  pdf_putsi(&ef->obj, "/Name /F");
  pdf_putint(&ef->obj, ef->obj.id);
  pdf_puts(&ef->obj, "\n");
  pdf_putsi(&ef->obj, "/BaseFont /");
  pdf_puts(&ef->obj, ef->ps_font);
  pdf_puts(&ef->obj, "\n");
  pdf_putsi(&ef->obj, "/Encoding /WinAnsiEncoding\n");
  /*  'Cp1252' (this is latin 1 extended with 27 characters;
      the encoding is also known as 'winansi')
      http://www.lowagie.com/iText/tutorial/ch09.html */
}

static void pdf_create_fonts(pdf_state *state)
{
  gfx_node_t *node;
  for (node = state->canvas->firstnode; node; node = node->next) {
    if (node->type == GFX_TEXT)
      pdf_add_font(state, node);
  }
}

static void pdf_write_linearea(pdf_state *state, gfx_node_t *node)
{
  int i;
  pdf_buffer *s = &state->graph_stream;
  if (node->type == GFX_LINE) {
    svg_dash dash_info;
    svg_get_dash(node, &dash_info);
    if (!svg_dash_equal(&dash_info, &state->dash)) {
      state->dash = dash_info;
      if (dash_info.dash_enable) {
	pdf_puts(s, "[");
	pdf_putnumber(s, dash_info.adjusted_on);
	pdf_puts(s, " ");
	pdf_putnumber(s, dash_info.adjusted_off);
	pdf_puts(s, "] ");
	pdf_putnumber(s, dash_info.dash_offset);
	pdf_puts(s, " d\n");
      } else {
	pdf_puts(s, "[] 0 d\n");
      }
    }
    pdf_set_stroke_color(s, node->color);
    if (state->linecap != 1) {
      pdf_puts(s, "1 j\n");
      state->linecap = 1;
    }
    if (state->linejoin != 1) {
      pdf_puts(s, "1 J\n");
      state->linejoin = 1;
    }
    if (node->size != state->line_width) {
      state->line_width = node->size;
      pdf_putnumber(s, state->line_width);
      pdf_puts(s, " w\n");
    }
  } else {
    pdf_set_fill_color(s, node->color);
  }
  for (i = 0; i < node->points; i++) {
    ArtVpath *vec = node->path + i;
    double x = vec->x;
    double y = state->page_height - vec->y;
    if (node->type == GFX_AREA) {
      x += LINEOFFSET; /* adjust for libart handling of areas */
      y -= LINEOFFSET;
    }
    switch (vec->code) {
    case ART_MOVETO_OPEN: /* fall-through */
    case ART_MOVETO:
      pdf_putnumber(s, x);
      pdf_puts(s, " ");
      pdf_putnumber(s, y);
      pdf_puts(s, " m\n");
      break;
    case ART_LINETO:
      pdf_putnumber(s, x);
      pdf_puts(s, " ");
      pdf_putnumber(s, y);
      pdf_puts(s, " l\n");
      break;
    case ART_CURVETO: break; /* unsupported */
    case ART_END: break; /* nop */
    }
  }
  if (node->type == GFX_LINE) {
    pdf_puts(s, node->closed_path ? "s\n" : "S\n");
   } else {
    pdf_puts(s, "f\n");
   }
}

static void pdf_write_text(pdf_state *state, gfx_node_t *node, 
    int last_was_text, int next_is_text)
{
  char tmp[30];
  pdf_buffer *s = &state->graph_stream;
  const unsigned char *p;
  pdf_font *font = pdf_find_font(state, node);
  double x = node->x;
  double y = state->page_height - node->y;
  double dx = 0, dy = 0;
  double cos_a = 0, sin_a = 0;
  if (font == NULL) {
    rrd_set_error("font disappeared");
    state->has_failed = 1;
    return;
  }
  switch(node->valign){
  case GFX_V_TOP:    dy = -node->size; break;
  case GFX_V_CENTER: dy = -node->size / 3.0; break;          
  case GFX_V_BOTTOM: break;          
  case GFX_V_NULL: break;          
  }
  switch (node->halign) {
  case GFX_H_RIGHT:  dx = -afm_get_text_width(0, font->ps_font, 
			 node->size, node->tabwidth, node->text);
		     break;
  case GFX_H_CENTER: dx = -afm_get_text_width(0, font->ps_font, 
			 node->size, node->tabwidth, node->text) / 2;
		     break;
  case GFX_H_LEFT: break;
  case GFX_H_NULL: break;
  }
  pdf_set_fill_color(s, node->color);
  if (node->angle != 0) {
    double a = 2 * M_PI * -node->angle / 360.0;
    double new_x, new_y;
    cos_a = cos(a);
    sin_a = sin(a);
    new_x = cos_a * dx - sin_a * dy + x;
    new_y = sin_a * dx + cos_a * dy + y;
    x = new_x;
    y = new_y;
  } else {
    x += dx;
    y += dy;
  }
  if (!last_was_text)
    pdf_puts(s, "BT\n");
  if (state->font_id != font->obj.id || node->size != state->font_size) {
    state->font_id = font->obj.id;
    state->font_size = node->size;
    pdf_puts(s, "/F");
    pdf_putint(s, font->obj.id);
    pdf_puts(s, " ");
    pdf_putnumber(s, node->size);
    pdf_puts(s, " Tf\n");
  }
  if (node->angle == 0) {
    pdf_puts(s, "1 0 0 1 ");
  } else {
    pdf_putnumber(s, cos_a);
    pdf_puts(s, " ");
    pdf_putnumber(s, sin_a);
    pdf_puts(s, " ");
    pdf_putnumber(s, -sin_a);
    pdf_puts(s, " ");
    pdf_putnumber(s, cos_a);
    pdf_puts(s, " ");
  }
  pdf_putnumber(s, x);
  pdf_puts(s, " ");
  pdf_putnumber(s, y);
  pdf_puts(s, " Tm\n");
  pdf_puts(s, "(");
  for (p = (const unsigned char*)node->text; *p; p++) {
    switch (*p) {
      case '(':
      case ')':
      case '\\':
      case '\n':
      case '\r':
      case '\t':
        pdf_puts(s, "\\");
        /* fall-through */
      default:
        if (*p >= 126) {
          snprintf(tmp, sizeof(tmp), "\\%03o", *p);
	  pdf_puts(s, tmp);
	} else {
          pdf_put(s, (const char*)p, 1);
	}
    }
  }
  pdf_puts(s, ") Tj\n");
  if (!next_is_text)
    pdf_puts(s, "ET\n");
}
 
static void pdf_write_content(pdf_state *state)
{
  gfx_node_t *node;
  int last_was_text = 0, next_is_text;
  for (node = state->canvas->firstnode; node; node = node->next) {
    switch (node->type) {
    case GFX_LINE:
    case GFX_AREA:
      pdf_write_linearea(state, node);
      break;
    case GFX_TEXT:
      next_is_text = node->next && node->next->type == GFX_TEXT;
      pdf_write_text(state, node, last_was_text, next_is_text);
      break;
    }
    last_was_text = node->type == GFX_TEXT;
  }
}

static void pdf_init_document(pdf_state *state)
{
  pdf_init_buffer(state, &state->pdf_header);
  pdf_init_dict(state, &state->catalog_obj);
  pdf_init_dict(state, &state->pages_obj);
  pdf_init_dict(state, &state->page1_obj);
  pdf_init_dict(state, &state->fontsdict_obj);
  pdf_create_fonts(state);
  if (state->has_failed)
    return;
  /* make stream last object in file */
  pdf_init_object(state, &state->graph_stream);
  state->graph_stream.is_stream = 1;
}

static void pdf_setup_document(pdf_state *state)
{
  /* all objects created by now, so init code can reference them */
  /* HEADER */
  pdf_puts(&state->pdf_header, "%PDF-1.3\n");
  /* following 8 bit comment is recommended by Adobe for
     indicating binary file to file transfer applications */
  pdf_puts(&state->pdf_header, "%\xE2\xE3\xCF\xD3\n");
  /* CATALOG */
  pdf_putsi(&state->catalog_obj, "/Type /Catalog\n");
  pdf_putsi(&state->catalog_obj, "/Pages ");
  pdf_putint(&state->catalog_obj, state->pages_obj.id);
  pdf_puts(&state->catalog_obj, " 0 R\n");
  /* PAGES */
  pdf_putsi(&state->pages_obj, "/Type /Pages\n");
  pdf_putsi(&state->pages_obj, "/Kids [");
  pdf_putint(&state->pages_obj, state->page1_obj.id);
  pdf_puts(&state->pages_obj, " 0 R]\n");
  pdf_putsi(&state->pages_obj, "/Count 1\n");
  /* PAGE 1 */
  pdf_putsi(&state->page1_obj, "/Type /Page\n");
  pdf_putsi(&state->page1_obj, "/Parent ");
  pdf_putint(&state->page1_obj, state->pages_obj.id);
  pdf_puts(&state->page1_obj, " 0 R\n");
  pdf_putsi(&state->page1_obj, "/MediaBox [0 0 ");
  pdf_putint(&state->page1_obj, state->page_width);
  pdf_puts(&state->page1_obj, " ");
  pdf_putint(&state->page1_obj, state->page_height);
  pdf_puts(&state->page1_obj, "]\n");
  pdf_putsi(&state->page1_obj, "/Contents ");
  pdf_putint(&state->page1_obj, state->graph_stream.id);
  pdf_puts(&state->page1_obj, " 0 R\n");
  pdf_putsi(&state->page1_obj, "/Resources << /Font ");
  pdf_putint(&state->page1_obj, state->fontsdict_obj.id);
  pdf_puts(&state->page1_obj, " 0 R >>\n");
}

static void pdf_write_string_to_file(pdf_state *state, const char *text)
{
    fputs(text, state->fp);
    state->pdf_file_pos += strlen(text);
}

static void pdf_write_buf_to_file(pdf_state *state, pdf_buffer *buf)
{
  char tmp[40];
  buf->pdf_file_pos = state->pdf_file_pos;
  if (buf->is_obj) {
    snprintf(tmp, sizeof(tmp), "%d 0 obj\n", buf->id);
    pdf_write_string_to_file(state, tmp);
  }
  if (buf->is_dict)
    pdf_write_string_to_file(state, "<<\n");
  if (buf->is_stream) {
    snprintf(tmp, sizeof(tmp), "<< /Length %d >>\n", buf->current_size);
    pdf_write_string_to_file(state, tmp);
    pdf_write_string_to_file(state, "stream\n");
  }
  fwrite(buf->data, 1, buf->current_size, state->fp);
  state->pdf_file_pos += buf->current_size;
  if (buf->is_stream)
    pdf_write_string_to_file(state, "endstream\n");
  if (buf->is_dict)
    pdf_write_string_to_file(state, ">>\n");
  if (buf->is_obj)
    pdf_write_string_to_file(state, "endobj\n");
}

static void pdf_write_to_file(pdf_state *state)
{
  pdf_buffer *buf = state->first_buffer;
  int xref_pos;
  state->pdf_file_pos = 0;
  pdf_write_buf_to_file(state, &state->pdf_header);
  while (buf) {
    if (buf->is_obj)
      pdf_write_buf_to_file(state, buf);
    buf = buf->next_buffer;
  }
  xref_pos = state->pdf_file_pos;
  fprintf(state->fp, "xref\n");
  fprintf(state->fp, "%d %d\n", 0, state->last_obj_id + 1);
  /* TOC lines must be exactly 20 bytes including \n */
  fprintf(state->fp, "%010d %05d f\x20\n", 0, 65535);
  for (buf = state->first_buffer; buf; buf = buf->next_buffer) {
    if (buf->is_obj)
      fprintf(state->fp, "%010d %05d n\x20\n", buf->pdf_file_pos, 0);
  }
  fprintf(state->fp, "trailer\n");
  fprintf(state->fp, "<<\n");
  fprintf(state->fp, "\t/Size %d\n", state->last_obj_id + 1);
  fprintf(state->fp, "\t/Root %d 0 R\n", state->catalog_obj.id);
  fprintf(state->fp, ">>\n");
  fprintf(state->fp, "startxref\n");
  fprintf(state->fp, "%d\n", xref_pos);
  fputs("%%EOF\n", state->fp);
}

static void pdf_free_resources(pdf_state *state)
{
  pdf_buffer *buf = state->first_buffer;
  while (buf) {
    free(buf->data);
    buf->data = NULL;
    buf->alloc_size = buf->current_size = 0;
    buf = buf->next_buffer;
  }
  while (state->font_list) {
    pdf_font *next = state->font_list->next;
    free(state->font_list);
    state->font_list = next;
  }
}

int       gfx_render_pdf (gfx_canvas_t *canvas,
                 art_u32 width, art_u32 height,
                 gfx_color_t background, FILE *fp){
  struct pdf_state state;
  memset(&state, 0, sizeof(pdf_state));
  state.fp = fp;
  state.canvas = canvas;
  state.page_width = width;
  state.page_height = height;
  state.font_id = -1;
  state.font_size = -1;
  state.font_list = NULL;
  state.linecap = -1;
  state.linejoin = -1;
  pdf_init_document(&state);
  /*
  pdf_set_color(&state, background);
  fprintf(fp, "0 0 M 0 %d L %d %d L %d 0 L fill\n",
      height, width, height, width);
  */
  if (!state.has_failed)
    pdf_write_content(&state);
  if (!state.has_failed)
    pdf_setup_document(&state);
  if (!state.has_failed)
    pdf_write_to_file(&state);
  pdf_free_resources(&state);
  return state.has_failed ? -1 : 0;
}

