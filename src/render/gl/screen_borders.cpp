//
//  screen_borders.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "screen_borders.h"
#include "blend_state.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace render_gl
{

namespace
{

using WFVERTEX = render_core::Vertex;

uint32_t PackColorRgba01(float r, float g, float b, float a)
{
    return ((static_cast<uint32_t>(static_cast<int>(a * 255)) & 0xff) << 24)
        | ((static_cast<uint32_t>(static_cast<int>(r * 255)) & 0xff) << 16)
        | ((static_cast<uint32_t>(static_cast<int>(g * 255)) & 0xff) << 8)
        | (static_cast<uint32_t>(static_cast<int>(b * 255)) & 0xff);
}

}

void DrawDarkenCenter(const BorderDrawInput& input, const DrawProgram& program, MeshBuffer& mesh)
{
    CState* const m_pState = input.state;
    const bool m_bScreenDependentRenderMode = input.screenDependentRenderMode;
    const float m_fAspectY = input.aspectY;

	if (*m_pState->var_pf_darken_center)
	{
		program.Bind();
		SetBlendAlpha();

		WFVERTEX v3[6];
		memset(v3, 0, sizeof(WFVERTEX)*6);

		// colors:
		v3[0].Diffuse = PackColorRgba01(0, 0, 0, 3.0f/32.0f);
		v3[1].Diffuse = PackColorRgba01(0, 0, 0, 0.0f/32.0f);
		v3[2].Diffuse = v3[1].Diffuse;
		v3[3].Diffuse = v3[1].Diffuse;
		v3[4].Diffuse = v3[1].Diffuse;
		v3[5].Diffuse = v3[1].Diffuse;

		// positioning:
		float fHalfSize = 0.05f;
		v3[0].x = 0.0f;
		if (m_bScreenDependentRenderMode)
		v3[1].x = 0.0f - fHalfSize;
		else
		v3[1].x = 0.0f - fHalfSize*m_fAspectY;
		v3[2].x = 0.0f;
		if (m_bScreenDependentRenderMode)
		v3[3].x = 0.0f + fHalfSize;
		else
		v3[3].x = 0.0f + fHalfSize*m_fAspectY;
		v3[4].x = 0.0f;
		v3[5].x = v3[1].x;
		v3[0].y = 0.0f;
		v3[1].y = 0.0f;
		v3[2].y = 0.0f - fHalfSize;
		v3[3].y = 0.0f;
		v3[4].y = 0.0f + fHalfSize;
		v3[5].y = v3[1].y;
		//v3[0].tu = 0;	v3[1].tu = 1;	v3[2].tu = 0;	v3[3].tu = 1;
		//v3[0].tv = 1;	v3[1].tv = 1;	v3[2].tv = 0;	v3[3].tv = 0;

		mesh.UploadVertices(v3, 6);
		mesh.DrawTriangleFan(6);

		SetBlendOpaque();
	}
}

void DrawBorders(const BorderDrawInput& input, const DrawProgram& program, MeshBuffer& mesh)
{
    CState* const m_pState = input.state;

	{
		float fOuterBorderSize = (float)*m_pState->var_pf_ob_size;
		float fInnerBorderSize = (float)*m_pState->var_pf_ib_size;

		program.Bind();
		SetBlendAlpha();

		for (int it=0; it<2; it++)
		{
			WFVERTEX v3[4];
			memset(v3, 0, sizeof(WFVERTEX)*4);

			// colors:
			float r = (it==0) ? (float)*m_pState->var_pf_ob_r : (float)*m_pState->var_pf_ib_r;
			float g = (it==0) ? (float)*m_pState->var_pf_ob_g : (float)*m_pState->var_pf_ib_g;
			float b = (it==0) ? (float)*m_pState->var_pf_ob_b : (float)*m_pState->var_pf_ib_b;
			float a = (it==0) ? (float)*m_pState->var_pf_ob_a : (float)*m_pState->var_pf_ib_a;
			if (a > 0.001f)
			{
				v3[0].Diffuse = PackColorRgba01(r,g,b,a);
				v3[1].Diffuse = v3[0].Diffuse;
				v3[2].Diffuse = v3[0].Diffuse;
				v3[3].Diffuse = v3[0].Diffuse;

				// positioning:
				float fInnerRad = (it==0) ? 1.0f - fOuterBorderSize : 1.0f - fOuterBorderSize - fInnerBorderSize;
				float fOuterRad = (it==0) ? 1.0f                    : 1.0f - fOuterBorderSize;
				v3[0].x =  fInnerRad;
				v3[1].x =  fOuterRad;
				v3[2].x =  fOuterRad;
				v3[3].x =  fInnerRad;
				v3[0].y =  fInnerRad;
				v3[1].y =  fOuterRad;
				v3[2].y = -fOuterRad;
				v3[3].y = -fInnerRad;

				for (int rot=0; rot<4; rot++)
				{
					mesh.UploadVertices(v3, 4);
					mesh.DrawTriangleFan(4);

					// rotate by 90 degrees
					for (int v=0; v<4; v++)
					{
						float t = 1.570796327f;
						float x = v3[v].x;
						float y = v3[v].y;
						v3[v].x = x*cosf(t) - y*sinf(t);
						v3[v].y = x*sinf(t) + y*cosf(t);
					}
				}
			}
		}
		SetBlendOpaque();
	}
}

}
