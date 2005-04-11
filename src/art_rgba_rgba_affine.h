#ifndef SP_ART_RGBA_RGBA_AFFINE_H
#define SP_ART_RGBA_RGBA_AFFINE_H

/*
 * Lauris Kaplinski <lauris@ariman.ee>
 *
 * A slightly modified version of art_rgb_rgba_affine to render into
 * rgba buffer
 */

/* Libart_LGPL - library of basic graphic primitives
 * Copyright (C) 1998 Raph Levien
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <math.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_filterlevel.h>
#include <libart_lgpl/art_alphagamma.h>

/* This module handles compositing of affine-transformed rgba images
   over rgb pixel buffers. */

/* Composite the source image over the destination image, applying the
   affine transform. */

/**
 * gnome_print_art_rgba_rgba_affine: Affine transform source RGBA image and composite.
 * @dst: Destination image RGBA buffer.
 * @x0: Left coordinate of destination rectangle.
 * @y0: Top coordinate of destination rectangle.
 * @x1: Right coordinate of destination rectangle.
 * @y1: Bottom coordinate of destination rectangle.
 * @dst_rowstride: Rowstride of @dst buffer.
 * @src: Source image RGBA buffer.
 * @src_width: Width of source image.
 * @src_height: Height of source image.
 * @src_rowstride: Rowstride of @src buffer.
 * @affine: Affine transform.
 * @level: Filter level.
 * @alphagamma: #ArtAlphaGamma for gamma-correcting the compositing.
 *
 * Affine transform the source image stored in @src, compositing over
 * the area of destination image @dst specified by the rectangle
 * (@x0, @y0) - (@x1, @y1). As usual in libart, the left and top edges
 * of this rectangle are included, and the right and bottom edges are
 * excluded.
 *
 * The @alphagamma parameter specifies that the alpha compositing be
 * done in a gamma-corrected color space. In the current
 * implementation, it is ignored.
 *
 * The @level parameter specifies the speed/quality tradeoff of the
 * image interpolation. Currently, only ART_FILTER_NEAREST is
 * implemented.
 **/
void
gnome_print_art_rgba_rgba_affine (art_u8 *dst,
		     int x0, int y0, int x1, int y1, int dst_rowstride,
		     const art_u8 *src,
		     int src_width, int src_height, int src_rowstride,
		     const double affine[6],
		     ArtFilterLevel level,
		     ArtAlphaGamma *alphagamma);

#endif
