/*****************************************************************************
 * RRDtool 1.2rc8  Copyright by Tobi Oetiker, 1997-2005
 *****************************************************************************
 * gdpng.c  add PNG output routine to gd library
 *****************************************************************************/

#include <png.h>
#include <gd.h>
#include <stdlib.h>

typedef struct _jmpbuf_wrapper {
  jmp_buf jmpbuf;
} jmpbuf_wrapper;

static jmpbuf_wrapper gdPngJmpbufStruct;

void gdImagePng(gdImagePtr im, FILE *out)
{
    int i;
    png_colorp palette;
    png_structp png_write_ptr = 
 	png_create_write_struct(PNG_LIBPNG_VER_STRING, 
	 			(png_voidp)NULL,
				/* we would need to point to error handlers
				   here to do it properly */
				(png_error_ptr)NULL, (png_error_ptr)NULL);
    png_infop info_ptr = png_create_info_struct(png_write_ptr);

    if (setjmp(gdPngJmpbufStruct.jmpbuf)) {
      png_destroy_write_struct(&png_write_ptr, &info_ptr);
      return;
    }

    palette = (png_colorp)png_malloc (png_write_ptr,
				      im->colorsTotal*sizeof(png_color));
    if (palette == NULL){
      png_destroy_write_struct(&png_write_ptr, &info_ptr);
      return;
    }
    
    
    png_init_io(png_write_ptr, out);
    png_set_write_status_fn(png_write_ptr, NULL);
    png_set_IHDR(png_write_ptr,info_ptr,
		 im->sx,im->sy,im->colorsTotal > 16 ? 8:4,
		 PNG_COLOR_TYPE_PALETTE,
		 im->interlace ? PNG_INTERLACE_ADAM7: PNG_INTERLACE_NONE ,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    for(i=0;i<im->colorsTotal;i++){
	palette[i].red = im->red[i];
	palette[i].green = im->green[i];
	palette[i].blue = im->blue[i];
    }
    png_set_PLTE(png_write_ptr, info_ptr, palette, im->colorsTotal);

    /* choose between speed (1) and space (9) optimisation */
    /* we want to be fast ... */
    png_set_compression_level(png_write_ptr,1);
    png_set_filter(png_write_ptr,PNG_FILTER_TYPE_BASE,PNG_NO_FILTERS);
    /* store file info */
    png_write_info(png_write_ptr, info_ptr);
    png_set_packing(png_write_ptr);
    png_write_image(png_write_ptr, im->pixels);
    png_write_end(png_write_ptr, info_ptr);
    png_free(png_write_ptr, palette);
    png_destroy_write_struct(&png_write_ptr, &info_ptr);
}




