#include "rrd_afm.h"
#include <stdio.h>

FILE *fp;

static void make_tests(void);
static void print(const char *s);
double y = 0;
//static const char *font = "Times-Roman";
static const char *font = "Times Bold Italic";
//static const char *font = "Courier";
//static const char *font = "Courier Bold Oblique";

void make_tests()
{
#ifdef __APPLE__
#define charset_legend "Macintosh charset"
#define AE "\xAE"
#define ae "\xBE"
#define oe "\xBF"
#define aa "\x8C"
#define NBSP "\x00CA"
#else
#define charset_legend "IsoLatin1 charset"
#define AE "\xC6"
#define ae "\xE6"
#define oe "\xF8"
#define aa "\xA5"
#define NBSP "\x00A0"
#endif
  print(AE); /* very wide char */
  print(AE AE AE AE AE AE AE AE AE AE AE AE AE AE AE);
  print(charset_legend);
  print("S,");
  print("sfil");
  print("Hello, world");
  print("AVAVAVAVAVAVAVAVAVAVAVAVAVAVAVAVAVAV");
  print("AAAAAAAAAAAAAAAAAAVVVVVVVVVVVVVVVVVV");
  print("fiffififfififfififfififfififfififfi");
  print("fi");
  print("fil");
  print("fifififififififififififififififififififififififififi");
  print(AE "bleskiver med gl"oe"gg. " NBSP NBSP NBSP NBSP NBSP NBSP NBSP
      AE" Fywerhus: 'A "ae" u "aa" "ae" "oe" i "ae" fywer'.");
  print("Ingef"ae"rp"ae"rer med karamelsauce. R"oe"dgr"oe"d med fl"oe"de.");
  print("(Optional.) Ligature sequence where successor and ligature are both names.  The current character may join ...");
}

static void vline(double x, double y1, double y2)
{
  fprintf(fp, "<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\""
      " stroke-width=\"1\" stroke=\"#000\"/>\n",
      x, y1, x, y2);
}

static void print(const char *s)
{
  double size = 12;
  double x = 10;
  double width = afm_get_text_width(0, font, size, 4, s);
  unsigned char *up = (unsigned char*)s;
  fprintf(stderr, "Width = %f for '%s'\n", width, s);
  y += 10;
  vline(x, y, y + 5);
  fprintf(fp, "<text x=\"%.2f\" y=\"%.2f\" font-size=\"%.2f\">", x, y, size);
  while (*up) {
    unsigned char ch = afm_host2unicode(*up);
    if (ch < 127)
      putc(ch, fp);
    else
      fprintf(fp, "&#%d;", ch);
    up++;
  }
  fputs("</text>\n", fp);
  vline(x + width, y, y + 5);
  y += 1.1 * size;
}

static void header()
{
   fprintf(fp,
 "<?xml version=\"1.0\" standalone=\"no\"?>\n"
 "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\"\n"
 "   \"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\">\n"
 "<svg width=\"650\" height=\"400\" preserveAspectRatio=\"xMidYMid\"\n"
 "    font-family=\"%s\">\n", font);
 }


static void footer()
{
   fputs("</svg>\n", fp);
}

int main()
{
  fp = fopen("test.svg", "w");
  if (fp == NULL) {
    fprintf(stderr, "Can't create output.\n");
    exit(1);
  }
  header();
  make_tests();
  footer();
  fclose(fp);
  return 0;
}

