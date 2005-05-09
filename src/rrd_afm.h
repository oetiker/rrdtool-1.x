/****************************************************************************
 * RRDtool 1.2.4  Copyright by Tobi Oetiker, 1997-2005
 ****************************************************************************
 * rrd_afm.h  Parsing afm tables to find width of strings.
 ****************************************************************************/

#ifndef  RRD_AFM_H
#define RRD_AFM_H

/*
   If the font specified by the name parameter in the routes below
   is not found
   (because it is not compiled into rrd_afm_data.c by compile_afm.pl)
   the font specified by RRD_AFM_DEFAULT_FONT will be used.
   If it is not installed, it uses the first font compiled
   into rrd_afm_data.c
   So they will always use some font.
*/

#define RRD_AFM_DEFAULT_FONT "Courier"

/* measure width of a text string */
/* fontname can be full name or postscript name */
double afm_get_text_width ( double start, const char* font, double size,
			    double tabwidth, const char* text);

double afm_get_ascender(const char* font, double size);
double afm_get_descender(const char* font, double size);

/* get postscript name from fullname or postscript name */
const char *afm_get_font_postscript_name ( const char* font);
const char *afm_get_font_name(const char* font);

/* cc -E -dM /dev/null */
#ifdef __APPLE__
/* need charset conversion from macintosh to unicode. */
extern const unsigned char afm_mac2iso[128];
#define afm_host2unicode(c) \
	( (c) >= 128 ? afm_mac2iso[(c) - 128] : (c))
#else
/* UNSAFE macro */
#define afm_host2unicode(a_unsigned_char) ((unsigned int)(a_unsigned_char))
#endif

#endif
