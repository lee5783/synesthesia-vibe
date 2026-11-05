//
//  mesh.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-02.
//

#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "state.h"
#include "equation_drivers.h"

namespace render_core
{

float SquishToCenter(float x, float fExp)
{
    if (x > 0.5f)
        return powf(x*2-1, fExp)*0.5f + 0.5f;

    return (1-powf(1-x*2, fExp))*0.5f;
}

int GetNearestPow2Size(int w, int h)
{
	float fExp = logf( std::max(w,h)*0.75f + 0.25f*std::min(w,h) ) / logf(2.0f);
    float bias = 0.55f;
    if (fExp + bias >= 11.0f)   // ..don't jump to 2048x2048 quite as readily
        bias = 0.5f;
	int   nExp = (int)(fExp + bias);
	int log2size = (int)powf(2.0f, (float)nExp);
    return log2size;
}

void ComputeAspectRatios(int texSizeX, int texSizeY, float* aspectX, float* aspectY, float* invAspectX, float* invAspectY)
{
    *aspectX = (texSizeY > texSizeX) ? texSizeX/(float)texSizeY : 1.0f;
    *aspectY = (texSizeX > texSizeY) ? texSizeY/(float)texSizeX : 1.0f;
    *invAspectX = 1.0f/(*aspectX);
    *invAspectY = 1.0f/(*aspectY);
}

void UvToMathSpace(bool screenDependentRenderMode, float aspectX, float aspectY, float u, float v, float* rad, float* ang)
{
    // (screen space = -1..1 on both axes; corresponds to UV space)
    // uv space = [0..1] on both axes
    // "math" space = what the preset authors are used to:
    //      upper left = [0,0]
    //      bottom right = [1,1]
    //      rad == 1 at corners of screen
    //      ang == 0 at three o'clock, and increases counter-clockwise (to 6.28).
	float px, py;
	if (screenDependentRenderMode)
	{
		px = (u*2-1);  // probably 1.0
		py = (v*2-1);  // probably <1

		*rad = sqrtf(px*px+py*py);
	}
	else
	{
		px = (u*2-1)*aspectX;  // probably 1.0
		py = (v*2-1)*aspectY;  // probably <1

		*rad = sqrtf(px*px+py*py) / sqrtf(aspectX*aspectX+aspectY*aspectY);
	}
    *ang = atan2f(py, px);
    if (*ang < 0)
        *ang += 6.2831853071796f;
}

void BuildCompositeMesh(int gridX, int gridY, int viewportWidth, int viewportHeight,
                         bool screenDependentRenderMode, float aspectX, float aspectY,
                         Vertex* outVerts, int* outIndices)
{
	// note the +0.5-texel offset!
	//   (otherwise, a 1-pixel-wide line of the image would wrap at the top and left edges).
	memset(outVerts, 0, sizeof(Vertex)*gridX*gridY);
    float fHalfTexelW =  0.5f / (float)viewportWidth;   // 2.5: 2 pixels bad @ bottom right
    float fHalfTexelH =  0.5f / (float)viewportHeight;
    float fDivX = 1.0f / (float)(gridX-2);
    float fDivY = 1.0f / (float)(gridY-2);
    for (int j=0; j<gridY; j++)
    {
        int j2 = j - j/(gridY/2);
        float v = j2*fDivY;
        v = SquishToCenter(v, 3.0f);
        float sy = -((v-fHalfTexelH)*2-1);//fOnePlusInvHeight*v*2-1;
        for (int i=0; i<gridX; i++)
        {
            int i2 = i - i/(gridX/2);
            float u = i2*fDivX;
            u = SquishToCenter(u, 3.0f);
            float sx = (u-fHalfTexelW)*2-1;//fOnePlusInvWidth*u*2-1;
            Vertex* p = &outVerts[i + j*gridX];
            p->x = sx;
            p->y = sy;
            p->z = 0;
            float rad, ang;
            UvToMathSpace( screenDependentRenderMode, aspectX, aspectY, u, v, &rad, &ang );
                // fix-ups:
               if (i==gridX/2-1) {
                   if (j < gridY/2-1)
                       ang = 3.1415926535898f*1.5f;
                   else if (j == gridY/2-1)
                       ang = 3.1415926535898f*1.25f;
                   else if (j == gridY/2)
                       ang = 3.1415926535898f*0.75f;
                   else
                       ang = 3.1415926535898f*0.5f;
               }
               else if (i==gridX/2) {
                   if (j < gridY/2-1)
                       ang = 3.1415926535898f*1.5f;
                   else if (j == gridY/2-1)
                       ang = 3.1415926535898f*1.75f;
                   else if (j == gridY/2)
                       ang = 3.1415926535898f*0.25f;
                   else
                       ang = 3.1415926535898f*0.5f;
               }
               else if (j==gridY/2-1) {
                   if (i < gridX/2-1)
                       ang = 3.1415926535898f*1.0f;
                   else if (i == gridX/2-1)
                       ang = 3.1415926535898f*1.25f;
                   else if (i == gridX/2)
                       ang = 3.1415926535898f*1.75f;
                   else
                       ang = 3.1415926535898f*2.0f;
               }
               else if (j==gridY/2) {
                   if (i < gridX/2-1)
                       ang = 3.1415926535898f*1.0f;
                   else if (i == gridX/2-1)
                       ang = 3.1415926535898f*0.75f;
                   else if (i == gridX/2)
                       ang = 3.1415926535898f*0.25f;
                   else
                       ang = 3.1415926535898f*0.0f;
               }
            p->tu = u;
            p->tv = v;
            //p->tu_orig = u;
            //p->tv_orig = v;
            p->rad = rad;
            p->ang = ang;
            p->Diffuse = 0xFFFFFFFF;
        }
    }

    // build index list for final composite blit -
    // order should be friendly for interpolation of 'ang' value!
    int* cur_index = &outIndices[0];
    for (int y=0; y<gridY-1; y++)
    {
        if (y==gridY/2-1)
            continue;
        for (int x=0; x<gridX-1; x++)
        {
            if (x==gridX/2-1)
                continue;
            bool left_half = (x < gridX/2);
            bool top_half  = (y < gridY/2);
            bool center_4 = ((x==gridX/2 || x==gridX/2-1) && (y==gridY/2 || y==gridY/2-1));

            if ( ((int)left_half + (int)top_half + (int)center_4) % 2 )
            {
                *(cur_index+0) = (y  )*gridX + (x  );
                *(cur_index+1) = (y  )*gridX + (x+1);
                *(cur_index+2) = (y+1)*gridX + (x+1);
                *(cur_index+3) = (y+1)*gridX + (x+1);
                *(cur_index+4) = (y+1)*gridX + (x  );
                *(cur_index+5) = (y  )*gridX + (x  );
            }
            else
            {
                *(cur_index+0) = (y+1)*gridX + (x  );
                *(cur_index+1) = (y  )*gridX + (x  );
                *(cur_index+2) = (y  )*gridX + (x+1);
                *(cur_index+3) = (y  )*gridX + (x+1);
                *(cur_index+4) = (y+1)*gridX + (x+1);
                *(cur_index+5) = (y+1)*gridX + (x  );
            }
            cur_index += 6;
        }
    }
}

void GetSafeBlurMinMax(CState* pState, float* blur_min, float* blur_max)
{
    blur_min[0] = (float)*pState->var_pf_blur1min;
    blur_min[1] = (float)*pState->var_pf_blur2min;
    blur_min[2] = (float)*pState->var_pf_blur3min;
    blur_max[0] = (float)*pState->var_pf_blur1max;
    blur_max[1] = (float)*pState->var_pf_blur2max;
    blur_max[2] = (float)*pState->var_pf_blur3max;

    // check that precision isn't wasted in later blur passes [...min-max gap can't grow!]
    // also, if min-max are close to each other, push them apart:
    const float fMinDist = 0.1f;
    if (blur_max[0] - blur_min[0] < fMinDist) {
        float avg = (blur_min[0] + blur_max[0])*0.5f;
        blur_min[0] = avg - fMinDist*0.5f;
        blur_max[0] = avg - fMinDist*0.5f;
    }
    blur_max[1] = std::min(blur_max[0], blur_max[1]);
    blur_min[1] = std::max(blur_min[0], blur_min[1]);
    if (blur_max[1] - blur_min[1] < fMinDist) {
        float avg = (blur_min[1] + blur_max[1])*0.5f;
        blur_min[1] = avg - fMinDist*0.5f;
        blur_max[1] = avg - fMinDist*0.5f;
    }
    blur_max[2] = std::min(blur_max[1], blur_max[2]);
    blur_min[2] = std::max(blur_min[1], blur_min[2]);
    if (blur_max[2] - blur_min[2] < fMinDist) {
        float avg = (blur_min[2] + blur_max[2])*0.5f;
        blur_min[2] = avg - fMinDist*0.5f;
        blur_max[2] = avg - fMinDist*0.5f;
    }
}

void ComputeGridAlphaValues(const GridAlphaInput& input, Vertex* verts, const VertInfo* vertinfo)
{
    float fBlend = input.state->m_fBlendProgress;//max(0,min(1,(m_pState->m_fBlendProgress*1.6f - 0.3f)));
    //fBlend = 1-fBlend;  // <-- THIS IS THE KEY - FLIPS THE ALPHAS AND EVERYTHING ELSE JUST WORKS.
    bool bBlending = input.state->m_bBlending;//(fBlend >= 0.0001f && fBlend <= 0.9999f);


	// warp stuff
	float fWarpTime = input.time * input.state->m_fWarpAnimSpeed;
	float fWarpScaleInv = 1.0f / input.state->m_fWarpScale.eval(input.time);
	float f[4];
	f[0] = 11.68f + 4.0f*cosf(fWarpTime*1.413f + 10);
	f[1] =  8.77f + 3.0f*cosf(fWarpTime*1.113f + 7);
	f[2] = 10.54f + 3.0f*cosf(fWarpTime*1.233f + 3);
	f[3] = 11.49f + 4.0f*cosf(fWarpTime*0.933f + 5);

	// texel alignment
	float texel_offset_x = 0.5f / (float)input.texSizeX;
	float texel_offset_y = 0.5f / (float)input.texSizeY;

    int num_reps = (input.state->m_bBlending) ? 2 : 1;
    int start_rep = 0;

    // FIRST WE HAVE 1-2 PASSES FOR CRUNCHING THE PER-VERTEX EQUATIONS
    for (int rep=start_rep; rep<num_reps; rep++)
	{
        // to blend the two PV equations together, we simulate both to get the final UV coords,
        // then we blend those final UV coords.  We also write out an alpha value so that
        // the second DRAW pass below (which might use a different shader) can do blending.
		CState *pState;

		if (rep==0)
			pState = input.state;
		else
			pState = input.oldState;

		// cache the doubles as floats so that computations are a bit faster
		float fZoom		= (float)(*pState->var_pf_zoom);
		float fZoomExp	= (float)(*pState->var_pf_zoomexp);
		float fRot		= (float)(*pState->var_pf_rot);
		float fWarp		= (float)(*pState->var_pf_warp);
		float fCX		= (float)(*pState->var_pf_cx);
		float fCY		= (float)(*pState->var_pf_cy);
		float fDX		= (float)(*pState->var_pf_dx);
		float fDY		= (float)(*pState->var_pf_dy);
		float fSX		= (float)(*pState->var_pf_sx);
		float fSY		= (float)(*pState->var_pf_sy);

		int n = 0;

		for (int y=0; y<=input.gridY; y++)
		{
			for (int x=0; x<=input.gridX; x++)
			{
				// Note: x, y, z are now set at init. time - no need to mess with them!
				EvaluatePerVertexEquations(pState, verts[n].x, verts[n].y, vertinfo[n].rad, vertinfo[n].ang,
				                           input.screenDependentRenderMode, input.aspectX, input.aspectY,
				                           fZoom, fZoomExp, fRot, fWarp, fCX, fCY, fDX, fDY, fSX, fSY);

				float fZoom2 = powf(fZoom, powf(fZoomExp, vertinfo[n].rad*2.0f - 1.0f));

				// initial texcoords, w/built-in zoom factor
				float fZoom2Inv = 1.0f/fZoom2;
				float u, v;

				if (input.screenDependentRenderMode)
				{
					u = verts[n].x*0.5f*fZoom2Inv+0.5f;
					v = -verts[n].y*0.5f*fZoom2Inv+0.5f;
				}
				else
				{
					u = verts[n].x*input.aspectX*0.5f*fZoom2Inv+0.5f;
					v = -verts[n].y*input.aspectY*0.5f*fZoom2Inv+0.5f;
				}

				// stretch on X, Y:
				u = (u - fCX)/fSX + fCX;
				v = (v - fCY)/fSY + fCY;

				// warping:
					u += fWarp*0.0035f*sinf(fWarpTime*0.333f + fWarpScaleInv*(verts[n].x*f[0] - verts[n].y*f[3]));
					v += fWarp*0.0035f*cosf(fWarpTime*0.375f - fWarpScaleInv*(verts[n].x*f[2] + verts[n].y*f[1]));
					u += fWarp*0.0035f*cosf(fWarpTime*0.753f - fWarpScaleInv*(verts[n].x*f[1] - verts[n].y*f[2]));
					v += fWarp*0.0035f*sinf(fWarpTime*0.825f + fWarpScaleInv*(verts[n].x*f[0] + verts[n].y*f[3]));

				// rotation:
				float u2 = u - fCX;
				float v2 = v - fCY;

				float cos_rot = cosf(fRot);
				float sin_rot = sinf(fRot);
				u = u2*cos_rot - v2*sin_rot + fCX;
				v = u2*sin_rot + v2*cos_rot + fCY;

				// translation:
				u -= fDX;
				v -= fDY;

                // undo aspect ratio fix:
				if (!input.screenDependentRenderMode)
				{
					u = (u - 0.5f) * input.invAspectX + 0.5f;
					v = (v - 0.5f) * input.invAspectY + 0.5f;
				}

				// final half-texel-offset translation:
				u += texel_offset_x;
				v += texel_offset_y;

                if (rep==0)
				{
                    // UV's for m_pState
					verts[n].tu = u;
					verts[n].tv = v;
					verts[n].Diffuse = 0xFFFFFFFF;
				}
				else
				{
                    // blend to UV's for m_pOldState
                    float mix2 = vertinfo[n].a*fBlend + vertinfo[n].c;//fCosineBlend2;
                    mix2 = std::max(0.0f, std::min(1.0f, mix2));
                    //     if fBlend un-flipped, then mix2 is 0 at the beginning of a blend, 1 at the end...
                    //                           and alphas are 0 at the beginning, 1 at the end.
					verts[n].tu = verts[n].tu*(mix2) + u*(1-mix2);
					verts[n].tv = verts[n].tv*(mix2) + v*(1-mix2);
                    // this sets the alpha values for blending between two presets:
					verts[n].Diffuse = 0x00FFFFFF | (((uint32_t)(mix2*255))<<24);
				}

				n++;
			}
		}

	}
}

}
