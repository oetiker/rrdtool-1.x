/****************************************************************************
 * RRDtool 1.2.12  Copyright by Tobi Oetiker, 1997-2005
 ****************************************************************************
 * rrd_afm.h  Parsing afm tables to find width of strings.
 ****************************************************************************/

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__CYGWIN32__) && !defined(HAVE_CONFIG_H)
#include "../confignt/config.h"
#else
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#endif

#include "rrd_afm.h"
#include "rrd_afm_data.h"

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "unused.h"

#if 0
# define DEBUG 1
# define DLOG(x) fprintf x
#else
# define DEBUG 0
# define DLOG(x) 
#endif

/* Adobe SVG View and Batik 1.1.1 can't handle ligatures.
   So disable it as we just waste speed.
   Besides, it doesn't matter much in normal text.
*/
#define ENABLE_LIGATURES 0

static const afm_fontinfo *afm_last_used_font = NULL;
static const char *last_unknown_font = NULL;

#define is_font(p, name) \
  (!strcmp(p->postscript_name, name) || !strcmp(p->fullname, name))

static const afm_fontinfo *afm_searchfont(const char *name)
{
  int i;
  const afm_fontinfo *p = afm_last_used_font;
  if (p && is_font(p, name))
    return p;
  p = afm_fontinfolist;
  for (i = 0; i < afm_fontinfo_count; i++, p++) {
    if (is_font(p, name)) {
      afm_last_used_font = p;
      return p;
    }
  }
  return NULL;
}


/* returns always a font, never NULL.
   The rest of the code depends on the result never being NULL.
   See rrd_afm.h */
static const afm_fontinfo *afm_findfont(const char *name)
{
  const afm_fontinfo *p = afm_searchfont(name);
  if (p)
    return p;
  if (!last_unknown_font || strcmp(name, last_unknown_font)) {
	  fprintf(stderr, "Can't find font '%s'\n", name);
	  last_unknown_font = name;
  }
  p = afm_searchfont(RRD_AFM_DEFAULT_FONT);
  if (p)
    return p;
  return afm_fontinfolist; /* anything, just anything. */
}

const char *afm_get_font_postscript_name(const char* font)
{
  const afm_fontinfo *p = afm_findfont(font);
  return p->postscript_name;
}

const char *afm_get_font_name(const char* font)
{
  const afm_fontinfo *p = afm_findfont(font);
  return p->fullname;
}

double afm_get_ascender(const char* font, double size)
{
  const afm_fontinfo *p = afm_findfont(font);
  return size * p->ascender / 1000.0;
}

double afm_get_descender(const char* font, double size)
{
  const afm_fontinfo *p = afm_findfont(font);
  return size * p->descender / 1000.0;
}

static int afm_find_char_index(const afm_fontinfo *fontinfo,
    afm_cunicode ch1)
{
  int idx = ch1 - 32;
  afm_cuint16 *indexP;
  int numIndexChars, i;
  if (idx <= 0)
    return 0;
  if (idx <= 126 - 32)
    return idx;
  indexP = fontinfo->highchars_index;
  if (indexP == 0)
    return 0;
  numIndexChars = fontinfo->highchars_count;
  DLOG((stderr, " find highbit, num = %d\n", numIndexChars));
  if (ch1 >= 161 && ch1 <= 255) {
    idx = ch1 - 161;
    DLOG((stderr, "  161, idx = %d -> %d\n", idx, indexP[idx]));
    if (idx < numIndexChars && indexP[idx] == ch1) {
      idx += 127 - 32;
      DLOG((stderr, "  161-guessed ok to %d\n", idx));
      return idx;
    }
  }
  for (i = 0; i < numIndexChars; i++) {
    DLOG((stderr, "    compares to %d -> %d\n", indexP[i], i));
    if (indexP[i] == ch1)
      return i + 127 - 32;
  }
  DLOG((stderr, "Did not find %d in highchars_index ??\n", ch1));
  return 0;
}

#if ENABLE_LIGATURES
static afm_cunicode afm_find_combined_ligature(const afm_fontinfo *fontinfo,
    afm_cunicode ch1, afm_cunicode ch2)
{
  afm_cunicode *p = fontinfo->ligatures;
  int num = fontinfo->ligatures_count;
  int i;
  if (!num)
    return 0;
  DLOG((stderr, " find-lig, num = %d\n", num));
  for (i = 0; i < num; i++, p += 3) {
    DLOG((stderr, "    lig: %d + %d -> %d (%c %c %c)\n",
        p[0], p[1], p[2], p[0], p[1], p[2]));
    if (ch1 == *p && ch2 == p[1]) {
      DLOG((stderr, "   matches.\n"));
      return p[2];
    }
  }
  return 0;
}
#endif

#define READ_ESCAPED(p, val) \
  if ((val = *p++) == 0) { \
    val = 254 + *p++; \
  } else if (!--val) { \
    val = *p++ << 8; \
    val |= *p++; \
  }


static long afm_find_kern(const afm_fontinfo *fontinfo,
    int kern_idx, afm_cunicode ch2)
{
  afm_cuint8 *p8 = fontinfo->kerning_data + kern_idx;
  int num;
  READ_ESCAPED(p8, num);
  DLOG((stderr, " find kern, num pairs = %d\n", num));
  while (num > 0) {
    afm_unicode ch;
    READ_ESCAPED(p8, ch);
    DLOG((stderr, "     pair-char = %d\n", ch));
    if (ch == ch2) {
      DLOG((stderr, " got kern = %d\n", *(afm_csint8*)p8));
      return *(afm_csint8*)p8;
    }
    p8++;
    num--;
  }
  return 0;
}

/* measure width of a text string */
double afm_get_text_width( double start, const char* font, double size,
          double tabwidth, const char* text)
{
#ifdef HAVE_MBSTOWCS     
    size_t clen = strlen(text) + 1;
    wchar_t *cstr = malloc(sizeof(wchar_t) * clen); /* yes we are allocating probably too much here, I know */
    int text_count = mbstowcs(cstr, text, clen);
    double w;
    if (text_count == -1)
	    text_count = mbstowcs(cstr, "Enc-Err", 6);
#ifdef __APPLE__
	while (text_count > 0) {
		text_count--;
		cstr[text_count] = afm_fix_osx_charset(cstr[text_count]); /* unsafe macro */
	}
#endif
    w = afm_get_text_width_wide(start, font, size, tabwidth, cstr);
    free(cstr);
    return w;
#else
    return afm_get_text_width_wide(start, font, size, tabwidth, text);
#endif
}

double afm_get_text_width_wide( double UNUSED(start), const char* font, double size,
          double UNUSED(tabwidth), const afm_char* text)
{
  const afm_fontinfo *fontinfo = afm_findfont(font);
  long width = 0;
  double widthf;
  const afm_char *up = text;
  DLOG((stderr, "================= %s\n", text));
  if (fontinfo == NULL) {
      while (*up)
	  up++;
    return size * (up - text);
  }
  while (1) {
    afm_unicode ch1, ch2;
    int idx1, kern_idx;
    if ((ch1 = *up) == 0)
        break;
    ch2 = *++up;
    DLOG((stderr, "------------- Loop: %d + %d (%c%c)   at %d\n",
          ch1, ch2, ch1, ch2 ? ch2 : ' ',
	  (up - (const unsigned char*)text) - 1));
    idx1 = afm_find_char_index(fontinfo, ch1);
    DLOG((stderr, "  idx1 = %d\n", idx1));
#if ENABLE_LIGATURES
    if (ch2) {
      int ch1_new = afm_find_combined_ligature(fontinfo, ch1, ch2);
      DLOG((stderr, "  lig-ch = %d\n", ch1_new));
      if (ch1_new) {
        ch1 = ch1_new;
        idx1 = afm_find_char_index(fontinfo, ch1);
        ch2 = *++up;
        DLOG((stderr, "  -> idx1 = %d, ch2 = %d (%c)\n", 
            idx1, ch2, ch2 ? ch2 : ' '));
      }
    }
#endif
    width += fontinfo->widths[idx1];
    DLOG((stderr, "Plain width of %d = %d\n", ch1, fontinfo->widths[idx1]));
    if (fontinfo->kerning_index && ch2) {
      kern_idx = fontinfo->kerning_index[idx1];
      DLOG((stderr, "    kern_idx = %d\n", kern_idx));
      if (kern_idx > 0)
        width += afm_find_kern(fontinfo, kern_idx, ch2);
    }
  }
  widthf = (width * 6 / 1000.0) * size;
  DLOG((stderr, "Returns %ld (%ld) -> %f\n", width, width * 6, widthf));
  return widthf;
}

#ifdef __APPLE__
const unsigned char afm_mac2iso[128] = {
  '\xC4', '\xC5', '\xC7', '\xC9', '\xD1', '\xD6', '\xDC', '\xE1', /* 80 */
  '\xE0', '\xE2', '\xE4', '\xE3', '\xE5', '\xE7', '\xE9', '\xE8', /* 88 */
  '\xEA', '\xEB', '\xED', '\xEC', '\xEE', '\xEF', '\xF1', '\xF3', /* 90 */
  '\xF2', '\xF4', '\xF6', '\xF5', '\xFA', '\xF9', '\xFB', '\xFC', /* 98 */
  '\xDD', '\xB0', '\xA2', '\xA3', '\xA7', ' ',    '\xB6', '\xDF', /* A0 */
  '\xAE', '\xA9', ' ',    '\xB4', '\xA8', ' ',    '\xC6', '\xD8', /* A8 */
  ' ',    '\xB1', '\xBE', ' ',    '\xA5', '\xB5', ' ',    ' ',    /* B0 */
  '\xBD', '\xBC', ' ',    '\xAA', '\xBA', ' ',    '\xE6', '\xF8', /* B8 */
  '\xBF', '\xA1', '\xAC', ' ',    ' ',    ' ',    ' ',    '\xAB', /* C0 */
  '\xBB', ' ',    '\xA0', '\xC0', '\xC3', '\xD5', ' ',    '\xA6', /* C8 */
  '\xAD', ' ',    '"',    '"',    '\'',   '\'',   '\xF7', '\xD7', /* D0 */
  '\xFF', ' ',    ' ',    '\xA4', '\xD0', '\xF0', '\xDE', '\xFE', /* D8 */
  '\xFD', '\xB7', ' ',    ' ',    ' ',    '\xC2', '\xCA', '\xC1', /* E0 */
  '\xCB', '\xC8', '\xCD', '\xCE', '\xCF', '\xCC', '\xD3', '\xD4', /* E8 */
  ' ',    '\xD2', '\xDA', '\xDB', '\xD9', ' ',    ' ',    ' ',    /* F0 */
  '\xAF', ' ',    ' ',    ' ',    '\xB8', ' ',    ' ',    ' ',    /* F8 */
};
#endif
