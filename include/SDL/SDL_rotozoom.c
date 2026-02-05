/*  

    SDL_rotozoom.c

    Copyright (C) A. Schiffler, July 2001

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifdef WIN32
#include <windows.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "SDL_rotozoom.h"

/* ARM NEON SIMD support for Cortex-A7 (Miyoo Mini) */
#ifdef __ARM_NEON
#include <arm_neon.h>
#define NEON_ZOOM_AVAILABLE 1
/* Prefetch hint for improved cache performance */
#define PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#else
#define NEON_ZOOM_AVAILABLE 0
#define PREFETCH(addr) ((void)0)
#endif

/* Include assembly optimizations when available */
#ifdef USE_NEON_ASM
#include "../../src/common/utils/neon_asm.h"
#endif

#define MAX(a,b)    (((a) > (b)) ? (a) : (b))

#ifdef __ARM_NEON
/*
 * NEON-optimized bilinear interpolation for a single row of 4 pixels
 * 
 * This function processes 4 destination pixels at once using NEON SIMD.
 * Each pixel requires bilinear interpolation from 4 source pixels (2x2 grid).
 *
 * The interpolation formula for each channel:
 *   t1 = c00 * (1-ex) + c01 * ex  (top row interpolation)
 *   t2 = c10 * (1-ex) + c11 * ex  (bottom row interpolation)
 *   result = t1 * (1-ey) + t2 * ey (vertical interpolation)
 */
static inline void neon_bilinear_interp_4px(
    tColorRGBA *dp,
    const tColorRGBA *c00_base,
    const tColorRGBA *c10_base,
    const int *csax,
    int ey)
{
    /* Load interpolation weights - x weights from csax, y weight from ey */
    int ex0 = csax[0] & 0xffff;
    int ex1 = csax[1] & 0xffff;
    int ex2 = csax[2] & 0xffff;
    int ex3 = csax[3] & 0xffff;
    
    /* Calculate source pixel offsets */
    int off0 = 0;
    int off1 = off0 + (csax[1] >> 16);
    int off2 = off1 + (csax[2] >> 16);
    int off3 = off2 + (csax[3] >> 16);
    
    /* Load corner pixels for all 4 destination pixels */
    const tColorRGBA *c00_0 = c00_base + off0;
    const tColorRGBA *c01_0 = c00_0 + 1;
    const tColorRGBA *c10_0 = c10_base + off0;
    const tColorRGBA *c11_0 = c10_0 + 1;
    
    const tColorRGBA *c00_1 = c00_base + off1;
    const tColorRGBA *c01_1 = c00_1 + 1;
    const tColorRGBA *c10_1 = c10_base + off1;
    const tColorRGBA *c11_1 = c10_1 + 1;
    
    const tColorRGBA *c00_2 = c00_base + off2;
    const tColorRGBA *c01_2 = c00_2 + 1;
    const tColorRGBA *c10_2 = c10_base + off2;
    const tColorRGBA *c11_2 = c10_2 + 1;
    
    const tColorRGBA *c00_3 = c00_base + off3;
    const tColorRGBA *c01_3 = c00_3 + 1;
    const tColorRGBA *c10_3 = c10_base + off3;
    const tColorRGBA *c11_3 = c10_3 + 1;
    
    /* Process each destination pixel with vectorized calculations */
    /* For simplicity, we use scalar NEON for interleaved channel data */
    
    /* Create weight vectors */
    int16x4_t vex = { (int16_t)(ex0 >> 8), (int16_t)(ex1 >> 8), 
                      (int16_t)(ex2 >> 8), (int16_t)(ex3 >> 8) };
    int16x4_t vey = vdup_n_s16((int16_t)(ey >> 8));
    int16x4_t v256 = vdup_n_s16(256);
    int16x4_t vex_inv = vsub_s16(v256, vex);
    int16x4_t vey_inv = vsub_s16(v256, vey);
    
    /* Process R channel */
    {
        int16x4_t c00_r = { c00_0->r, c00_1->r, c00_2->r, c00_3->r };
        int16x4_t c01_r = { c01_0->r, c01_1->r, c01_2->r, c01_3->r };
        int16x4_t c10_r = { c10_0->r, c10_1->r, c10_2->r, c10_3->r };
        int16x4_t c11_r = { c11_0->r, c11_1->r, c11_2->r, c11_3->r };
        
        /* t1 = c00 * (256-ex)/256 + c01 * ex/256 */
        int32x4_t t1 = vmull_s16(c00_r, vex_inv);
        t1 = vmlal_s16(t1, c01_r, vex);
        
        /* t2 = c10 * (256-ex)/256 + c11 * ex/256 */
        int32x4_t t2 = vmull_s16(c10_r, vex_inv);
        t2 = vmlal_s16(t2, c11_r, vex);
        
        /* Reduce to 16-bit and do vertical interpolation */
        int16x4_t t1_16 = vshrn_n_s32(t1, 8);
        int16x4_t t2_16 = vshrn_n_s32(t2, 8);
        
        int32x4_t result = vmull_s16(t1_16, vey_inv);
        result = vmlal_s16(result, t2_16, vey);
        
        int16x4_t r_out = vshrn_n_s32(result, 8);
        
        dp[0].r = vget_lane_s16(r_out, 0);
        dp[1].r = vget_lane_s16(r_out, 1);
        dp[2].r = vget_lane_s16(r_out, 2);
        dp[3].r = vget_lane_s16(r_out, 3);
    }
    
    /* Process G channel */
    {
        int16x4_t c00_g = { c00_0->g, c00_1->g, c00_2->g, c00_3->g };
        int16x4_t c01_g = { c01_0->g, c01_1->g, c01_2->g, c01_3->g };
        int16x4_t c10_g = { c10_0->g, c10_1->g, c10_2->g, c10_3->g };
        int16x4_t c11_g = { c11_0->g, c11_1->g, c11_2->g, c11_3->g };
        
        int32x4_t t1 = vmull_s16(c00_g, vex_inv);
        t1 = vmlal_s16(t1, c01_g, vex);
        
        int32x4_t t2 = vmull_s16(c10_g, vex_inv);
        t2 = vmlal_s16(t2, c11_g, vex);
        
        int16x4_t t1_16 = vshrn_n_s32(t1, 8);
        int16x4_t t2_16 = vshrn_n_s32(t2, 8);
        
        int32x4_t result = vmull_s16(t1_16, vey_inv);
        result = vmlal_s16(result, t2_16, vey);
        
        int16x4_t g_out = vshrn_n_s32(result, 8);
        
        dp[0].g = vget_lane_s16(g_out, 0);
        dp[1].g = vget_lane_s16(g_out, 1);
        dp[2].g = vget_lane_s16(g_out, 2);
        dp[3].g = vget_lane_s16(g_out, 3);
    }
    
    /* Process B channel */
    {
        int16x4_t c00_b = { c00_0->b, c00_1->b, c00_2->b, c00_3->b };
        int16x4_t c01_b = { c01_0->b, c01_1->b, c01_2->b, c01_3->b };
        int16x4_t c10_b = { c10_0->b, c10_1->b, c10_2->b, c10_3->b };
        int16x4_t c11_b = { c11_0->b, c11_1->b, c11_2->b, c11_3->b };
        
        int32x4_t t1 = vmull_s16(c00_b, vex_inv);
        t1 = vmlal_s16(t1, c01_b, vex);
        
        int32x4_t t2 = vmull_s16(c10_b, vex_inv);
        t2 = vmlal_s16(t2, c11_b, vex);
        
        int16x4_t t1_16 = vshrn_n_s32(t1, 8);
        int16x4_t t2_16 = vshrn_n_s32(t2, 8);
        
        int32x4_t result = vmull_s16(t1_16, vey_inv);
        result = vmlal_s16(result, t2_16, vey);
        
        int16x4_t b_out = vshrn_n_s32(result, 8);
        
        dp[0].b = vget_lane_s16(b_out, 0);
        dp[1].b = vget_lane_s16(b_out, 1);
        dp[2].b = vget_lane_s16(b_out, 2);
        dp[3].b = vget_lane_s16(b_out, 3);
    }
    
    /* Process A channel */
    {
        int16x4_t c00_a = { c00_0->a, c00_1->a, c00_2->a, c00_3->a };
        int16x4_t c01_a = { c01_0->a, c01_1->a, c01_2->a, c01_3->a };
        int16x4_t c10_a = { c10_0->a, c10_1->a, c10_2->a, c10_3->a };
        int16x4_t c11_a = { c11_0->a, c11_1->a, c11_2->a, c11_3->a };
        
        int32x4_t t1 = vmull_s16(c00_a, vex_inv);
        t1 = vmlal_s16(t1, c01_a, vex);
        
        int32x4_t t2 = vmull_s16(c10_a, vex_inv);
        t2 = vmlal_s16(t2, c11_a, vex);
        
        int16x4_t t1_16 = vshrn_n_s32(t1, 8);
        int16x4_t t2_16 = vshrn_n_s32(t2, 8);
        
        int32x4_t result = vmull_s16(t1_16, vey_inv);
        result = vmlal_s16(result, t2_16, vey);
        
        int16x4_t a_out = vshrn_n_s32(result, 8);
        
        dp[0].a = vget_lane_s16(a_out, 0);
        dp[1].a = vget_lane_s16(a_out, 1);
        dp[2].a = vget_lane_s16(a_out, 2);
        dp[3].a = vget_lane_s16(a_out, 3);
    }
}
#endif /* __ARM_NEON */

/* 
 
 32bit Zoomer with optional anti-aliasing by bilinear interpolation.

 Zoomes 32bit RGBA/ABGR 'src' surface to 'dst' surface.
 
*/

int
zoomSurfaceRGBA (SDL_Surface * src, SDL_Surface * dst, int smooth)
{
  int x, y, sx, sy, *sax, *say, *csax, *csay, csx, csy, ex, ey, t1, t2, sstep;
  tColorRGBA *c00, *c01, *c10, *c11;
  tColorRGBA *sp, *csp, *dp;
  int sgap, dgap, orderRGBA;

  /* Variable setup */
  if (smooth)
    {
      /* For interpolation: assume source dimension is one pixel */
      /* smaller to avoid overflow on right and bottom edge.     */
      sx = (int) (65536.0 * (float) (src->w - 1) / (float) dst->w);
      sy = (int) (65536.0 * (float) (src->h - 1) / (float) dst->h);
    }
  else
    {
      sx = (int) (65536.0 * (float) src->w / (float) dst->w);
      sy = (int) (65536.0 * (float) src->h / (float) dst->h);
    }

  /* Allocate memory for row increments */
  if ((sax = (int *) malloc ((dst->w + 1) * sizeof (Uint32))) == NULL)
    {
      return (-1);
    }
  if ((say = (int *) malloc ((dst->h + 1) * sizeof (Uint32))) == NULL)
    {
      free (sax);
      return (-1);
    }

  /* Precalculate row increments */
  csx = 0;
  csax = sax;
  for (x = 0; x <= dst->w; x++)
    {
      *csax = csx;
      csax++;
      csx &= 0xffff;
      csx += sx;
    }
  csy = 0;
  csay = say;
  for (y = 0; y <= dst->h; y++)
    {
      *csay = csy;
      csay++;
      csy &= 0xffff;
      csy += sy;
    }

  /* Pointer setup */
  sp = csp = (tColorRGBA *) src->pixels;
  dp = (tColorRGBA *) dst->pixels;
  sgap = src->pitch - src->w * 4;
  dgap = dst->pitch - dst->w * 4;
  orderRGBA = (src->format->Rmask == 0x000000ff);

  /* Switch between interpolating and non-interpolating code */
  if (smooth)
    {

      /* Interpolating Zoom */

      /* Scan destination */
      csay = say;
      for (y = 0; y < dst->h; y++)
	{
	  /* Prefetch next source row for better cache utilization */
	  PREFETCH(csp + src->w);
	  PREFETCH((Uint8 *)csp + src->pitch + src->w * 4);
	  
	  /* Setup color source pointers */
	  c00 = csp;
	  c01 = csp;
	  c01++;
	  c10 = (tColorRGBA *) ((Uint8 *) csp + src->pitch);
	  c11 = c10;
	  c11++;
	  csax = sax;

#if defined(USE_NEON_ASM) && defined(NEON_BILINEAR_INTERP_4PX_AVAILABLE) && NEON_BILINEAR_INTERP_4PX_AVAILABLE
	  /* Use assembly-optimized 4-pixel batch processing */
	  ey = (*csay & 0xffff);
	  
	  /* Track cumulative source offset for the batch processing */
	  int batch_src_offset = 0;
	  
	  /* Process 4 pixels at a time with NEON assembly */
	  for (x = 0; x + 4 <= dst->w; x += 4)
	    {
	      /* Prefetch ahead for next pixels (safe bounds check) */
	      if ((x & 15) == 0 && x + 8 < dst->w) {
	        PREFETCH(csp + batch_src_offset + 16);
	        PREFETCH((Uint8 *)csp + src->pitch + (batch_src_offset + 16) * 4);
	      }
	      
	      /* Call assembly bilinear interpolation for 4 pixels
	       * csax[0-3] are used for the current batch of 4 pixels
	       * The assembly uses csax[1-3]>>16 as incremental steps within the batch
	       */
	      NEON_BILINEAR_INTERP_4PX((uint32_t *)dp, (const uint32_t *)(csp + batch_src_offset),
	                               (const uint32_t *)((Uint8 *)(csp + batch_src_offset) + src->pitch),
	                               csax, ey);
	      
	      /* Update cumulative offset for the next batch:
	       * Add steps csax[1]>>16 (0→1), csax[2]>>16 (1→2), csax[3]>>16 (2→3)
	       * These are the steps between pixels within the batch we just processed.
	       * For the next batch, we also need csax[4]>>16 (3→next batch's 0),
	       * but we check bounds first.
	       */
	      batch_src_offset += (csax[1] >> 16);
	      batch_src_offset += (csax[2] >> 16);
	      batch_src_offset += (csax[3] >> 16);
	      
	      /* Advance csax pointer FIRST, then safely access the next step if needed */
	      csax += 4;
	      
	      /* If there's another batch or remainder pixels, add the step to next pixel */
	      if (x + 4 < dst->w) {
	          batch_src_offset += (csax[0] >> 16);  /* csax now points to next batch, [0] is the next pixel's step */
	      }
	      
	      /* Advance destination pointer */
	      dp += 4;
	    }
	  
	  /* Handle remaining pixels (0-3) with scalar code */
	  /* Use tracked cumulative offset instead of recalculating */
	  c00 = csp + batch_src_offset;
	  c01 = c00 + 1;
	  c10 = (tColorRGBA *)((Uint8 *)(csp + batch_src_offset) + src->pitch);
	  c11 = c10 + 1;
	  
	  for (; x < dst->w; x++)
	    {
	      ex = (*csax & 0xffff);
	      t1 = ((((c01->r - c00->r) * ex) >> 16) + c00->r) & 0xff;
	      t2 = ((((c11->r - c10->r) * ex) >> 16) + c10->r) & 0xff;
	      dp->r = (((t2 - t1) * ey) >> 16) + t1;
	      t1 = ((((c01->g - c00->g) * ex) >> 16) + c00->g) & 0xff;
	      t2 = ((((c11->g - c10->g) * ex) >> 16) + c10->g) & 0xff;
	      dp->g = (((t2 - t1) * ey) >> 16) + t1;
	      t1 = ((((c01->b - c00->b) * ex) >> 16) + c00->b) & 0xff;
	      t2 = ((((c11->b - c10->b) * ex) >> 16) + c10->b) & 0xff;
	      dp->b = (((t2 - t1) * ey) >> 16) + t1;
	      t1 = ((((c01->a - c00->a) * ex) >> 16) + c00->a) & 0xff;
	      t2 = ((((c11->a - c10->a) * ex) >> 16) + c10->a) & 0xff;
	      dp->a = (((t2 - t1) * ey) >> 16) + t1;
	      csax++;
	      sstep = (*csax >> 16);
	      c00 += sstep;
	      c01 += sstep;
	      c10 += sstep;
	      c11 += sstep;
	      dp++;
	    }
#else
	  /* Scalar fallback - process one pixel at a time */
	  for (x = 0; x < dst->w; x++)
	    {
	      /* Prefetch ahead for next pixels */
	      if ((x & 7) == 0) {
	        PREFETCH(c00 + 16);
	        PREFETCH(c10 + 16);
	      }
	      
	      /* ABGR ordering */
	      /* Interpolate colors */
	      ex = (*csax & 0xffff);
	      ey = (*csay & 0xffff);
	      t1 = ((((c01->r - c00->r) * ex) >> 16) + c00->r) & 0xff;
	      t2 = ((((c11->r - c10->r) * ex) >> 16) + c10->r) & 0xff;
	      dp->r = (((t2 - t1) * ey) >> 16) + t1;
	      t1 = ((((c01->g - c00->g) * ex) >> 16) + c00->g) & 0xff;
	      t2 = ((((c11->g - c10->g) * ex) >> 16) + c10->g) & 0xff;
	      dp->g = (((t2 - t1) * ey) >> 16) + t1;
	      t1 = ((((c01->b - c00->b) * ex) >> 16) + c00->b) & 0xff;
	      t2 = ((((c11->b - c10->b) * ex) >> 16) + c10->b) & 0xff;
	      dp->b = (((t2 - t1) * ey) >> 16) + t1;
	      t1 = ((((c01->a - c00->a) * ex) >> 16) + c00->a) & 0xff;
	      t2 = ((((c11->a - c10->a) * ex) >> 16) + c10->a) & 0xff;
	      dp->a = (((t2 - t1) * ey) >> 16) + t1;
	      /* Advance source pointers */
	      csax++;
	      sstep = (*csax >> 16);
	      c00 += sstep;
	      c01 += sstep;
	      c10 += sstep;
	      c11 += sstep;
	      /* Advance destination pointer */
	      dp++;
	    }
#endif
	  /* Advance source pointer */
	  csay++;
	  csp = (tColorRGBA *) ((Uint8 *) csp + (*csay >> 16) * src->pitch);
	  /* Advance destination pointers */
	  dp = (tColorRGBA *) ((Uint8 *) dp + dgap);
	}

    }
  else
    {

      /* Non-Interpolating Zoom */

      csay = say;
      for (y = 0; y < dst->h; y++)
	{
	  /* Prefetch next source row */
	  PREFETCH(csp + src->w);
	  
	  sp = csp;
	  csax = sax;
	  for (x = 0; x < dst->w; x++)
	    {
	      /* Prefetch ahead */
	      if ((x & 15) == 0) {
	        PREFETCH(sp + 32);
	      }
	      
	      /* Draw */
	      *dp = *sp;
	      /* Advance source pointers */
	      csax++;
	      sp += (*csax >> 16);
	      /* Advance destination pointer */
	      dp++;
	    }
	  /* Advance source pointer */
	  csay++;
	  csp = (tColorRGBA *) ((Uint8 *) csp + (*csay >> 16) * src->pitch);
	  /* Advance destination pointers */
	  dp = (tColorRGBA *) ((Uint8 *) dp + dgap);
	}

    }

  /* Remove temp arrays */
  free (sax);
  free (say);

  return (0);
}

/* 
 
 8bit Zoomer without smoothing.

 Zoomes 8bit palette/Y 'src' surface to 'dst' surface.
 
*/

int
zoomSurfaceY (SDL_Surface * src, SDL_Surface * dst)
{
  Uint32 x, y, sx, sy, *sax, *say, *csax, *csay, csx, csy;
  Uint8 *sp, *dp, *csp;
  int dgap;

  /* Variable setup */
  sx = (Uint32) (65536.0 * (float) src->w / (float) dst->w);
  sy = (Uint32) (65536.0 * (float) src->h / (float) dst->h);

  /* Allocate memory for row increments */
  if ((sax = (Uint32 *) malloc (dst->w * sizeof (Uint32))) == NULL)
    {
      return (-1);
    }
  if ((say = (Uint32 *) malloc (dst->h * sizeof (Uint32))) == NULL)
    {
      if (sax != NULL)
	{
	  free (sax);
	}
      return (-1);
    }

  /* Precalculate row increments */
  csx = 0;
  csax = sax;
  for (x = 0; x < dst->w; x++)
    {
      csx += sx;
      *csax = (csx >> 16);
      csx &= 0xffff;
      csax++;
    }
  csy = 0;
  csay = say;
  for (y = 0; y < dst->h; y++)
    {
      csy += sy;
      *csay = (csy >> 16);
      csy &= 0xffff;
      csay++;
    }

  csx = 0;
  csax = sax;
  for (x = 0; x < dst->w; x++)
    {
      csx += (*csax);
      csax++;
    }
  csy = 0;
  csay = say;
  for (y = 0; y < dst->h; y++)
    {
      csy += (*csay);
      csay++;
    }

  /* Pointer setup */
  sp = csp = (Uint8 *) src->pixels;
  dp = (Uint8 *) dst->pixels;
  dgap = dst->pitch - dst->w;

  /* Draw */
  csay = say;
  for (y = 0; y < dst->h; y++)
    {
      /* Prefetch next source row */
      PREFETCH(csp + src->w);
      
      csax = sax;
      sp = csp;
      for (x = 0; x < dst->w; x++)
	{
	  /* Prefetch ahead */
	  if ((x & 31) == 0) {
	    PREFETCH(sp + 64);
	  }
	  
	  /* Draw */
	  *dp = *sp;
	  /* Advance source pointers */
	  sp += (*csax);
	  csax++;
	  /* Advance destination pointer */
	  dp++;
	}
      /* Advance source pointer (for row) */
      csp += ((*csay) * src->pitch);
      csay++;
      /* Advance destination pointers */
      dp += dgap;
    }

  /* Remove temp arrays */
  free (sax);
  free (say);

  return (0);
}

/* 
 
 32bit Rotozoomer with optional anti-aliasing by bilinear interpolation.

 Rotates and zoomes 32bit RGBA/ABGR 'src' surface to 'dst' surface.
 
*/

void
transformSurfaceRGBA (SDL_Surface * src, SDL_Surface * dst, int cx, int cy,
		      int isin, int icos, int smooth)
{
  int x, y, t1, t2, dx, dy, xd, yd, sdx, sdy, ax, ay, ex, ey, sw, sh;
  tColorRGBA c00, c01, c10, c11;
  tColorRGBA *pc, *sp;
  int gap, orderRGBA;

  /* Variable setup */
  xd = ((src->w - dst->w) << 15);
  yd = ((src->h - dst->h) << 15);
  ax = (cx << 16) - (icos * cx);
  ay = (cy << 16) - (isin * cx);
  sw = src->w - 1;
  sh = src->h - 1;
  pc = (tColorRGBA*)dst->pixels;
  gap = dst->pitch - dst->w * 4;
  orderRGBA = (src->format->Rmask == 0x000000ff);

  /* Switch between interpolating and non-interpolating code */
  if (smooth)
    {
      for (y = 0; y < dst->h; y++)
	{
	  dy = cy - y;
	  sdx = (ax + (isin * dy)) + xd;
	  sdy = (ay - (icos * dy)) + yd;
	  for (x = 0; x < dst->w; x++)
	    {
	      dx = (sdx >> 16);
	      dy = (sdy >> 16);
	      if ((dx >= -1) && (dy >= -1) && (dx < src->w) && (dy < src->h))
		{
		  if ((dx >= 0) && (dy >= 0) && (dx < sw) && (dy < sh))
		    {
		      sp =
			(tColorRGBA *) ((Uint8 *) src->pixels +
					src->pitch * dy);
		      sp += dx;
		      c00 = *sp;
		      sp += 1;
		      c01 = *sp;
		      sp = (tColorRGBA *) ((Uint8 *) sp + src->pitch);
		      sp -= 1;
		      c10 = *sp;
		      sp += 1;
		      c11 = *sp;
		    }
		  else if ((dx == sw) && (dy == sh))
		    {
		      sp =
			(tColorRGBA *) ((Uint8 *) src->pixels +
					src->pitch * dy);
		      sp += dx;
		      c00 = *sp;
		      c01 = *pc;
		      c10 = *pc;
		      c11 = *pc;
		    }
		  else if ((dx == -1) && (dy == -1))
		    {
		      sp = (tColorRGBA *) (src->pixels);
		      c00 = *pc;
		      c01 = *pc;
		      c10 = *pc;
		      c11 = *sp;
		    }
		  else if ((dx == -1) && (dy == sh))
		    {
		      sp = (tColorRGBA *) (src->pixels);
		      sp =
			(tColorRGBA *) ((Uint8 *) src->pixels +
					src->pitch * dy);
		      c00 = *pc;
		      c01 = *sp;
		      c10 = *pc;
		      c11 = *pc;
		    }
		  else if ((dx == sw) && (dy == -1))
		    {
		      sp = (tColorRGBA *) (src->pixels);
		      sp += dx;
		      c00 = *pc;
		      c01 = *pc;
		      c10 = *sp;
		      c11 = *pc;
		    }
		  else if (dx == -1)
		    {
		      sp =
			(tColorRGBA *) ((Uint8 *) src->pixels +
					src->pitch * dy);
		      c00 = *pc;
		      c01 = *sp;
		      c10 = *pc;
		      sp = (tColorRGBA *) ((Uint8 *) sp + src->pitch);
		      c11 = *sp;
		    }
		  else if (dy == -1)
		    {
		      sp = (tColorRGBA *) (src->pixels);
		      sp += dx;
		      c00 = *pc;
		      c01 = *pc;
		      c10 = *sp;
		      sp += 1;
		      c11 = *sp;
		    }
		  else if (dx == sw)
		    {
		      sp =
			(tColorRGBA *) ((Uint8 *) src->pixels +
					src->pitch * dy);
		      sp += dx;
		      c00 = *sp;
		      c01 = *pc;
		      sp = (tColorRGBA *) ((Uint8 *) sp + src->pitch);
		      c10 = *sp;
		      c11 = *pc;
		    }
		  else if (dy == sh)
		    {
		      sp =
			(tColorRGBA *) ((Uint8 *) src->pixels +
					src->pitch * dy);
		      sp += dx;
		      c00 = *sp;
		      sp += 1;
		      c01 = *sp;
		      c10 = *pc;
		      c11 = *pc;
		    }
		  /* Interpolate colors */
		  ex = (sdx & 0xffff);
		  ey = (sdy & 0xffff);
		  t1 = ((((c01.r - c00.r) * ex) >> 16) + c00.r) & 0xff;
		  t2 = ((((c11.r - c10.r) * ex) >> 16) + c10.r) & 0xff;
		  pc->r = (((t2 - t1) * ey) >> 16) + t1;
		  t1 = ((((c01.g - c00.g) * ex) >> 16) + c00.g) & 0xff;
		  t2 = ((((c11.g - c10.g) * ex) >> 16) + c10.g) & 0xff;
		  pc->g = (((t2 - t1) * ey) >> 16) + t1;
		  t1 = ((((c01.b - c00.b) * ex) >> 16) + c00.b) & 0xff;
		  t2 = ((((c11.b - c10.b) * ex) >> 16) + c10.b) & 0xff;
		  pc->b = (((t2 - t1) * ey) >> 16) + t1;
		  t1 = ((((c01.a - c00.a) * ex) >> 16) + c00.a) & 0xff;
		  t2 = ((((c11.a - c10.a) * ex) >> 16) + c10.a) & 0xff;
		  pc->a = (((t2 - t1) * ey) >> 16) + t1;

		}
	      sdx += icos;
	      sdy += isin;
	      pc++;
	    }
	  pc = (tColorRGBA *) ((Uint8 *) pc + gap);
	}
    }
  else
    {
      for (y = 0; y < dst->h; y++)
	{
	  dy = cy - y;
	  sdx = (ax + (isin * dy)) + xd;
	  sdy = (ay - (icos * dy)) + yd;
	  for (x = 0; x < dst->w; x++)
	    {
	      dx = (short) (sdx >> 16);
	      dy = (short) (sdy >> 16);
	      if ((dx >= 0) && (dy >= 0) && (dx < src->w) && (dy < src->h))
		{
		  sp =
		    (tColorRGBA *) ((Uint8 *) src->pixels + src->pitch * dy);
		  sp += dx;
		  *pc = *sp;
		}
	      sdx += icos;
	      sdy += isin;
	      pc++;
	    }
	  pc = (tColorRGBA *) ((Uint8 *) pc + gap);
	}
    }
}

/* 
 
 8bit Rotozoomer without smoothing

 Rotates and zoomes 8bit palette/Y 'src' surface to 'dst' surface.
 
*/

void
transformSurfaceY (SDL_Surface * src, SDL_Surface * dst, int cx, int cy,
		   int isin, int icos)
{
  int x, y, dx, dy, xd, yd, sdx, sdy, ax, ay, sw, sh;
  tColorY *pc, *sp;
  int gap;

  /* Variable setup */
  xd = ((src->w - dst->w) << 15);
  yd = ((src->h - dst->h) << 15);
  ax = (cx << 16) - (icos * cx);
  ay = (cy << 16) - (isin * cx);
  sw = src->w - 1;
  sh = src->h - 1;
  pc = (tColorY*)dst->pixels;
  gap = dst->pitch - dst->w;
  /* Clear surface to colorkey */
  memset (pc, (unsigned char) (src->format->colorkey & 0xff),
	  dst->pitch * dst->h);
  /* Iterate through destination surface */
  for (y = 0; y < dst->h; y++)
    {
      dy = cy - y;
      sdx = (ax + (isin * dy)) + xd;
      sdy = (ay - (icos * dy)) + yd;
      for (x = 0; x < dst->w; x++)
	{
	  dx = (short) (sdx >> 16);
	  dy = (short) (sdy >> 16);
	  if ((dx >= 0) && (dy >= 0) && (dx < src->w) && (dy < src->h))
	    {
	      sp = (tColorY *) (src->pixels);
	      sp += (src->pitch * dy + dx);
	      *pc = *sp;
	    }
	  sdx += icos;
	  sdy += isin;
	  pc++;
	}
      pc += gap;
    }
}

/* 
 
 rotozoomSurface()

 Rotates and zoomes a 32bit or 8bit 'src' surface to newly created 'dst' surface.
 'angle' is the rotation in degrees. 'zoom' a scaling factor. If 'smooth' is 1
 then the destination 32bit surface is anti-aliased. If the surface is not 8bit
 or 32bit RGBA/ABGR it will be converted into a 32bit RGBA format on the fly.

*/

#define VALUE_LIMIT	0.001

SDL_Surface *
rotozoomSurface (SDL_Surface * src, double angle, double zoom, int smooth)
{
  SDL_Surface *rz_src;
  SDL_Surface *rz_dst;
  double zoominv;
  double radangle, sanglezoom, canglezoom, sanglezoominv, canglezoominv;
  int dstwidthhalf, dstwidth, dstheighthalf, dstheight;
  double x, y, cx, cy, sx, sy;
  int is32bit;
  int i, src_converted;

  /* Sanity check */
  if (src == NULL)
    return (NULL);

  /* Determine if source surface is 32bit or 8bit */
  is32bit = (src->format->BitsPerPixel == 32);
  if ((is32bit) || (src->format->BitsPerPixel == 8))
    {
      /* Use source surface 'as is' */
      rz_src = src;
      src_converted = 0;
    }
  else
    {
      /* New source surface is 32bit with a defined RGBA ordering */
      rz_src =
	SDL_CreateRGBSurface (SDL_SWSURFACE, src->w, src->h, 32, 0x000000ff,
			      0x0000ff00, 0x00ff0000, 0xff000000);
      SDL_BlitSurface (src, NULL, rz_src, NULL);
      src_converted = 1;
      is32bit = 1;
    }

  /* Sanity check zoom factor */
  if (zoom < VALUE_LIMIT)
    {
      zoom = VALUE_LIMIT;
    }
  zoominv = 65536.0 / zoom;

  /* Check if we have a rotozoom or just a zoom */
  if (fabs (angle) > VALUE_LIMIT)
    {

      /* Angle!=0: full rotozoom */
      /* ----------------------- */

      /* Calculate target factors from sin/cos and zoom */
      radangle = angle * (M_PI / 180.0);
      sanglezoom = sanglezoominv = sin (radangle);
      canglezoom = canglezoominv = cos (radangle);
      sanglezoom *= zoom;
      canglezoom *= zoom;
      sanglezoominv *= zoominv;
      canglezoominv *= zoominv;

      /* Determine destination width and height by rotating a centered source box */
      x = rz_src->w / 2;
      y = rz_src->h / 2;
      cx = canglezoom * x;
      cy = canglezoom * y;
      sx = sanglezoom * x;
      sy = sanglezoom * y;
      dstwidthhalf =
	MAX ((int)
	     ceil (MAX
		   (MAX
		    (MAX (fabs (cx + sy), fabs (cx - sy)), fabs (-cx + sy)),
		    fabs (-cx - sy))), 1);
      dstheighthalf =
	MAX ((int)
	     ceil (MAX
		   (MAX
		    (MAX (fabs (sx + cy), fabs (sx - cy)), fabs (-sx + cy)),
		    fabs (-sx - cy))), 1);
      dstwidth = 2 * dstwidthhalf;
      dstheight = 2 * dstheighthalf;

      /* Alloc space to completely contain the rotated surface */
      rz_dst = NULL;
      if (is32bit)
	{
	  /* Target surface is 32bit with source RGBA/ABGR ordering */
	  rz_dst =
	    SDL_CreateRGBSurface (SDL_SWSURFACE, dstwidth, dstheight, 32,
				  rz_src->format->Rmask,
				  rz_src->format->Gmask,
				  rz_src->format->Bmask,
				  rz_src->format->Amask);
	}
      else
	{
	  /* Target surface is 8bit */
	  rz_dst =
	    SDL_CreateRGBSurface (SDL_SWSURFACE, dstwidth, dstheight, 8, 0, 0,
				  0, 0);
	}

      /* Lock source surface */
      SDL_LockSurface (rz_src);
      /* Check which kind of surface we have */
      if (is32bit)
	{
	  /* Call the 32bit transformation routine to do the rotation (using alpha) */
	  transformSurfaceRGBA (rz_src, rz_dst, dstwidthhalf, dstheighthalf,
				(int) (sanglezoominv),
				(int) (canglezoominv), smooth);
	  /* Turn on source-alpha support */
	  SDL_SetAlpha (rz_dst, SDL_SRCALPHA, 255);
	}
      else
	{
	  /* Copy palette and colorkey info */
	  for (i = 0; i < rz_src->format->palette->ncolors; i++)
	    {
	      rz_dst->format->palette->colors[i] =
		rz_src->format->palette->colors[i];
	    }
	  rz_dst->format->palette->ncolors = rz_src->format->palette->ncolors;
	  /* Call the 8bit transformation routine to do the rotation */
	  transformSurfaceY (rz_src, rz_dst, dstwidthhalf, dstheighthalf,
			     (int) (sanglezoominv), (int) (canglezoominv));
	  SDL_SetColorKey (rz_dst, SDL_SRCCOLORKEY | SDL_RLEACCEL,
			   rz_src->format->colorkey);
	}
      /* Unlock source surface */
      SDL_UnlockSurface (rz_src);

    }
  else
    {

      /* Angle=0: Just a zoom */
      /* -------------------- */

      /* Calculate target size and set rect */
      dstwidth = (int) ((double) rz_src->w * zoom);
      dstheight = (int) ((double) rz_src->h * zoom);
      if (dstwidth < 1)
	{
	  dstwidth = 1;
	}
      if (dstheight < 1)
	{
	  dstheight = 1;
	}

      /* Alloc space to completely contain the zoomed surface */
      rz_dst = NULL;
      if (is32bit)
	{
	  /* Target surface is 32bit with source RGBA/ABGR ordering */
	  rz_dst =
	    SDL_CreateRGBSurface (SDL_SWSURFACE, dstwidth, dstheight, 32,
				  rz_src->format->Rmask,
				  rz_src->format->Gmask,
				  rz_src->format->Bmask,
				  rz_src->format->Amask);
	}
      else
	{
	  /* Target surface is 8bit */
	  rz_dst =
	    SDL_CreateRGBSurface (SDL_SWSURFACE, dstwidth, dstheight, 8, 0, 0,
				  0, 0);
	}

      /* Lock source surface */
      SDL_LockSurface (rz_src);
      /* Check which kind of surface we have */
      if (is32bit)
	{
	  /* Call the 32bit transformation routine to do the zooming (using alpha) */
	  zoomSurfaceRGBA (rz_src, rz_dst, smooth);
	  /* Turn on source-alpha support */
	  SDL_SetAlpha (rz_dst, SDL_SRCALPHA, 255);
	}
      else
	{
	  /* Copy palette and colorkey info */
	  for (i = 0; i < rz_src->format->palette->ncolors; i++)
	    {
	      rz_dst->format->palette->colors[i] =
		rz_src->format->palette->colors[i];
	    }
	  rz_dst->format->palette->ncolors = rz_src->format->palette->ncolors;
	  /* Call the 8bit transformation routine to do the zooming */
	  zoomSurfaceY (rz_src, rz_dst);
	  SDL_SetColorKey (rz_dst, SDL_SRCCOLORKEY | SDL_RLEACCEL,
			   rz_src->format->colorkey);
	}
      /* Unlock source surface */
      SDL_UnlockSurface (rz_src);
    }

  /* Cleanup temp surface */
  if (src_converted)
    {
      SDL_FreeSurface (rz_src);
    }

  /* Return destination surface */
  return (rz_dst);
}

/* 
 
 zoomSurface()

 Zoomes a 32bit or 8bit 'src' surface to newly created 'dst' surface.
 'zoomx' and 'zoomy' are scaling factors for width and height. If 'smooth' is 1
 then the destination 32bit surface is anti-aliased. If the surface is not 8bit
 or 32bit RGBA/ABGR it will be converted into a 32bit RGBA format on the fly.

*/

#define VALUE_LIMIT	0.001

SDL_Surface *
zoomSurface (SDL_Surface * src, double zoomx, double zoomy, int smooth)
{
  SDL_Surface *rz_src;
  SDL_Surface *rz_dst;
  int dstwidth, dstheight;
  int is32bit;
  int i, src_converted;

  /* Sanity check */
  if (src == NULL)
    return (NULL);

  /* Determine if source surface is 32bit or 8bit */
  is32bit = (src->format->BitsPerPixel == 32);
  if ((is32bit) || (src->format->BitsPerPixel == 8))
    {
      /* Use source surface 'as is' */
      rz_src = src;
      src_converted = 0;
    }
  else
    {
      /* New source surface is 32bit with a defined RGBA ordering */
      rz_src =
	SDL_CreateRGBSurface (SDL_SWSURFACE, src->w, src->h, 32, 0x000000ff,
			      0x0000ff00, 0x00ff0000, 0xff000000);
      SDL_BlitSurface (src, NULL, rz_src, NULL);
      src_converted = 1;
      is32bit = 1;
    }

  /* Sanity check zoom factors */
  if (zoomx < VALUE_LIMIT)
    {
      zoomx = VALUE_LIMIT;
    }
  if (zoomy < VALUE_LIMIT)
    {
      zoomy = VALUE_LIMIT;
    }

  /* Calculate target size and set rect */
  dstwidth = (int) ((double) rz_src->w * zoomx);
  dstheight = (int) ((double) rz_src->h * zoomy);
  if (dstwidth < 1)
    {
      dstwidth = 1;
    }
  if (dstheight < 1)
    {
      dstheight = 1;
    }

  /* Alloc space to completely contain the zoomed surface */
  rz_dst = NULL;
  if (is32bit)
    {
      /* Target surface is 32bit with source RGBA/ABGR ordering */
      rz_dst =
	SDL_CreateRGBSurface (SDL_SWSURFACE, dstwidth, dstheight, 32,
			      rz_src->format->Rmask, rz_src->format->Gmask,
			      rz_src->format->Bmask, rz_src->format->Amask);
    }
  else
    {
      /* Target surface is 8bit */
      rz_dst =
	SDL_CreateRGBSurface (SDL_SWSURFACE, dstwidth, dstheight, 8, 0, 0, 0,
			      0);
    }

  /* Lock source surface */
  SDL_LockSurface (rz_src);
  /* Check which kind of surface we have */
  if (is32bit)
    {
      /* Call the 32bit transformation routine to do the zooming (using alpha) */
      zoomSurfaceRGBA (rz_src, rz_dst, smooth);
      /* Turn on source-alpha support */
      SDL_SetAlpha (rz_dst, SDL_SRCALPHA, 255);
    }
  else
    {
      /* Copy palette and colorkey info */
      for (i = 0; i < rz_src->format->palette->ncolors; i++)
	{
	  rz_dst->format->palette->colors[i] =
	    rz_src->format->palette->colors[i];
	}
      rz_dst->format->palette->ncolors = rz_src->format->palette->ncolors;
      /* Call the 8bit transformation routine to do the zooming */
      zoomSurfaceY (rz_src, rz_dst);
      SDL_SetColorKey (rz_dst, SDL_SRCCOLORKEY | SDL_RLEACCEL,
		       rz_src->format->colorkey);
    }
  /* Unlock source surface */
  SDL_UnlockSurface (rz_src);

  /* Cleanup temp surface */
  if (src_converted)
    {
      SDL_FreeSurface (rz_src);
    }

  /* Return destination surface */
  return (rz_dst);
}

#ifdef WIN32
 /* For DLL building under VC6 */
BOOL APIENTRY
DllMain (HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
      break;
    }
  return TRUE;
}
#endif
