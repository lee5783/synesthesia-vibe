//
//  custom_shapes.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "custom_shapes.h"
#include "blend_state.h"
#include "equation_drivers.h"
#include "mesh.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace render_gl
{

namespace
{

using SPRITEVERTEX = render_core::Vertex;
using WFVERTEX = render_core::Vertex;

}

void DrawCustomShapes(const CustomShapesInput& input, const DrawProgram& texturedProgram,
    const DrawProgram& untexturedProgram, MeshBuffer& mesh, const Texture& source)
{
    CState* const m_pState = input.state;
    CState* const m_pOldState = input.oldState;
    const bool m_bScreenDependentRenderMode = input.screenDependentRenderMode;
    const float m_fAspectY = input.aspectY;
    const int m_nTexSizeX = input.texSizeX;
    const int m_nTexSizeY = input.texSizeY;

	int num_reps = (m_pState->m_bBlending) ? 2 : 1;
	for (int rep=0; rep<num_reps; rep++)
	{
        CState *pState = (rep==0) ? m_pState : m_pOldState;
        float alpha_mult = 1;
        if (num_reps==2)
            alpha_mult = (rep==0) ? m_pState->m_fBlendProgress : (1-m_pState->m_fBlendProgress);

        for (int i=0; i<MAX_CUSTOM_SHAPES; i++)
        {
            if (pState->m_shape[i].enabled)
            {
                for (int instance=0; instance<pState->m_shape[i].instances; instance++)
                {
                    // 1. execute per-frame code
                    ::LoadCustomShapePerFrameEvallibVars(pState, i, instance, *input.frameInputs);

			        #ifndef _NO_EXPR_
				        if (pState->m_shape[i].m_pf_codehandle)
				        {
					        NSEEL_code_execute(pState->m_shape[i].m_pf_codehandle);
				        }
			        #endif

                    int sides = (int)(*pState->m_shape[i].var_pf_sides);
                    if (sides<3) sides=3;
                    if (sides>100) sides=100;

                    SetBlendState(true, BlendFactor::SrcAlpha, ((int)(*pState->m_shape[i].var_pf_additive) != 0) ? BlendFactor::One : BlendFactor::InvSrcAlpha);

                    SPRITEVERTEX v[512];  // for textured shapes (has texcoords)
                    WFVERTEX v2[512];     // for untextured shapes + borders
                    memset(v, 0, sizeof(SPRITEVERTEX) * (sides + 2));
                    memset(v2, 0, sizeof(WFVERTEX) * (sides + 2));
                    int j;

                    v[0].x = (float)(*pState->m_shape[i].var_pf_x* 2-1);// * ASPECT;
                    v[0].y = (float)(*pState->m_shape[i].var_pf_y*-2+1);
                    v[0].z = 0;
                    v[0].tu = 0.5f;
                    v[0].tv = 0.5f;
                    v[0].Diffuse =
                        ((((int)(*pState->m_shape[i].var_pf_a * 255 * alpha_mult)) & 0xFF) << 24) |
                        ((((int)(*pState->m_shape[i].var_pf_r * 255)) & 0xFF) << 16) |
                        ((((int)(*pState->m_shape[i].var_pf_g * 255)) & 0xFF) <<  8) |
                        ((((int)(*pState->m_shape[i].var_pf_b * 255)) & 0xFF)      );
                    v[1].Diffuse =
                        ((((int)(*pState->m_shape[i].var_pf_a2 * 255 * alpha_mult)) & 0xFF) << 24) |
                        ((((int)(*pState->m_shape[i].var_pf_r2 * 255)) & 0xFF) << 16) |
                        ((((int)(*pState->m_shape[i].var_pf_g2 * 255)) & 0xFF) <<  8) |
                        ((((int)(*pState->m_shape[i].var_pf_b2 * 255)) & 0xFF)      );

                    for (j=1; j<sides+1; j++)
                    {
                        float t = (j-1)/(float)sides;
						if (m_bScreenDependentRenderMode)
						v[j].x = v[0].x + (float)*pState->m_shape[i].var_pf_rad*cosf(t*3.1415927f*2 + (float)*pState->m_shape[i].var_pf_ang + 3.1415927f*0.25f);
						else
                        v[j].x = v[0].x + (float)*pState->m_shape[i].var_pf_rad*cosf(t*3.1415927f*2 + (float)*pState->m_shape[i].var_pf_ang + 3.1415927f*0.25f)*m_fAspectY;  // DON'T TOUCH!
                        v[j].y = v[0].y + (float)*pState->m_shape[i].var_pf_rad*sinf(t*3.1415927f*2 + (float)*pState->m_shape[i].var_pf_ang + 3.1415927f*0.25f);  // DON'T TOUCH!
                        v[j].z = 0;
						if (m_bScreenDependentRenderMode)
						v[j].tu = 0.5f + 0.5f*cosf(t*3.1415927f*2 + (float)*pState->m_shape[i].var_pf_tex_ang + 3.1415927f*0.25f)/((float)*pState->m_shape[i].var_pf_tex_zoom);
						else
                        v[j].tu = 0.5f + 0.5f*cosf(t*3.1415927f*2 + (float)*pState->m_shape[i].var_pf_tex_ang + 3.1415927f*0.25f)/((float)*pState->m_shape[i].var_pf_tex_zoom)*m_fAspectY;  // DON'T TOUCH!
                        v[j].tv = 0.5f + 0.5f*sinf(t*3.1415927f*2 + (float)*pState->m_shape[i].var_pf_tex_ang + 3.1415927f*0.25f)/((float)*pState->m_shape[i].var_pf_tex_zoom);  // DON'T TOUCH!
                        v[j].Diffuse = v[1].Diffuse;
                    }
                    v[sides+1] = v[1];

                    if ((int)(*pState->m_shape[i].var_pf_textured) != 0)
                    {
                        // draw textured version
                        texturedProgram.Bind();
                        texturedProgram.SetUvOffset(0.0f, 0.0f);
                        source.Bind();
                        mesh.UploadVertices(v, sides + 2);
                        mesh.DrawTriangleFan(sides + 2);
                    }
                    else
                    {
                        // no texture
                        for (j=0; j < sides+2; j++)
                        {
                            v2[j].x       = v[j].x;
                            v2[j].y       = v[j].y;
                            v2[j].z       = v[j].z;
                            v2[j].Diffuse = v[j].Diffuse;
                        }
                        untexturedProgram.Bind();
                        mesh.UploadVertices(v2, sides + 2);
                        mesh.DrawTriangleFan(sides + 2);
                    }

                    // DRAW BORDER
                    if (*pState->m_shape[i].var_pf_border_a > 0)
                    {
                        untexturedProgram.Bind();

                        v2[0].Diffuse =
                            ((((int)(*pState->m_shape[i].var_pf_border_a * 255 * alpha_mult)) & 0xFF) << 24) |
                            ((((int)(*pState->m_shape[i].var_pf_border_r * 255)) & 0xFF) << 16) |
                            ((((int)(*pState->m_shape[i].var_pf_border_g * 255)) & 0xFF) <<  8) |
                            ((((int)(*pState->m_shape[i].var_pf_border_b * 255)) & 0xFF)      );
                        for (j=0; j<sides+2; j++)
                        {
                            v2[j].x = v[j].x;
                            v2[j].y = v[j].y;
                            v2[j].z = v[j].z;
                            v2[j].Diffuse = v2[0].Diffuse;
                        }

                        int its = ((int)(*pState->m_shape[i].var_pf_thick) != 0) ? 4 : 1;
		                float x_inc = 2.0f / (float)m_nTexSizeX;
		                float y_inc = 2.0f / (float)m_nTexSizeY;
                        for (int it=0; it<its; it++)
                        {
			                switch(it)
			                {
			                case 0: break;
			                case 1: for (j=0; j<sides+2; j++) v2[j].x += x_inc; break;		// draw fat dots
			                case 2: for (j=0; j<sides+2; j++) v2[j].y += y_inc; break;		// draw fat dots
			                case 3: for (j=0; j<sides+2; j++) v2[j].x -= x_inc; break;		// draw fat dots
			                }
                            mesh.UploadVertices(v2, sides + 2);
                            mesh.DrawLineStrip(1, sides + 1);
                        }
                    }
                }
            }
        }
    }

	SetBlendOpaque();
}

}
