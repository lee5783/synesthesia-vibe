//
//  motion_vectors.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "motion_vectors.h"
#include "blend_state.h"

#include <algorithm>
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

bool ReversePropagatePoint(const render_core::Vertex* m_verts, int m_nGridX, int m_nGridY, float fx, float fy, float *fx2, float *fy2)
{
	//float fy = y/(float)nMotionVectorsY;
	int   y0 = (int)(fy*m_nGridY);
	float dy = fy*m_nGridY - y0;

	//float fx = x/(float)nMotionVectorsX;
	int   x0 = (int)(fx*m_nGridX);
	float dx = fx*m_nGridX - x0;

	int x1 = x0 + 1;
	int y1 = y0 + 1;

	if (x0 < 0) return false;
	if (y0 < 0) return false;
	//if (x1 < 0) return false;
	//if (y1 < 0) return false;
	//if (x0 > m_nGridX) return false;
	//if (y0 > m_nGridY) return false;
	if (x1 > m_nGridX) return false;
	if (y1 > m_nGridY) return false;

	float tu, tv;
	tu  = m_verts[y0*(m_nGridX+1)+x0].tu * (1-dx)*(1-dy);
	tv  = m_verts[y0*(m_nGridX+1)+x0].tv * (1-dx)*(1-dy);
	tu += m_verts[y0*(m_nGridX+1)+x1].tu * (dx)*(1-dy);
	tv += m_verts[y0*(m_nGridX+1)+x1].tv * (dx)*(1-dy);
	tu += m_verts[y1*(m_nGridX+1)+x0].tu * (1-dx)*(dy);
	tv += m_verts[y1*(m_nGridX+1)+x0].tv * (1-dx)*(dy);
	tu += m_verts[y1*(m_nGridX+1)+x1].tu * (dx)*(dy);
	tv += m_verts[y1*(m_nGridX+1)+x1].tv * (dx)*(dy);

	*fx2 = tu;
	*fy2 = 1.0f - tv;
	return true;
}

}

void DrawMotionVectors(const MotionVectorsInput& input, const DrawProgram& program, MeshBuffer& mesh)
{
    CState* const m_pState = input.state;
    const render_core::Vertex* const m_verts = input.verts;
    const int m_nGridX = input.gridX;
    const int m_nGridY = input.gridY;
    const int m_nTexSizeX = input.texSizeX;

    if ((float)*m_pState->var_pf_mv_a >= 0.001f)
    {
        program.Bind();

		int x,y;

		int nX = (int)(*m_pState->var_pf_mv_x);// + 0.999f);
		int nY = (int)(*m_pState->var_pf_mv_y);// + 0.999f);
		float dx = (float)*m_pState->var_pf_mv_x - nX;
		float dy = (float)*m_pState->var_pf_mv_y - nY;
		if (nX > 64) { nX = 64; dx = 0; }
		if (nY > 48) { nY = 48; dy = 0; }

		if (nX > 0 && nY > 0)
		{
			/*
			float dx2 = m_fMotionVectorsTempDx;//(*m_pState->var_pf_mv_dx) * 0.05f*GetTime();		// 0..1 range
			float dy2 = m_fMotionVectorsTempDy;//(*m_pState->var_pf_mv_dy) * 0.05f*GetTime();		// 0..1 range
			if (GetFps() > 2.0f && GetFps() < 300.0f)
			{
				dx2 += (float)(*m_pState->var_pf_mv_dx) * 0.05f / GetFps();
				dy2 += (float)(*m_pState->var_pf_mv_dy) * 0.05f / GetFps();
			}
			if (dx2 > 1.0f) dx2 -= (int)dx2;
			if (dy2 > 1.0f) dy2 -= (int)dy2;
			if (dx2 < 0.0f) dx2 = 1.0f - (-dx2 - (int)(-dx2));
			if (dy2 < 0.0f) dy2 = 1.0f - (-dy2 - (int)(-dy2));
			// hack: when there is only 1 motion vector on the screem, to keep it in
			//       the center, we gradually migrate it toward 0.5.
			dx2 = dx2*0.995f + 0.5f*0.005f;
			dy2 = dy2*0.995f + 0.5f*0.005f;
			// safety catch
			if (dx2 < 0 || dx2 > 1 || dy2 < 0 || dy2 > 1)
			{
				dx2 = 0.5f;
				dy2 = 0.5f;
			}
			m_fMotionVectorsTempDx = dx2;
			m_fMotionVectorsTempDy = dy2;*/
			float dx2 = (float)(*m_pState->var_pf_mv_dx);
			float dy2 = (float)(*m_pState->var_pf_mv_dy);

			float len_mult = (float)*m_pState->var_pf_mv_l;
			if (dx < 0) dx = 0;
			if (dy < 0) dy = 0;
			if (dx > 1) dx = 1;
			if (dy > 1) dy = 1;
			//dx = dx * 1.0f/(float)nX;
			//dy = dy * 1.0f/(float)nY;
			float inv_texsize = 1.0f/(float)m_nTexSizeX;
			float min_len = 1.0f*inv_texsize;

			WFVERTEX v[(64+1)*2];
			memset(v, 0, sizeof(WFVERTEX)*(64+1)*2);
			v[0].Diffuse = PackColorRgba01((float)*m_pState->var_pf_mv_r,(float)*m_pState->var_pf_mv_g,(float)*m_pState->var_pf_mv_b,(float)*m_pState->var_pf_mv_a);
			for (x=1; x<(nX+1)*2; x++)
				v[x].Diffuse = v[0].Diffuse;

			SetBlendAlpha();

			for (y=0; y<nY; y++)
			{
				float fy = (y + 0.25f)/(float)(nY + dy + 0.25f - 1.0f);

				// now move by offset
				fy -= dy2;

				if (fy > 0.0001f && fy < 0.9999f)
				{
					int n = 0;
					for (x=0; x<nX; x++)
					{
						//float fx = (x + 0.25f)/(float)(nX + dx + 0.25f - 1.0f);
						float fx = (x + 0.25f)/(float)(nX + dx + 0.25f - 1.0f);

						// now move by offset
						fx += dx2;

						if (fx > 0.0001f && fx < 0.9999f)
						{
							float fx2, fy2;
							ReversePropagatePoint(m_verts, m_nGridX, m_nGridY, fx, fy, &fx2, &fy2);	// NOTE: THIS IS REALLY A REVERSE-PROPAGATION
							//fx2 = fx*2 - fx2;
							//fy2 = fy*2 - fy2;
							//fx2 = fx + 1.0f/(float)m_nTexSize;
							//fy2 = 1-(fy + 1.0f/(float)m_nTexSize);

							// enforce minimum trail lengths:
							{
								float dx = (fx2 - fx);
								float dy = (fy2 - fy);
								dx *= len_mult;
								dy *= len_mult;
								float len = sqrtf(dx*dx + dy*dy);

								if (len > min_len)
								{

								}
								else if (len > 0.00000001f)
								{
									len = min_len/len;
									dx *= len;
									dy *= len;
								}
								else
								{
									dx = min_len;
									dy = min_len;
								}

								fx2 = fx + dx;
								fy2 = fy + dy;
							}
							/**/

							v[n].x = fx * 2.0f - 1.0f;
							v[n].y = fy * 2.0f - 1.0f;
							v[n+1].x = fx2 * 2.0f - 1.0f;
							v[n+1].y = fy2 * 2.0f - 1.0f;

							// actually, project it in the reverse direction
							//v[n+1].x = v[n].x*2.0f - v[n+1].x;// + dx*2;
							//v[n+1].y = v[n].y*2.0f - v[n+1].y;// + dy*2;
							//v[n].x += dx*2;
							//v[n].y += dy*2;

							n += 2;
						}
					}

					// draw it
					mesh.UploadVertices(v, n);
					mesh.DrawLineList(n);
				}
			}

			SetBlendOpaque();
		}
	}
}

}
