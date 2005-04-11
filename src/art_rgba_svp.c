/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  art_rgba_svp.c: A slightly modified version of art_rgb_svp to render into rgba buffer
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors:
 *    Raph Levien <raph@acm.org>
 *    Lauris Kaplinski <lauris@ariman.ee>
 *
 *  Copyright (C) 1998 Raph Levien
 *
 */

#define SP_ART_RGBA_SVP_C

/* Render a sorted vector path into an RGBA buffer. */

#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_svp_render_aa.h>
#include <libart_lgpl/art_rgb.h>

#include "art_rgba_svp.h"

static void art_rgba_fill_run (art_u8 * linebuf, art_u8 r, art_u8 g, art_u8 b, int n);
static void art_rgba_run_alpha (art_u8 * linebuf, art_u8 r, art_u8 g, art_u8 b, int alpha, int n);

typedef struct _ArtRgbaSVPAlphaData ArtRgbaSVPAlphaData;

struct _ArtRgbaSVPAlphaData {
  int alphatab[256];
  art_u8 r, g, b, alpha;
  art_u8 *buf;
  int rowstride;
  int x0, x1;
};

static void
art_rgba_svp_alpha_callback (void *callback_data, int y,
			    int start, ArtSVPRenderAAStep *steps, int n_steps)
{
  ArtRgbaSVPAlphaData *data = callback_data;
  art_u8 *linebuf;
  int run_x0, run_x1;
  art_u32 running_sum = start;
  int x0, x1;
  int k;
  art_u8 r, g, b;
  int *alphatab;
  int alpha;

  linebuf = data->buf;
  x0 = data->x0;
  x1 = data->x1;

  r = data->r;
  g = data->g;
  b = data->b;
  alphatab = data->alphatab;

  if (n_steps > 0)
    {
      run_x1 = steps[0].x;
      if (run_x1 > x0)
	{
	  alpha = (running_sum >> 16) & 0xff;
	  if (alpha)
	    art_rgba_run_alpha (linebuf,
			       r, g, b, alphatab[alpha],
			       run_x1 - x0);
	}

      /* render the steps into tmpbuf */
      for (k = 0; k < n_steps - 1; k++)
	{
	  running_sum += steps[k].delta;
	  run_x0 = run_x1;
	  run_x1 = steps[k + 1].x;
	  if (run_x1 > run_x0)
	    {
	      alpha = (running_sum >> 16) & 0xff;
	      if (alpha)
		art_rgba_run_alpha (linebuf + (run_x0 - x0) * 4,
				   r, g, b, alphatab[alpha],
				   run_x1 - run_x0);
	    }
	}
      running_sum += steps[k].delta;
      if (x1 > run_x1)
	{
	  alpha = (running_sum >> 16) & 0xff;
	  if (alpha)
	    art_rgba_run_alpha (linebuf + (run_x1 - x0) * 4,
			       r, g, b, alphatab[alpha],
			       x1 - run_x1);
	}
    }
  else
    {
      alpha = (running_sum >> 16) & 0xff;
      if (alpha)
	art_rgba_run_alpha (linebuf,
			   r, g, b, alphatab[alpha],
			   x1 - x0);
    }

  data->buf += data->rowstride;
}

static void
art_rgba_svp_alpha_opaque_callback (void *callback_data, int y,
				   int start,
				   ArtSVPRenderAAStep *steps, int n_steps)
{
  ArtRgbaSVPAlphaData *data = callback_data;
  art_u8 *linebuf;
  int run_x0, run_x1;
  art_u32 running_sum = start;
  int x0, x1;
  int k;
  art_u8 r, g, b;
  int *alphatab;
  int alpha;

  linebuf = data->buf;
  x0 = data->x0;
  x1 = data->x1;

  r = data->r;
  g = data->g;
  b = data->b;
  alphatab = data->alphatab;

  if (n_steps > 0)
    {
      run_x1 = steps[0].x;
      if (run_x1 > x0)
	{
	  alpha = running_sum >> 16;
	  if (alpha)
	    {
	      if (alpha >= 255)
		art_rgba_fill_run (linebuf,
				  r, g, b,
				  run_x1 - x0);
	      else
		art_rgba_run_alpha (linebuf,
				   r, g, b, alphatab[alpha],
				   run_x1 - x0);
	    }
	}

      /* render the steps into tmpbuf */
      for (k = 0; k < n_steps - 1; k++)
	{
	  running_sum += steps[k].delta;
	  run_x0 = run_x1;
	  run_x1 = steps[k + 1].x;
	  if (run_x1 > run_x0)
	    {
	      alpha = running_sum >> 16;
	      if (alpha)
		{
		  if (alpha >= 255)
		    art_rgba_fill_run (linebuf + (run_x0 - x0) * 4,
				      r, g, b,
				      run_x1 - run_x0);
		  else
		    art_rgba_run_alpha (linebuf + (run_x0 - x0) * 4,
				       r, g, b, alphatab[alpha],
				       run_x1 - run_x0);
		}
	    }
	}
      running_sum += steps[k].delta;
      if (x1 > run_x1)
	{
	  alpha = running_sum >> 16;
	  if (alpha)
	    {
	      if (alpha >= 255)
		art_rgba_fill_run (linebuf + (run_x1 - x0) * 4,
				  r, g, b,
				  x1 - run_x1);
	      else
		art_rgba_run_alpha (linebuf + (run_x1 - x0) * 4,
				   r, g, b, alphatab[alpha],
				   x1 - run_x1);
	    }
	}
    }
  else
    {
      alpha = running_sum >> 16;
      if (alpha)
	{
	  if (alpha >= 255)
	    art_rgba_fill_run (linebuf,
			      r, g, b,
			      x1 - x0);
	  else
	    art_rgba_run_alpha (linebuf,
			       r, g, b, alphatab[alpha],
			       x1 - x0);
	}
    }

  data->buf += data->rowstride;
}

/**
 * gnome_print_art_rgba_svp_alpha: Alpha-composite sorted vector path over RGBA buffer.
 * @svp: The source sorted vector path.
 * @x0: Left coordinate of destination rectangle.
 * @y0: Top coordinate of destination rectangle.
 * @x1: Right coordinate of destination rectangle.
 * @y1: Bottom coordinate of destination rectangle.
 * @rgba: Color in 0xRRGGBBAA format.
 * @buf: Destination RGB buffer.
 * @rowstride: Rowstride of @buf buffer.
 * @alphagamma: #ArtAlphaGamma for gamma-correcting the compositing.
 *
 * Renders the shape specified with @svp over the @buf RGB buffer.
 * @x1 - @x0 specifies the width, and @y1 - @y0 specifies the height,
 * of the rectangle rendered. The new pixels are stored starting at
 * the first byte of @buf. Thus, the @x0 and @y0 parameters specify
 * an offset within @svp, and may be tweaked as a way of doing
 * integer-pixel translations without fiddling with @svp itself.
 *
 * The @rgba argument specifies the color for the rendering. Pixels of
 * entirely 0 winding number are left untouched. Pixels of entirely
 * 1 winding number have the color @rgba composited over them (ie,
 * are replaced by the red, green, blue components of @rgba if the alpha
 * component is 0xff). Pixels of intermediate coverage are interpolated
 * according to the rule in @alphagamma, or default to linear if
 * @alphagamma is NULL.
 **/
void
gnome_print_art_rgba_svp_alpha (const ArtSVP *svp,
				int x0, int y0, int x1, int y1,
				art_u32 rgba,
				art_u8 *buf, int rowstride,
				ArtAlphaGamma *alphagamma)
{
  ArtRgbaSVPAlphaData data;
  int r, g, b, alpha;
  int i;
  int a, da;

  r = rgba >> 24;
  g = (rgba >> 16) & 0xff;
  b = (rgba >> 8) & 0xff;
  alpha = rgba & 0xff;

  data.r = r;
  data.g = g;
  data.b = b;
  data.alpha = alpha;

  a = 0x8000;
  da = (alpha * 66051 + 0x80) >> 8; /* 66051 equals 2 ^ 32 / (255 * 255) */

  for (i = 0; i < 256; i++)
    {
      data.alphatab[i] = a >> 16;
      a += da;
    }

  data.buf = buf;
  data.rowstride = rowstride;
  data.x0 = x0;
  data.x1 = x1;
  if (alpha == 255)
    art_svp_render_aa (svp, x0, y0, x1, y1, art_rgba_svp_alpha_opaque_callback,
		       &data);
  else
    art_svp_render_aa (svp, x0, y0, x1, y1, art_rgba_svp_alpha_callback, &data);
}

static void
art_rgba_fill_run (art_u8 * buf, art_u8 r, art_u8 g, art_u8 b, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		* buf++ = r;
		* buf++ = g;
		* buf++ = b;
		* buf++ = 255;
	}
}

/* fixme: this */

static void
art_rgba_run_alpha (art_u8 * buf, art_u8 r, art_u8 g, art_u8 b, int alpha, int n)
{
	int i;
	int br, bg, bb, ba;
	int cr, cg, cb;

	for (i = 0; i < n; i++) {
		br = * (buf + 0);
		bg = * (buf + 1);
		bb = * (buf + 2);
		ba = * (buf + 3);

		cr = (br * ba + 0x80) >> 8;
		cg = (bg * ba + 0x80) >> 8;
		cb = (bb * ba + 0x80) >> 8;

		* buf++ = cr + (((r - cr) * alpha + 0x80) >> 8);
		* buf++ = cg + (((g - cg) * alpha + 0x80) >> 8);
		* buf++ = cb + (((b - cb) * alpha + 0x80) >> 8);
		* buf++ = ba + (((255 - ba) * alpha + 0x80) >> 8);
	}
}


