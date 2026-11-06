#ifndef SPIDERMAN_UI_TRANSITION_ATOMIC_MASK_H
#define SPIDERMAN_UI_TRANSITION_ATOMIC_MASK_H
/* Portable CPU portion of the opt-in D3D7 V10 atomically composited iris.
 * Original zero-alpha black source polygons are rasterized just ONCE into
 * one black/transparent texture. Each live frame ONLY changes a SINGLE
 * uniformly-scaled UV rectangle for the whole image. No per-quad motion.
 * Original black cinema bars are explicitly excluded by the caller.
 */
#include <stdint.h>
#include <string.h>
#include <math.h>
#define SPIDER_ATOMIC_MASK_MAX 128u
typedef struct SpiderAtomicMaskQuad {
    float x[4],y[4];
} SpiderAtomicMaskQuad;
static inline float SpiderAtomicMask_Edge(float ax,float ay,float bx,float by,
                                          float px,float py)
{
    return (px-ax)*(by-ay)-(py-ay)*(bx-ax);
}
static inline unsigned SpiderAtomicMask_Rasterize(
    const SpiderAtomicMaskQuad *quads,unsigned count,
    unsigned source_w,unsigned source_h,
    uint32_t *pixels,unsigned tex_w,unsigned tex_h)
{
    unsigned q,tri,painted=0u;
    float sx,sy;
    if(!quads || !pixels || !count || count>SPIDER_ATOMIC_MASK_MAX ||
       source_w<640u || source_h<480u || tex_w<64u || tex_h<64u ||
       tex_w>4096u || tex_h>4096u)return 0u;
    if((uint64_t)tex_w*tex_h>4096u*4096u)return 0u;
    memset(pixels,0,(size_t)tex_w*tex_h*sizeof(*pixels));
    sx=(float)tex_w/(float)source_w;
    sy=(float)tex_h/(float)source_h;
    for(q=0u;q<count;q++){
        for(tri=0u;tri<2u;tri++){
            unsigned inds[3],i,x,y,lo_x,hi_x,lo_y,hi_y;
            float vx[3],vy[3],minx,maxx,miny,maxy,area;
            inds[0]=0u;inds[1]=tri==0u?1u:2u;
            inds[2]=tri==0u?2u:3u;
            for(i=0u;i<3u;i++){
                vx[i]=quads[q].x[inds[i]]*sx;
                vy[i]=quads[q].y[inds[i]]*sy;
                if(!(vx[i]>-8192.0f && vx[i]<8192.0f &&
                     vy[i]>-8192.0f && vy[i]<8192.0f))return 0u;
            }
            area=SpiderAtomicMask_Edge(vx[0],vy[0],vx[1],vy[1],vx[2],vy[2]);
            if(area<.00001f && area>-.00001f)continue;
            minx=maxx=vx[0];miny=maxy=vy[0];
            for(i=1u;i<3u;i++){
                if(vx[i]<minx)minx=vx[i];
                if(vx[i]>maxx)maxx=vx[i];
                if(vy[i]<miny)miny=vy[i];
                if(vy[i]>maxy)maxy=vy[i];
            }
            if(maxx<=0.0f || maxy<=0.0f || minx>=(float)tex_w ||
               miny>=(float)tex_h)continue;
            lo_x=minx<=0.0f?0u:(unsigned)minx;
            lo_y=miny<=0.0f?0u:(unsigned)miny;
            hi_x=maxx>=(float)tex_w?tex_w:(unsigned)maxx+1u;
            hi_y=maxy>=(float)tex_h?tex_h:(unsigned)maxy+1u;
            for(y=lo_y;y<hi_y;y++){
                for(x=lo_x;x<hi_x;x++){
                    float px=(float)x+.5f,py=(float)y+.5f;
                    float a=SpiderAtomicMask_Edge(vx[0],vy[0],vx[1],vy[1],px,py);
                    float b=SpiderAtomicMask_Edge(vx[1],vy[1],vx[2],vy[2],px,py);
                    float c=SpiderAtomicMask_Edge(vx[2],vy[2],vx[0],vy[0],px,py);
                    if((a>=-.0001f && b>=-.0001f && c>=-.0001f) ||
                       (a<=.0001f && b<=.0001f && c<=.0001f)){
                        pixels[(size_t)y*tex_w+x]=0xFF000000u;
                        painted++;
                    }
                }
            }
        }
    }
    return painted;
}
/* Native source-iris geometry often replays the same polygon twice
 * in opposite triangle-fan orientation. Rasterize() returns the number
 * of TRIANGLE SAMPLE WRITES, which intentionally includes overlaps
 * and must NEVER be treated as the fraction of the texture covered.
 * Count final alpha pixels instead: each screen pixel is counted once
 * irrespective of how many original game primitives wrote to it.
 */
static inline unsigned SpiderAtomicMask_CountOpaquePixels(
    const uint32_t *pixels,unsigned width,unsigned height)
{
    size_t i,area;
    unsigned covered=0u;
    if(!pixels || width==0u || height==0u ||
       width>4096u || height>4096u)return 0u;
    area=(size_t)width*height;
    for(i=0u;i<area;i++)
        if((pixels[i]&0xFF000000u)==0xFF000000u)
            ++covered;
    return covered;
}
/* The original spider aperture is a 4:3 sprite, even when the D3D7
 * gameplay scene is presented across a 16:9 or ultrawide desktop.
 * The verified original PC quads give a WHOLE black/alpha composition,
 * but their source X was expanded to physical widescreen dimensions.
 *
 * Re-establish the original 4:3 proportions ONCE in texture space,
 * BEFORE any native game-driven uniform zoom. Never change an
 * individual tooth, quad, scene, or animation time. The entire
 * silhouette is sampled through ONE global affine inverse X map.
 *
 * Essential distinction from old V6-V9: the exterior of the finite
 * reference canvas is OPAQUE BLACK, not transparent. This preserves
 * its exterior matte even in the physical widescreen gutters.
 * Input and output may not overlap.
 */
static inline int SpiderAtomicMask_CorrectReference43(
    const uint32_t *native_rgba,uint32_t *corrected_rgba,
    unsigned tex_w,unsigned tex_h,unsigned display_w,unsigned display_h)
{
    unsigned y,x;
    float ratio,cx;
    if(!native_rgba || !corrected_rgba || native_rgba==corrected_rgba ||
       tex_w<64u || tex_h<64u || tex_w>4096u || tex_h>4096u ||
       display_w<640u || display_h<480u ||
       display_w>16384u || display_h>16384u ||
       (uint64_t)display_w*3u < (uint64_t)display_h*4u)
        return 0;
    ratio=((float)display_h*4.0f)/((float)display_w*3.0f);
    if(!(ratio>0.0f && ratio<=1.000001f))return 0;
    cx=(float)tex_w*.5f;
    for(y=0u;y<tex_h;y++){
        const uint32_t *src=native_rgba+(size_t)y*tex_w;
        uint32_t *dst=corrected_rgba+(size_t)y*tex_w;
        for(x=0u;x<tex_w;x++){
            /* Inverse of original 4:3 X squeeze, without scaling Y.
             * Thus a connected original aperture remains a single
             * connected image with the Dreamcast 4:3 silhouette. */
            float sx=cx+((float)x+.5f-cx)/ratio-.5f;
            int xi=(int)floorf(sx+.5f);
            dst[x]=(xi<0 || xi>=(int)tex_w) ? 0xFF000000u : src[xi];
        }
    }
    return 1;
}

/* Recover game-owned growth from a *source* quad by comparing it with
 * all reference quads. Native positions are quantized and culled near
 * the edges. Compare residuals after positive isotropic scaling about
 * the ORIGINAL source center. No timer and no previously corrected UI.
 */
static inline int SpiderAtomicMask_EstimateScale(
    const SpiderAtomicMaskQuad *reference,unsigned ref_count,
    const SpiderAtomicMaskQuad *observed,unsigned source_w,unsigned source_h,
    float *out_scale,float *out_error)
{
    unsigned j,k;
    float best_error=1.0e20f,best_scale=0.0f;
    float cx=(float)source_w*.5f,cy=(float)source_h*.5f;
    if(!reference || !observed || !out_scale || !ref_count ||
       ref_count>SPIDER_ATOMIC_MASK_MAX || source_w<640u || source_h<480u)
        return 0;
    for(j=0u;j<ref_count;j++){
        float dot=0.0f,energy=0.0f,err=0.0f,scale;
        for(k=0u;k<4u;k++){
            float rx=reference[j].x[k]-cx,ry=reference[j].y[k]-cy;
            float ox=observed->x[k]-cx,oy=observed->y[k]-cy;
            dot+=rx*ox+ry*oy;
            energy+=rx*rx+ry*ry;
        }
        if(energy<100.0f)continue;
        scale=dot/energy;
        if(!(scale>=.015f && scale<=24.0f))continue;
        for(k=0u;k<4u;k++){
            float dx=observed->x[k]-cx-(reference[j].x[k]-cx)*scale;
            float dy=observed->y[k]-cy-(reference[j].y[k]-cy)*scale;
            err+=dx*dx+dy*dy;
        }
        err=sqrtf(err*.25f);
        if(err<best_error){best_error=err;best_scale=scale;}
    }
    if(!(best_scale>.0f && best_error <= 2.5f+2.5f*best_scale &&
         best_error <= (float)source_w*.015f))return 0;
    *out_scale=best_scale;
    if(out_error)*out_error=best_error;
    return 1;
}
static inline int SpiderAtomicMask_ScreenUV(float scale,float *umin,
                                          float *vmin,float *umax,float *vmax)
{
    float half;
    if(!umin || !vmin || !umax || !vmax ||
       !(scale>=.015f && scale<=24.0f))return 0;
    half=.5f/scale;
    *umin=*vmin=.5f-half;
    *umax=*vmax=.5f+half;
    return 1;
}
#endif
