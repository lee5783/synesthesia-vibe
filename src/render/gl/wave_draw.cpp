//
//  wave_draw.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "wave_draw.h"
#include "blend_state.h"
#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace render_gl
{

namespace
{

using WFVERTEX = render_core::Vertex;

constexpr int kNumWaveformSamples = 480;

using std::max;
using std::min;

uint32_t PackColorRgba01(float r, float g, float b, float a)
{
    return ((static_cast<uint32_t>(static_cast<int>(a * 255)) & 0xff) << 24)
        | ((static_cast<uint32_t>(static_cast<int>(r * 255)) & 0xff) << 16)
        | ((static_cast<uint32_t>(static_cast<int>(g * 255)) & 0xff) << 8)
        | (static_cast<uint32_t>(static_cast<int>(b * 255)) & 0xff);
}

float CosineInterpolate(float x)
{
    return 0.5f - 0.5f * cosf(x * 3.1415926535898f);
}

int SmoothWave(WFVERTEX* vi, int nVertsIn, WFVERTEX* vo)
{
    const float c1 = -0.15f;
    const float c2 = 1.15f;
    const float c3 = 1.15f;
    const float c4 = -0.15f;
    const float inv_sum = 1.0f/(c1+c2+c3+c4);

    int j = 0;

    int i_below = 0;
    int i_above;
    int i_above2 = 1;
    for (int i=0; i<nVertsIn-1; i++)
    {
        i_above = i_above2;
        i_above2 = min(nVertsIn-1,i+2);
        vo[j] = vi[i];
        vo[j+1].x = (c1*vi[i_below].x + c2*vi[i].x + c3*vi[i_above].x + c4*vi[i_above2].x)*inv_sum;
        vo[j+1].y = (c1*vi[i_below].y + c2*vi[i].y + c3*vi[i_above].y + c4*vi[i_above2].y)*inv_sum;
        vo[j+1].z = 0;
        vo[j+1].Diffuse = vi[i].Diffuse;//0xFFFF0080;
        i_below = i;
        j += 2;
    }
    vo[j++] = vi[nVertsIn-1];

    return j;
}

}

void DrawWave(const WaveDrawInput& input, const float* fL, const float* fR, const DrawProgram& program, MeshBuffer& mesh)
{
    CState* const m_pState = input.state;
    const td_mysounddata& mysound = *input.mysound;
    const bool m_bScreenDependentRenderMode = input.screenDependentRenderMode;
    const float m_fAspectX = input.aspectX;
    const float m_fAspectY = input.aspectY;
    const int m_nTexSizeX = input.texSizeX;
    const int m_nTexSizeY = input.texSizeY;
    const auto GetTime = [&input]() { return input.time; };

    int i;
    WFVERTEX v1[576+1], v2[576+1];

    SetBlendState(true, BlendFactor::SrcAlpha, (*m_pState->var_pf_wave_additive) ? BlendFactor::One : BlendFactor::InvSrcAlpha);

	float cr = (float)(*m_pState->var_pf_wave_r);
	float cg = (float)(*m_pState->var_pf_wave_g);
	float cb = (float)(*m_pState->var_pf_wave_b);
	float cx = (float)(*m_pState->var_pf_wave_x);
	float cy = (float)(*m_pState->var_pf_wave_y); // note: it was backwards (top==1) in the original milkdrop, so we keep it that way!
	float fWaveParam = (float)(*m_pState->var_pf_wave_mystery);

	/*if (m_pState->m_bBlending)
	{
		cr = cr*(m_pState->m_fBlendProgress) + (1.0f-m_pState->m_fBlendProgress)*((float)(*m_pOldState->var_pf_wave_r));
		cg = cg*(m_pState->m_fBlendProgress) + (1.0f-m_pState->m_fBlendProgress)*((float)(*m_pOldState->var_pf_wave_g));
		cb = cb*(m_pState->m_fBlendProgress) + (1.0f-m_pState->m_fBlendProgress)*((float)(*m_pOldState->var_pf_wave_b));
		cx = cx*(m_pState->m_fBlendProgress) + (1.0f-m_pState->m_fBlendProgress)*((float)(*m_pOldState->var_pf_wave_x));
		cy = cy*(m_pState->m_fBlendProgress) + (1.0f-m_pState->m_fBlendProgress)*((float)(*m_pOldState->var_pf_wave_y));
		fWaveParam = fWaveParam*(m_pState->m_fBlendProgress) + (1.0f-m_pState->m_fBlendProgress)*((float)(*m_pOldState->var_pf_wave_mystery));
	}*/

	if (cr < 0) cr = 0;
	if (cg < 0) cg = 0;
	if (cb < 0) cb = 0;
	if (cr > 1) cr = 1;
	if (cg > 1) cg = 1;
	if (cb > 1) cb = 1;

	// maximize color:
	if (*m_pState->var_pf_wave_brighten)
	{
		float fMaximizeWaveColorAmount = 1.0f;
		float max = cr;
		if (max < cg) max = cg;
		if (max < cb) max = cb;
		if (max > 0.01f)
		{
			cr = cr/max*fMaximizeWaveColorAmount + cr*(1.0f - fMaximizeWaveColorAmount);
			cg = cg/max*fMaximizeWaveColorAmount + cg*(1.0f - fMaximizeWaveColorAmount);
			cb = cb/max*fMaximizeWaveColorAmount + cb*(1.0f - fMaximizeWaveColorAmount);
		}
	}

	float fWavePosX = cx*2.0f - 1.0f; // go from 0..1 user-range to -1..1 D3D range
	float fWavePosY = cy*2.0f - 1.0f;

	float bass_rel   = mysound.imm[0];
	float mid_rel    = mysound.imm[1];
	float treble_rel = mysound.imm[2];

	int sample_offset = 0;
	int new_wavemode = (int)(*m_pState->var_pf_wave_mode) % NUM_WAVES;  // since it can be changed from per-frame code!

	int its = (m_pState->m_bBlending && (new_wavemode != m_pState->m_nOldWaveMode)) ? 2 : 1;
	int nVerts1 = 0;
	int nVerts2 = 0;
	int nBreak1 = -1;
	int nBreak2 = -1;
	float alpha1, alpha2;

	for (int it=0; it<its; it++)
	{
		int   wave   = (it==0) ? new_wavemode : m_pState->m_nOldWaveMode;
		int   nVerts = kNumWaveformSamples;		// allowed to peek ahead 64 (i.e. left is [i], right is [i+64])
		int   nBreak = -1;

        float fWaveParam2 = fWaveParam;
        //std::string fWaveParam; // kill its scope
        if ((wave==0 || wave==1 || wave==4) && (fWaveParam2 < -1 || fWaveParam2 > 1))
        {
            //fWaveParam2 = max(fWaveParam2, -1.0f);
            //fWaveParam2 = min(fWaveParam2,  1.0f);
            fWaveParam2 = fWaveParam2*0.5f + 0.5f;
            fWaveParam2 -= floorf(fWaveParam2);
            fWaveParam2 = fabsf(fWaveParam2);
            fWaveParam2 = fWaveParam2*2-1;
        }

		WFVERTEX *v = (it==0) ? v1 : v2;
		memset(v, 0, sizeof(WFVERTEX)*nVerts);

		float alpha = (float)(*m_pState->var_pf_wave_a);//m_pState->m_fWaveAlpha.eval(GetTime());

		switch(wave)
		{
		case 0:
			// circular wave

			nVerts /= 2;
			sample_offset = (kNumWaveformSamples-nVerts)/2;//mysound.GoGoAlignatron(nVerts * 12/10);	// only call this once nVerts is final!

			if (m_pState->m_bModWaveAlphaByVolume)
				alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2])*0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime()))/(m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
			if (alpha < 0) alpha = 0;
			if (alpha > 1) alpha = 1;
			//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

			{
				float inv_nverts_minus_one = 1.0f/(float)(nVerts-1);

				for (i=0; i<nVerts; i++)
				{
					float rad = 0.5f + 0.4f*(fL[i+sample_offset]+fR[i+sample_offset])*0.5f + fWaveParam2;
					float ang = (i)*inv_nverts_minus_one*6.28f + GetTime()*0.2f;
					if (i < nVerts/10)
					{
						float mix = i/(nVerts*0.1f);
						mix = 0.5f - 0.5f*cosf(mix * 3.1416f);
						float rad_2 = 0.5f + 0.4f*(fL[i + nVerts + sample_offset]+fR[i + nVerts + sample_offset])*0.5f + fWaveParam2;
						rad = rad_2*(1.0f-mix) + rad*(mix);
					}
					if (m_bScreenDependentRenderMode)
					{
						v[i].x = rad*cosf(ang)+fWavePosX;
						v[i].y = rad*sinf(ang)+fWavePosY;
					}
					else
					{
						v[i].x = rad*cosf(ang)*m_fAspectY+fWavePosX;		// 0.75 = adj. for aspect ratio
						v[i].y = rad*sinf(ang)*m_fAspectX+fWavePosY;
					}
					//v[i].Diffuse = color;
				}
			}

			// dupe last vertex to connect the lines; skip if blending
			if (!m_pState->m_bBlending)
			{
				nVerts++;
				memcpy(&v[nVerts-1], &v[0], sizeof(WFVERTEX));
			}

			break;

		case 1:
			// x-y osc. that goes around in a spiral, in time

			alpha *= 1.25f;
			if (m_pState->m_bModWaveAlphaByVolume)
				alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2])*0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime()))/(m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
			if (alpha < 0) alpha = 0;
			if (alpha > 1) alpha = 1;
			//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

			nVerts /= 2;

			for (i=0; i<nVerts; i++)
			{
				float rad = 0.53f + 0.43f*fR[i] + fWaveParam2;
				float ang = fL[i+32] * 1.57f + GetTime()*2.3f;
				if (m_bScreenDependentRenderMode)
				{
					v[i].x = rad*cosf(ang)+fWavePosX;
					v[i].y = rad*sinf(ang)+fWavePosY;
				}
				else
				{
					v[i].x = rad*cosf(ang)*m_fAspectY+fWavePosX;		// 0.75 = adj. for aspect ratio
					v[i].y = rad*sinf(ang)*m_fAspectX+fWavePosY;
				}
				//v[i].Diffuse = color;//(D3DCOLOR_RGBA_01(cr, cg, cb, alpha*min(1, max(0, fL[i])));
			}

			break;

		case 2:
			// centered spiro (alpha constant)
			//	 aimed at not being so sound-responsive, but being very "nebula-like"
			//   difference is that alpha is constant (and faint), and waves a scaled way up

			switch(m_nTexSizeX)
			{
			case 256:  alpha *= 0.07f; break;
			case 512:  alpha *= 0.09f; break;
			case 1024: alpha *= 0.11f; break;
			case 2048: alpha *= 0.13f; break;
			}

			if (m_pState->m_bModWaveAlphaByVolume)
				alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2])*0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime()))/(m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
			if (alpha < 0) alpha = 0;
			if (alpha > 1) alpha = 1;
			//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

			for (i=0; i<nVerts; i++)
			{
				if (m_bScreenDependentRenderMode)
				{
					v[i].x = fR[i] + fWavePosX;//((pR[i] ^ 128) - 128)/90.0f * ASPECT; // 0.75 = adj. for aspect ratio
					v[i].y = fL[i + 32] + fWavePosY;//((pL[i+32] ^ 128) - 128)/90.0f;
				}
				else
				{
					v[i].x = fR[i] * m_fAspectY + fWavePosX;//((pR[i] ^ 128) - 128)/90.0f * ASPECT; // 0.75 = adj. for aspect ratio
					v[i].y = fL[i + 32] * m_fAspectX + fWavePosY;//((pL[i+32] ^ 128) - 128)/90.0f;
				}
				//v[i].Diffuse = color;
			}

			break;
		case 3:
			// centered spiro (alpha tied to volume)
			//	 aimed at having a strong audio-visual tie-in
			//   colors are always bright (no darks)

			switch(m_nTexSizeX)
			{
			case 256:  alpha = 0.075f; break;
			case 512:  alpha = 0.150f; break;
			case 1024: alpha = 0.220f; break;
			case 2048: alpha = 0.330f; break;
			}

			alpha *= 1.3f;
			alpha *= powf(treble_rel, 2.0f);
			if (m_pState->m_bModWaveAlphaByVolume)
				alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2])*0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime()))/(m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
			if (alpha < 0) alpha = 0;
			if (alpha > 1) alpha = 1;
			//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

			for (i=0; i<nVerts; i++)
			{
				if (m_bScreenDependentRenderMode)
				{
					v[i].x = fR[i]+fWavePosX;//((pR[i] ^ 128) - 128)/90.0f * ASPECT; // 0.75 = adj. for aspect ratio
					v[i].y = fL[i + 32]+fWavePosY;//((pL[i+32] ^ 128) - 128)/90.0f;
				}
				else
				{
					v[i].x = fR[i]*m_fAspectY+fWavePosX;//((pR[i] ^ 128) - 128)/90.0f * ASPECT; // 0.75 = adj. for aspect ratio
					v[i].y = fL[i + 32]*m_fAspectX+fWavePosY;//((pL[i+32] ^ 128) - 128)/90.0f;
				}
				//v[i].Diffuse = color;
			}
			break;
		case 4:
			// horizontal "script", left channel

			if (nVerts > m_nTexSizeX/3)
				nVerts = m_nTexSizeX/3;

			sample_offset = (kNumWaveformSamples-nVerts)/2;//mysound.GoGoAlignatron(nVerts + 25);	// only call this once nVerts is final!

			/*
			if (treble_rel > treb_thresh_for_wave6)
			{
				//alpha = 1.0f;
				treb_thresh_for_wave6 = treble_rel * 1.025f;
			}
			else
			{
				alpha *= 0.2f;
				treb_thresh_for_wave6 *= 0.996f;		// fixme: make this fps-independent
			}
			*/

			if (m_pState->m_bModWaveAlphaByVolume)
				alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2])*0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime()))/(m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
			if (alpha < 0) alpha = 0;
			if (alpha > 1) alpha = 1;
			//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

			{
				float w1 = 0.45f + 0.5f*(fWaveParam2*0.5f + 0.5f);		// 0.1 - 0.9
				float w2 = 1.0f - w1;

				float inv_nverts = 1.0f/(float)(nVerts);

				for (i=0; i<nVerts; i++)
				{
					v[i].x = -1.0f + 2.0f*(i*inv_nverts) + fWavePosX;
					v[i].y = 0.5f*(fL[i+sample_offset] + fR[i + sample_offset])*0.47f + fWavePosY;//((pL[i] ^ 128) - 128)/270.0f;
					v[i].x += fR[i+25+sample_offset]*0.44f;//((pR[i+25] ^ 128) - 128)/290.0f;
					//v[i].Diffuse = color;

					// momentum
					if (i>1)
					{
						v[i].x = v[i].x*w2 + w1*(v[i-1].x*2.0f - v[i-2].x);
						v[i].y = v[i].y*w2 + w1*(v[i-1].y*2.0f - v[i-2].y);
					}
				}

				/*
				// center on Y
				float avg_y = 0;
				for (i=0; i<nVerts; i++)
					avg_y += v[i].y;
				avg_y /= (float)nVerts;
				avg_y *= 0.5f;		// damp the movement
				for (i=0; i<nVerts; i++)
					v[i].y -= avg_y;
				*/
			}

			break;

		case 5:
			// weird explosive complex # thingy

			switch(m_nTexSizeX)
			{
			case 256:  alpha *= 0.07f; break;
			case 512:  alpha *= 0.09f; break;
			case 1024: alpha *= 0.11f; break;
			case 2048: alpha *= 0.13f; break;
			}

			if (m_pState->m_bModWaveAlphaByVolume)
				alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2])*0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime()))/(m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
			if (alpha < 0) alpha = 0;
			if (alpha > 1) alpha = 1;
			//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

			{
				float cos_rot = cosf(GetTime()*0.3f);
				float sin_rot = sinf(GetTime()*0.3f);

				for (i=0; i<nVerts; i++)
				{
					float x0 = (fR[i]*fL[i+32] + fL[i]*fR[i+32]);
					float y0 = (fR[i]*fR[i] - fL[i+32]*fL[i+32]);
					if (m_bScreenDependentRenderMode)
					{
						v[i].x = (x0*cos_rot - y0*sin_rot) + fWavePosX;
						v[i].y = (x0*sin_rot + y0*cos_rot) + fWavePosY;
					}
					else
					{
						v[i].x = (x0*cos_rot - y0*sin_rot)*m_fAspectY + fWavePosX;
						v[i].y = (x0*sin_rot + y0*cos_rot)*m_fAspectX + fWavePosY;
					}
					//v[i].Diffuse = color;
				}
			}

			break;

		case 6:
		case 7:
		case 8:
			// 6: angle-adjustable left channel, with temporal wave alignment;
			//   fWaveParam2 controls the angle at which it's drawn
			//	 fWavePosX slides the wave away from the center, transversely.
			//   fWavePosY does nothing
			//
			// 7: same, except there are two channels shown, and
			//   fWavePosY determines the separation distance.
			//
			// 8: same as 6, except using the spectrum analyzer (UNFINISHED)
			//
			nVerts /= 2;

			if (nVerts > m_nTexSizeX/3)
				nVerts = m_nTexSizeX/3;

			if (wave==8)
				nVerts = 256;
			else
				sample_offset = (kNumWaveformSamples-nVerts)/2;//mysound.GoGoAlignatron(nVerts);	// only call this once nVerts is final!

			if (m_pState->m_bModWaveAlphaByVolume)
				alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2])*0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime()))/(m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
			if (alpha < 0) alpha = 0;
			if (alpha > 1) alpha = 1;
			//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

			{
				float ang = 1.57f*fWaveParam2;	// from -PI/2 to PI/2
				float dx  = cosf(ang);
				float dy  = sinf(ang);

				float edge_x[2], edge_y[2];

				//edge_x[0] = fWavePosX - dx*3.0f;
				//edge_y[0] = fWavePosY - dy*3.0f;
				//edge_x[1] = fWavePosX + dx*3.0f;
				//edge_y[1] = fWavePosY + dy*3.0f;
				edge_x[0] = fWavePosX*cosf(ang + 1.57f) - dx*3.0f;
				edge_y[0] = fWavePosX*sinf(ang + 1.57f) - dy*3.0f;
				edge_x[1] = fWavePosX*cosf(ang + 1.57f) + dx*3.0f;
				edge_y[1] = fWavePosX*sinf(ang + 1.57f) + dy*3.0f;

				for (i=0; i<2; i++)	// for each point defining the line
				{
					// clip the point against 4 edges of screen
					// be a bit lenient (use +/-1.1 instead of +/-1.0)
					//	 so the dual-wave doesn't end too soon, after the channels are moved apart
					for (int j=0; j<4; j++)
					{
						float t;
						bool bClip = false;

						switch(j)
						{
						case 0:
							if (edge_x[i] > 1.1f)
							{
								t = (1.1f - edge_x[1-i]) / (edge_x[i] - edge_x[1-i]);
								bClip = true;
							}
							break;
						case 1:
							if (edge_x[i] < -1.1f)
							{
								t = (-1.1f - edge_x[1-i]) / (edge_x[i] - edge_x[1-i]);
								bClip = true;
								}
							break;
						case 2:
							if (edge_y[i] > 1.1f)
							{
								t = (1.1f - edge_y[1-i]) / (edge_y[i] - edge_y[1-i]);
								bClip = true;
							}
							break;
						case 3:
							if (edge_y[i] < -1.1f)
							{
								t = (-1.1f - edge_y[1-i]) / (edge_y[i] - edge_y[1-i]);
								bClip = true;
							}
							break;
						}

						if (bClip)
						{
							float dx = edge_x[i] - edge_x[1-i];
							float dy = edge_y[i] - edge_y[1-i];
							edge_x[i] = edge_x[1-i] + dx*t;
							edge_y[i] = edge_y[1-i] + dy*t;
						}
					}
				}

				dx = (edge_x[1] - edge_x[0]) / (float)nVerts;
				dy = (edge_y[1] - edge_y[0]) / (float)nVerts;
				float ang2 = atan2f(dy,dx);
				float perp_dx = cosf(ang2 + 1.57f);
				float perp_dy = sinf(ang2 + 1.57f);

				if (wave == 6)
					for (i=0; i<nVerts; i++)
					{
						v[i].x = edge_x[0] + dx*i + perp_dx*0.25f*(fL[i + sample_offset] + fR[i + sample_offset])*0.5f;
						v[i].y = edge_y[0] + dy*i + perp_dy*0.25f*(fL[i + sample_offset] + fR[i + sample_offset])*0.5f;
						//v[i].Diffuse = color;
					}
				else if (wave == 8)
					//256 verts
					for (i=0; i<nVerts; i++)
					{
						float f = 0.1f*logf((mysound.fSpecLeft[i*2] + mysound.fSpecLeft[i*2+1] + mysound.fSpecRight[i*2] + mysound.fSpecRight[i*2+1])*0.5f);
						v[i].x = edge_x[0] + dx*i + perp_dx*f;
						v[i].y = edge_y[0] + dy*i + perp_dy*f;
						//v[i].Diffuse = color;
					}
				else
				{
					float sep = powf(fWavePosY*0.5f + 0.5f, 2.0f);
					for (i=0; i<nVerts; i++)
					{
						v[i].x = edge_x[0] + dx*i + perp_dx*(0.25f*fL[i + sample_offset] + sep);
						v[i].y = edge_y[0] + dy*i + perp_dy*(0.25f*fL[i + sample_offset] + sep);
						//v[i].Diffuse = color;
					}

					//D3DPRIMITIVETYPE primtype = (*m_pState->var_pf_wave_usedots) ? D3DPT_POINTLIST : D3DPT_LINESTRIP;
					//m_lpD3DDev->DrawPrimitive(primtype, D3DFVF_LVERTEX, (LPVOID)v, nVerts, NULL);

					for (i=0; i<nVerts; i++)
					{
						v[i+nVerts].x = edge_x[0] + dx*i + perp_dx*(0.25f*fR[i + sample_offset] - sep);
						v[i+nVerts].y = edge_y[0] + dy*i + perp_dy*(0.25f*fR[i + sample_offset] - sep);
						//v[i+nVerts].Diffuse = color;
					}

					nBreak = nVerts;
					nVerts *= 2;
				}
			}

			break;
						case 9:
		// large wave
		nVerts /= 2;

		if (nVerts > m_nTexSizeX / 3)
			nVerts = m_nTexSizeX / 3;

		if (wave == 8)
			nVerts = 256;
		else
			sample_offset = (kNumWaveformSamples - nVerts) / 2;//mysound.GoGoAlignatron(nVerts);	// only call this once nVerts is final!

		if (m_pState->m_bModWaveAlphaByVolume)
			alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2]) * 0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime())) / (m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
		if (alpha < 0) alpha = 0;
		if (alpha > 1) alpha = 1;
		//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

		{
			float ang = 1.57f * fWaveParam2;	// from -PI/2 to PI/2
			float dx = cosf(ang);
			float dy = sinf(ang);

			float edge_x[2], edge_y[2];

			//edge_x[0] = fWavePosX - dx*3.0f;
			//edge_y[0] = fWavePosY - dy*3.0f;
			//edge_x[1] = fWavePosX + dx*3.0f;
			//edge_y[1] = fWavePosY + dy*3.0f;
			edge_x[0] = fWavePosX * cosf(ang + 1.57f) - dx * 3.0f;
			edge_y[0] = fWavePosX * sinf(ang + 1.57f) - dy * 3.0f;
			edge_x[1] = fWavePosX * cosf(ang + 1.57f) + dx * 3.0f;
			edge_y[1] = fWavePosX * sinf(ang + 1.57f) + dy * 3.0f;

			for (i = 0; i < 2; i++)	// for each point defining the line
			{
				// clip the point against 4 edges of screen
				// be a bit lenient (use +/-1.1 instead of +/-1.0)
				//	 so the dual-wave doesn't end too soon, after the channels are moved apart
				for (int j = 0; j < 4; j++)
				{
					float t;
					bool bClip = false;

					switch (j)
					{
					case 0:
						if (edge_x[i] > 1.1f)
						{
							t = (1.1f - edge_x[1 - i]) / (edge_x[i] - edge_x[1 - i]);
							bClip = true;
						}
						break;
					case 1:
						if (edge_x[i] < -1.1f)
						{
							t = (-1.1f - edge_x[1 - i]) / (edge_x[i] - edge_x[1 - i]);
							bClip = true;
						}
						break;
					case 2:
						if (edge_y[i] > 1.1f)
						{
							t = (1.1f - edge_y[1 - i]) / (edge_y[i] - edge_y[1 - i]);
							bClip = true;
						}
						break;
					case 3:
						if (edge_y[i] < -1.1f)
						{
							t = (-1.1f - edge_y[1 - i]) / (edge_y[i] - edge_y[1 - i]);
							bClip = true;
						}
						break;
					}

					if (bClip)
					{
						float dx = edge_x[i] - edge_x[1 - i];
						float dy = edge_y[i] - edge_y[1 - i];
						edge_x[i] = edge_x[1 - i] + dx * t;
						edge_y[i] = edge_y[1 - i] + dy * t;
					}
				}
			}

			dx = (edge_x[1] - edge_x[0]) / (float)nVerts;
			dy = (edge_y[1] - edge_y[0]) / (float)nVerts;
			float ang2 = atan2f(dy, dx);
			float perp_dx = cosf(ang2 + 1.57f);
			float perp_dy = sinf(ang2 + 1.57f);

			
				for (i = 0; i < nVerts; i++)
				{
					v[i].x = edge_x[0] + dx * i + perp_dx * 1.00f * (fL[i + sample_offset] + fR[i + sample_offset])*0.5f;
					v[i].y = edge_y[0] + dy * i + perp_dy * 1.00f * (fL[i + sample_offset] + fR[i + sample_offset])*0.5f;
					//v[i].Diffuse = color;
				}
			


				nBreak = nVerts;
				nVerts *= 2;
			}
		

		break;



		case 10:
		// x wave (It should be called X marks the spot, isn't it?)
			nVerts /= 2;

			if (nVerts > m_nTexSizeX / 3)
				nVerts = m_nTexSizeX / 3;

            sample_offset = (kNumWaveformSamples - nVerts) / 2;//mysound.GoGoAlignatron(nVerts);	// only call this once nVerts is final!

			if (m_pState->m_bModWaveAlphaByVolume)
				alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2]) * 0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime())) / (m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
			if (alpha < 0) alpha = 0;
			if (alpha > 1) alpha = 1;
			//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

			{
				float ang = -0.75 + fWaveParam2*3.15;	// from -PI/2 to PI/2
				float dx = cosf(ang);
				float dy = sinf(ang);

				float edge_x[2], edge_y[2];

				//edge_x[0] = fWavePosX - dx*3.0f;
				//edge_y[0] = fWavePosY - dy*3.0f;
				//edge_x[1] = fWavePosX + dx*3.0f;
				//edge_y[1] = fWavePosY + dy*3.0f;
				edge_x[0] = fWavePosX * cosf(ang + 1.57f) - dx * 3.0f;
				edge_y[0] = fWavePosX * sinf(ang + 1.57f) - dy * 3.0f;
				edge_x[1] = fWavePosX * cosf(ang + 1.57f) + dx * 3.0f;
				edge_y[1] = fWavePosX * sinf(ang + 1.57f) + dy * 3.0f;

				for (i = 0; i < 2; i++)	// for each point defining the line
				{
					// clip the point against 4 edges of screen
					// be a bit lenient (use +/-1.1 instead of +/-1.0)
					//	 so the dual-wave doesn't end too soon, after the channels are moved apart
					for (int j = 0; j < 4; j++)
					{
						float t;
						bool bClip = false;

						switch (j)
						{
						case 0:
							if (edge_x[i] > 1.1f)
							{
								t = (1.1f - edge_x[1 - i]) / (edge_x[i] - edge_x[1 - i]);
								bClip = true;
							}
							break;
						case 1:
							if (edge_x[i] < -1.1f)
							{
								t = (-1.1f - edge_x[1 - i]) / (edge_x[i] - edge_x[1 - i]);
								bClip = true;
							}
							break;
						case 2:
							if (edge_y[i] > 1.1f)
							{
								t = (1.1f - edge_y[1 - i]) / (edge_y[i] - edge_y[1 - i]);
								bClip = true;
							}
							break;
						case 3:
							if (edge_y[i] < -1.1f)
							{
								t = (-1.1f - edge_y[1 - i]) / (edge_y[i] - edge_y[1 - i]);
								bClip = true;
							}
							break;
						}

						if (bClip)
						{
							float dx = edge_x[i] - edge_x[1 - i];
							float dy = edge_y[i] - edge_y[1 - i];
							edge_x[i] = edge_x[1 - i] + dx * t;
							edge_y[i] = edge_y[1 - i] + dy * t;
						}
					}
				}

				dx = (edge_x[1] - edge_x[0]) / (float)nVerts;
				dy = (edge_y[1] - edge_y[0]) / (float)nVerts;
				float ang2 = atan2f(dy, dx);
				float perp_dx = cosf(ang2 + 1.57f);
				float perp_dy = sinf(ang2 + 1.57f);


				for (i = 0; i < nVerts; i++)
				{
					v[i].x = edge_x[0] + dx * i + perp_dx * 0.35f * fL[i + sample_offset];
					v[i].y = edge_y[0] + dy * i + perp_dy * 0.35f * fL[i + sample_offset];
					//v[i].Diffuse = color;
				}


		////////////////\\\\\\\\\\\\\\\\\\\\\

				float ang3 = 0.75 + fWaveParam2*3.15;	// from -PI/2 to PI/2
				float dx3 = cosf(ang3);
				float dy3 = sinf(ang3);

				float edge_x3[2], edge_y3[2];

				edge_x3[0] = fWavePosX * cosf(ang3 + 1.57f) - dx3 * 3.0f;
				edge_y3[0] = fWavePosX * sinf(ang3 + 1.57f) - dy3 * 3.0f;
				edge_x3[1] = fWavePosX * cosf(ang3 + 1.57f) + dx3 * 3.0f;
				edge_y3[1] = fWavePosX * sinf(ang3 + 1.57f) + dy3 * 3.0f;

				for (i = 0; i < 2; i++)	// for each point defining the line
				{
					for (int j = 0; j < 4; j++)
					{
						float t;
						bool bClip = false;

						switch (j)
						{
						case 0:
							if (edge_x3[i] > 1.1f)
							{
								t = (1.1f - edge_x3[1 - i]) / (edge_x3[i] - edge_x3[1 - i]);
								bClip = true;
							}
							break;
						case 1:
							if (edge_x3[i] < -1.1f)
							{
								t = (-1.1f - edge_x3[1 - i]) / (edge_x3[i] - edge_x3[1 - i]);
								bClip = true;
							}
							break;
						case 2:
							if (edge_y3[i] > 1.1f)
							{
								t = (1.1f - edge_y3[1 - i]) / (edge_y3[i] - edge_y3[1 - i]);
								bClip = true;
							}
							break;
						case 3:
							if (edge_y3[i] < -1.1f)
							{
								t = (-1.1f - edge_y3[1 - i]) / (edge_y3[i] - edge_y3[1 - i]);
								bClip = true;
							}
							break;
						}

						if (bClip)
						{
							float dx3 = edge_x3[i] - edge_x3[1 - i];
							float dy3 = edge_y3[i] - edge_y3[1 - i];
							edge_x3[i] = edge_x3[1 - i] + dx3 * t;
							edge_y3[i] = edge_y3[1 - i] + dy3 * t;
						}
					}
				}

				dx3 = (edge_x3[1] - edge_x3[0]) / (float)nVerts;
				dy3 = (edge_y3[1] - edge_y3[0]) / (float)nVerts;
				float ang4 = atan2f(dy3, dx3);
				float perp_dx3 = cosf(ang4 + 1.57f);
				float perp_dy3 = sinf(ang4 + 1.57f);

				for (i = 0; i < nVerts; i++)
				{
					v[i + nVerts].x = edge_x3[0] + dx3 * i + perp_dx3 * (0.35f * fR[i + sample_offset]);
					v[i + nVerts].y = edge_y3[0] + dy3 * i + perp_dy3 * (0.35f * fR[i + sample_offset]);
					//v[i+nVerts].Diffuse = color;
				}



				nBreak = nVerts;
				nVerts *= 2;
			}


			break;


			case 11:
			// vertical dual wave
				nVerts /= 2;

				if (nVerts > m_nTexSizeX / 3)
					nVerts = m_nTexSizeX / 3;

				sample_offset = (kNumWaveformSamples - nVerts) / 2;//mysound.GoGoAlignatron(nVerts);	// only call this once nVerts is final!

				if (m_pState->m_bModWaveAlphaByVolume)
					alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2]) * 0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime())) / (m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
				if (alpha < 0) alpha = 0;
				if (alpha > 1) alpha = 1;
				//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

				{
					float ang = 1.57;	// from -PI/2 to PI/2
					float dx = cosf(ang);
					float dy = sinf(ang);

					float edge_x[2], edge_y[2];

					//edge_x[0] = fWavePosX - dx*3.0f;
					//edge_y[0] = fWavePosY - dy*3.0f;
					//edge_x[1] = fWavePosX + dx*3.0f;
					//edge_y[1] = fWavePosY + dy*3.0f;
					edge_x[0] = fWavePosX * cosf(ang + 1.57f) - dx * 3.0f;
					edge_y[0] = fWavePosX * sinf(ang + 1.57f) - dy * 3.0f;
					edge_x[1] = fWavePosX * cosf(ang + 1.57f) + dx * 3.0f;
					edge_y[1] = fWavePosX * sinf(ang + 1.57f) + dy * 3.0f;

					for (i = 0; i < 2; i++)	// for each point defining the line
					{
						// clip the point against 4 edges of screen
						// be a bit lenient (use +/-1.1 instead of +/-1.0)
						//	 so the dual-wave doesn't end too soon, after the channels are moved apart
						for (int j = 0; j < 4; j++)
						{
							float t;
							bool bClip = false;

							switch (j)
							{
							case 0:
								if (edge_x[i] > 1.1f)
								{
									t = (1.1f - edge_x[1 - i]) / (edge_x[i] - edge_x[1 - i]);
									bClip = true;
								}
								break;
							case 1:
								if (edge_x[i] < -1.1f)
								{
									t = (-1.1f - edge_x[1 - i]) / (edge_x[i] - edge_x[1 - i]);
									bClip = true;
								}
								break;
							case 2:
								if (edge_y[i] > 1.1f)
								{
									t = (1.1f - edge_y[1 - i]) / (edge_y[i] - edge_y[1 - i]);
									bClip = true;
								}
								break;
							case 3:
								if (edge_y[i] < -1.1f)
								{
									t = (-1.1f - edge_y[1 - i]) / (edge_y[i] - edge_y[1 - i]);
									bClip = true;
								}
								break;
							}

							if (bClip)
							{
								float dx = edge_x[i] - edge_x[1 - i];
								float dy = edge_y[i] - edge_y[1 - i];
								edge_x[i] = edge_x[1 - i] + dx * t;
								edge_y[i] = edge_y[1 - i] + dy * t;
							}
						}
					}

					dx = (edge_x[1] - edge_x[0]) / (float)nVerts;
					dy = (edge_y[1] - edge_y[0]) / (float)nVerts;
					float ang2 = atan2f(dy, dx);
					float perp_dx = cosf(ang2 + 1.57f);
					float perp_dy = sinf(ang2 + 1.57f);


					for (i = 0; i < nVerts; i++)
					{
						v[i].x = edge_x[0] - 0.45 + dx * i + perp_dx * 0.35f * fL[i + sample_offset];
						v[i].y = edge_y[0] + dy * i + perp_dy * 0.35f * fL[i + sample_offset];
						//v[i].Diffuse = color;
					}


					////////////////\\\\\\\\\\\\\\\\\\\\\


					float ang3 = 1.57;	// from -PI/2 to PI/2
					float dx3 = cosf(ang3);
					float dy3 = sinf(ang3);

					float edge_x3[2], edge_y3[2];

					edge_x3[0] = fWavePosX * cosf(ang3 + 1.57f) - dx3 * 3.0f;
					edge_y3[0] = fWavePosX * sinf(ang3 + 1.57f) - dy3 * 3.0f;
					edge_x3[1] = fWavePosX * cosf(ang3 + 1.57f) + dx3 * 3.0f;
					edge_y3[1] = fWavePosX * sinf(ang3 + 1.57f) + dy3 * 3.0f;

					for (i = 0; i < 2; i++)	// for each point defining the line
					{
						for (int j = 0; j < 4; j++)
						{
							float t;
							bool bClip = false;

							switch (j)
							{
							case 0:
								if (edge_x3[i] > 1.1f)
								{
									t = (1.1f - edge_x3[1 - i]) / (edge_x3[i] - edge_x3[1 - i]);
									bClip = true;
								}
								break;
							case 1:
								if (edge_x3[i] < -1.1f)
								{
									t = (-1.1f - edge_x3[1 - i]) / (edge_x3[i] - edge_x3[1 - i]);
									bClip = true;
								}
								break;
							case 2:
								if (edge_y3[i] > 1.1f)
								{
									t = (1.1f - edge_y3[1 - i]) / (edge_y3[i] - edge_y3[1 - i]);
									bClip = true;
								}
								break;
							case 3:
								if (edge_y3[i] < -1.1f)
								{
									t = (-1.1f - edge_y3[1 - i]) / (edge_y3[i] - edge_y3[1 - i]);
									bClip = true;
								}
								break;
							}

							if (bClip)
							{
								float dx3 = edge_x3[i] - edge_x3[1 - i];
								float dy3 = edge_y3[i] - edge_y3[1 - i];
								edge_x3[i] = edge_x3[1 - i] + dx3 * t;
								edge_y3[i] = edge_y3[1 - i] + dy3 * t;
							}
						}
					}

					dx3 = (edge_x3[1] - edge_x3[0]) / (float)nVerts;
					dy3 = (edge_y3[1] - edge_y3[0]) / (float)nVerts;
					float ang4 = atan2f(dy3, dx3);
					float perp_dx3 = cosf(ang4 + 1.57f);
					float perp_dy3 = sinf(ang4 + 1.57f);

					for (i = 0; i < nVerts; i++)
					{
						v[i + nVerts].x = edge_x3[0] + 0.45 + dx3 * i + perp_dx3 * (0.35f * fR[i + sample_offset]);
						v[i + nVerts].y = edge_y3[0] + dy3 * i + perp_dy3 * (0.35f * fR[i + sample_offset]);
						//v[i+nVerts].Diffuse = color;
					}

					////////////////\\\\\\\\\\\\\\\\\\\\\


					nBreak = nVerts;
					nVerts *= 2;
				}


				break;


				case 12:
					// x-y osc. that goes around in a spiral, in time, but skewed

					alpha *= 1.25f;
					if (m_pState->m_bModWaveAlphaByVolume)
						alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2]) * 0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime())) / (m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
					if (alpha < 0) alpha = 0;
					if (alpha > 1) alpha = 1;
					//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

					nVerts /= 2;
					for (i = 0; i < nVerts; i++)
					{
						//float rad = 0.13f + 0.43f * fR[i] + fWaveParam2 + perp_dy3/2 ;
						//float ang = fL[i + 32] * 3.57f + GetTime() * perp_dx3/2;
						//v[i].x = rad * cosf(ang+ alpha) * m_fAspectY + fWavePosX ;		// 0.75 = adj. for aspect ratio
						//v[i].y = rad * sinf(ang) * m_fAspectX + fWavePosY ;
						//v[i].Diffuse = color;//(D3DCOLOR_RGBA_01(cr, cg, cb, alpha*min(1, max(0, fL[i])));
						float rad = 0.63f + 0.23f * fR[i] + fWaveParam2;
						float ang = fL[i + 32] * 0.9f + GetTime() * 3.3f;
						if (m_bScreenDependentRenderMode)
						{
							v[i].x = rad * cosf(ang + alpha) + fWavePosX;
							v[i].y = rad * sinf(ang) + fWavePosY;
						}
						else
						{
							v[i].x = rad * cosf(ang + alpha) * m_fAspectY + fWavePosX;		// 0.75 = adj. for aspect ratio
							v[i].y = rad * sinf(ang) * m_fAspectX + fWavePosY;
						}
					}


				break;

				//NEW WAVEFORMS 2023
				case 13: // Star Wave, MilkDrop2077 -----------------------------------------------------------------------------------------------------------

					nVerts /= 2;
					sample_offset = (kNumWaveformSamples - nVerts) / 2;//mysound.GoGoAlignatron(nVerts * 12/10);// only call this once nVerts is final!

					if (m_pState->m_bModWaveAlphaByVolume)
						alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2]) * 0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime())) / (m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
					if (alpha < 0) alpha = 0;
					if (alpha > 1) alpha = 1;
					//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

					{
						float inv_nverts_minus_one = 1.0f / (float)(nVerts - 1);

						for (i = 0; i < nVerts; i++)
						{
							float rad = 0.7f + 0.4f * (fL[i + sample_offset] + fR[i + sample_offset])*0.5f + fWaveParam2;
							float ang = (i)*inv_nverts_minus_one * 6.28f + GetTime() * 0.2f;
							if (i < nVerts / rad)
							{
								float mix = i / (nVerts * 0.1f);
								mix = 0.5f - 0.5f * cosf(mix * 3.1416f);
								float rad_2 = 0.5f + 0.4f * (fL[i + nVerts + sample_offset] + fR[i + nVerts + sample_offset])*0.5f + fWaveParam2;
								rad = rad_2 * (1.0f - mix) + rad * (mix);
							}
							if (m_bScreenDependentRenderMode)
							{
								v[i].x = rad * cosf(ang) + fWavePosX;
								v[i].y = rad * sinf(ang) + fWavePosY;
							}
							else
							{
								v[i].x = rad * cosf(ang) * m_fAspectY + fWavePosX;// 0.75 = adj. for aspect ratio
								v[i].y = rad * sinf(ang) * m_fAspectX + fWavePosY;
							}
							//v[i].Diffuse = color;
						}
					}

					// dupe last vertex to connect the lines; skip if blending
					if (!m_pState->m_bBlending)
					{
						nVerts++;
						memcpy(&v[nVerts - 1], &v[0], sizeof(WFVERTEX));
					}

					break;

				case 14: // Flower Wave, MilkDrop2077 -------------------------------------------------------------------------------------------------------------

					nVerts /= 2;
					sample_offset = (kNumWaveformSamples - nVerts) / 2;//mysound.GoGoAlignatron(nVerts * 12/10);// only call this once nVerts is final!

					if (m_pState->m_bModWaveAlphaByVolume)
						alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2]) * 0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime())) / (m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
					if (alpha < 0) alpha = 0;
					if (alpha > 1) alpha = 1;
					//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

					{
						float inv_nverts_minus_one = 1.0f / (float)(nVerts - 1);

						for (i = 0; i < nVerts; i++)
						{
							float rad = 0.7f + 0.7f * (fL[i + sample_offset] + fR[i + sample_offset])*0.5f + fWaveParam2;
							float ang = (i)*inv_nverts_minus_one * 6.28f + GetTime() * 0.2f;
							ang == ang / 2;
							rad == rad / 2;
							if (i < nVerts / rad)
							{
								float mix = i / (nVerts * 0.1f);
								//mix = 0.7f - 0.7f * cosf(mix * 3.1416f) - sinf((GetTime()/10));
								//mix = 0.7f - 0.7f * cosf(mix * 1.1416f) - sinf((GetTime())/8);
								//mix = 0.7f - 0.7f * cosf(mix*2 * 3.1416f); //flower, more leaves
								mix = 0.7f - 0.7f * cosf(mix * 3.1416f);   //flower

								float rad_2 = 0.7f + 0.7f * (fL[i + nVerts + sample_offset] + fR[i + nVerts + sample_offset])*0.5f + fWaveParam2;
								//rad = rad_2 * (1.0f - mix) + rad * (mix);
									//rad = rad_2 * (1.0f - mix) + rad * (mix) /5; // div5 optional
								rad = rad_2 * (1.0f - mix) + rad * (mix * 2) / 8;
							}
							//v[i].x = rad * (cosf(GetTime()*ang)/3) * m_fAspectY + fWavePosX;// 0.75 = adj. for aspect ratio
							//v[i].y = rad * sinf(GetTime()*ang) * m_fAspectX + fWavePosY;
							//v[i].Diffuse = color;
							//v[i].x = rad * cosf(ang* 2) * m_fAspectY + fWavePosX;//
							if (m_bScreenDependentRenderMode)
							{
								v[i].x = rad * cosf(ang * 3.1416f) / 1.5 + fWavePosX * cosf(3.1416f);
								v[i].y = rad * sinf(ang - GetTime() / 3) / 1.5 + fWavePosY * cosf(3.1416f);
							}
							else
							{
								v[i].x = rad * cosf(ang * 3.1416f) * m_fAspectY / 1.5 + fWavePosX * cosf(3.1416f);// 0.75 = adj. for aspect ratio
								v[i].y = rad * sinf(ang - GetTime() / 3) * m_fAspectX / 1.5 + fWavePosY * cosf(3.1416f);
							}
						}
					}

					// dupe last vertex to connect the lines; skip if blending
					if (!m_pState->m_bBlending)
					{
						nVerts++;
						memcpy(&v[nVerts - 1], &v[0], sizeof(WFVERTEX));
					}

					break;

					/*
					ALTERNATIVE:
					nVerts /= 2;
					sample_offset = (kNumWaveformSamples - nVerts) / 2;//mysound.GoGoAlignatron(nVerts * 12/10);// only call this once nVerts is final!

					if (m_pState->m_bModWaveAlphaByVolume)
					alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2]) * 0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime())) / (m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
					if (alpha < 0) alpha = 0;
					if (alpha > 1) alpha = 1;
					//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

					{
					float inv_nverts_minus_one = 1.0f / (float)(nVerts - 1);

					for (i = 0; i < nVerts; i++)
					{
					float rad = 0.7f + 0.4f * fR[i + sample_offset] + fWaveParam2;
					float ang = (i)* inv_nverts_minus_one * 6.28f + GetTime() * 0.2f;
					if (i < nVerts / rad)
					{
					float mix = i / (nVerts * 0.1f);
					mix = 0.5f - 0.5f * cosf(mix * 3.1416f);
					float rad_2 = 0.5f + 0.4f * fR[i + nVerts + sample_offset] + fWaveParam2/(mix*3);
					rad = rad_2 * (1.0f - mix) + rad * (mix);
					  //rad = rad_2 * (1.0f - mix) + rad * (GetTime() / 3); // BIG
					}
					v[i].x = rad * cosf(ang*2) * m_fAspectY + fWavePosX;// 0.75 = adj. for aspect ratio
					v[i].y = rad * sinf(ang - GetTime() / 3) * m_fAspectX + fWavePosY;
					//v[i].Diffuse = color;
					}
					}

					// dupe last vertex to connect the lines; skip if blending
					if (!m_pState->m_bBlending)
					{
					nVerts++;
					memcpy(&v[nVerts - 1], &v[0], sizeof(WFVERTEX));
					}

					break;*/

				case 15: // Lasso Wave, MilkDrop2077 --------------------------------------------------------------------------------------------------------------

				//m_pState->m_wave[i].Import(NULL, m_waitstring.szText, 0);
				//break;
					alpha *= 1.25f;
					if (m_pState->m_bModWaveAlphaByVolume)
						alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2]) * 0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime())) / (m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
					if (alpha < 0) alpha = 0;
					if (alpha > 1) alpha = 1;
					//color = D3DCOLOR_RGBA_01(cr, cg, cb, alpha);

					nVerts /= 2;

					for (i = 0; i < nVerts; i++)
					{
						//float rad = 0.53f + 0.43f * fR[i] + fWaveParam2;	// unused
						float ang = 0.5f*(fL[i + 32] + fR[i + 32]) * 1.57f + GetTime() * 2.0f;
						// Avoid an infinite tangent when the animated angle crosses zero.
						// A non-finite vertex can poison the whole line strip.
						if (fabsf(ang) < 0.001f)
							ang = (ang < 0.0f) ? -0.001f : 0.001f;
						float t = GetTime() / ang;
						//ball
						//v[i].x = cosf(ang* GetTime())*3 * m_fAspectY/3 + fWavePosX;// 0.75 = adj. for aspect ratio
						//v[i].y = sin(rad* GetTime()) *3 * sinf(ang) * m_fAspectX/3 + fWavePosY;
						//v[i].x = cos(GetTime()) / 2 + cosf(ang * 2 +tanf(t))*m_fAspectY / 2.8 + fWavePosX
						v[i].x = cos(GetTime()) / 2 + cosf(ang * 2 + tanf(t));
						v[i].y = sin(GetTime()) * 2 * sinf(ang * 3.14) * m_fAspectX / 2.8 + fWavePosY;
						//v[i].Diffuse = color;//(D3DCOLOR_RGBA_01(cr, cg, cb, alpha*min(1, max(0, fL[i])));
					}

					break;

				case 16: // DeepSeek + Kai Blaschke (CodAv) - Triangle Waveform
				{
					if (m_pState->m_bModWaveAlphaByVolume)
						alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2]) * 0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime())) / (m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
					if (alpha < 0) alpha = 0;
					if (alpha > 1) alpha = 1;

					// Use half the vertices for this waveform
					nVerts /= 2;

					float inverseSamplesMinusOne = 1.0f / static_cast<float>(nVerts);
					float angleOffset = (GetTime() * .2f);

					for (int i = 0; i < nVerts; i++)
					{
						float progress = static_cast<float>(i) * inverseSamplesMinusOne;
						float phi0 = (floor(progress * 3.0f) + .5f) / 3.0f * 6.28f + angleOffset;
						float angle = (progress * 6.28f) + angleOffset;
						float edgeDistance = cosf(angle - phi0);
						// The triangle's edge distance reaches zero at its corners. A
						// raw division there creates an unbounded vertex and a visible
						// spike. Preserve the shape while applying a small denominator
						// floor around the singularity.
						if (fabsf(edgeDistance) < 0.02f)
							edgeDistance = (edgeDistance < 0.0f) ? -0.02f : 0.02f;
						float radius = (.7f + (edgeDistance * (fL[i]+fR[i])*0.5f) + fWaveParam2) / (2.0f * edgeDistance);
						if (radius > 2.0f) radius = 2.0f;
						if (radius < -2.0f) radius = -2.0f;

						if (m_bScreenDependentRenderMode)
						{
							v[i].x = radius * cosf(angle) + fWavePosX;
							v[i].y = radius * sinf(angle) + fWavePosY;
						}
						else
						{
							v[i].x = radius * cosf(angle) * m_fAspectY + fWavePosX;
							v[i].y = radius * sinf(angle) * m_fAspectX + fWavePosY;
						}
					}

					// Close the loop if not blending
					if (!m_pState->m_bBlending)
					{
						nVerts++;
						memcpy(&v[nVerts - 1], &v[0], sizeof(WFVERTEX));
					}
					break;
				}

				case 17:
					// DeepSeek - Fireworks Waveform
					nVerts = 256; // Use fewer vertices for better performance with particles

					if (m_pState->m_bModWaveAlphaByVolume)
						alpha *= ((mysound.imm_rel[0] + mysound.imm_rel[1] + mysound.imm_rel[2]) * 0.333f - m_pState->m_fModWaveAlphaStart.eval(GetTime())) / (m_pState->m_fModWaveAlphaEnd.eval(GetTime()) - m_pState->m_fModWaveAlphaStart.eval(GetTime()));
					if (alpha < 0) alpha = 0;
					if (alpha > 1) alpha = 1;

					{
						float time = GetTime();
						float burst_frequency = 1.0f - fWaveParam2 + .001; // How often new bursts occur
						float burst_phase = fmodf(time, burst_frequency) / burst_frequency;

						// Random seed based on which burst we're on
						int burst_num = (int)(time / burst_frequency);
						float rand_seed = (burst_num * 10.0f);

						// Base position for this firework
						float base_x = (rand_seed * 0.1345f - floor(rand_seed * 0.1345f)) * 2.0f - 1.0f;
						float base_y = (rand_seed * 0.2783f - floor(rand_seed * 0.2783f)) * 2.0f - 1.0f;

						// Make bursts start from random positions but stay centered more often
						if (fmodf(rand_seed, 1.0f) > 0.3f) {
							base_x *= 0.3f;
							base_y *= 0.3f;
						}

						// Size and fade of current burst
						float burst_size = min(1.0f, burst_phase * 4.0f); // Quick expansion
						float burst_fade = 1.0f - powf(burst_phase, 3.0f); // Slow fade

						// Audio reactivity
						float audio_boost = 1.0f + 2.0f * (mysound.imm_rel[0] + mysound.imm_rel[1]) * 0.5f;

						for (i = 0; i < nVerts; i++)
						{
							// Particle angle
							float ang = (i / (float)nVerts) * 6.283185f;

							// Particle distance from center (with some randomness)
							float dist_var = 0.7f + 0.3f * (fmodf(rand_seed + i * 0.1f, 1.0f));
							float dist = burst_size * dist_var * (0.5f + 0.5f * (fL[(i * 3) % kNumWaveformSamples] + fR[(i * 3) % kNumWaveformSamples])*0.5f) * audio_boost;

							// Calculate position
							float x = base_x + cosf(ang) * dist;
							float y = base_y + sinf(ang) * dist;

							// Add some secondary motion
							float swirl = time * 3.0f + ang;
							x += cosf(swirl) * burst_size * 0.1f;
							y += sinf(swirl) * burst_size * 0.1f;

							// Apply aspect ratio
							if (m_bScreenDependentRenderMode)
							{
								v[i].x = x + fWavePosX;
								v[i].y = y + fWavePosY;
							}
							else
							{
								v[i].x = x * m_fAspectY + fWavePosX;
								v[i].y = y * m_fAspectX + fWavePosY;
							}

							// Fade particles as burst ages
							alpha *= burst_fade;
						}
					}

					// For point rendering mode (recommended for this waveform)
					if (!m_pState->m_bBlending)
					{
						nVerts++;
						memcpy(&v[nVerts - 1], &v[0], sizeof(WFVERTEX));
					}
					break;
		}

		if (it==0)
		{
			nVerts1 = nVerts;
			nBreak1 = nBreak;
			alpha1  = alpha;
		}
		else
		{
			nVerts2 = nVerts;
			nBreak2 = nBreak;
			alpha2  = alpha;
		}
	}

	float mix = CosineInterpolate(m_pState->m_fBlendProgress);
	float mix2 = 1.0f - mix;

	// blend 2 waveforms
	if (nVerts2 > 0)
	{
		// note: this won't yet handle the case where (nBreak1 > 0 && nBreak2 > 0)
		//       in this case, code must break wave into THREE segments
		float m = (nVerts2-1)/(float)nVerts1;
		float x,y;
		for (int i=0; i<nVerts1; i++)
		{
			float fIdx = i*m;
			int   nIdx = (int)fIdx;
			float t = fIdx - nIdx;
			if (nIdx == nBreak2-1)
			{
				x = v2[nIdx].x;
				y = v2[nIdx].y;
				nBreak1 = i+1;
			}
			else
			{
				x = v2[nIdx].x*(1-t) + v2[nIdx+1].x*(t);
				y = v2[nIdx].y*(1-t) + v2[nIdx+1].y*(t);
			}
			v1[i].x = v1[i].x*(mix) + x*(mix2);
			v1[i].y = v1[i].y*(mix) + y*(mix2);
		}
	}

	// determine alpha
	if (nVerts2 > 0)
	{
		alpha1 = alpha1*(mix) + alpha2*(1.0f-mix);
	}

	// apply color & alpha
    // ALSO reverse all y values, to stay consistent with the pre-VMS milkdrop,
    //  which DIDN'T:
	v1[0].Diffuse = PackColorRgba01(cr, cg, cb, alpha1);
	for (i=0; i<nVerts1; i++)
    {
		v1[i].Diffuse = v1[0].Diffuse;
        v1[i].y = -v1[i].y;
    }

    // don't draw wave if (possibly blended) alpha is less than zero.
    if (alpha1 < 0.004f)
        return;

    // TESSELLATE - smooth the wave, one time.
    WFVERTEX* pVerts = v1;
    WFVERTEX vTess[(576+3)*2];
    if (1)
    {
        if (nBreak1==-1)
        {
            nVerts1 = SmoothWave(v1, nVerts1, vTess);
        }
        else
        {
            int oldBreak = nBreak1;
            nBreak1 = SmoothWave(v1, nBreak1, vTess);
            nVerts1 = SmoothWave(&v1[oldBreak], nVerts1-oldBreak, &vTess[nBreak1]) + nBreak1;
        }
        pVerts = vTess;
    }

	// draw primitives
	{
		float x_inc = 2.0f / (float)m_nTexSizeX;
		float y_inc = 2.0f / (float)m_nTexSizeY;
		int drawing_its = ((*m_pState->var_pf_wave_thick || *m_pState->var_pf_wave_usedots) && (m_nTexSizeX >= 512)) ? 4 : 1;

		program.Bind();

		for (int it=0; it<drawing_its; it++)
		{
			int j;

			switch(it)
			{
			case 0: break;
			case 1: for (j=0; j<nVerts1; j++) pVerts[j].x += x_inc; break;		// draw fat dots
			case 2: for (j=0; j<nVerts1; j++) pVerts[j].y += y_inc; break;		// draw fat dots
			case 3: for (j=0; j<nVerts1; j++) pVerts[j].x -= x_inc; break;		// draw fat dots
			}

			mesh.UploadVertices(pVerts, nVerts1);

			if (nBreak1 == -1)
			{
                if (*m_pState->var_pf_wave_usedots)
                    mesh.DrawPointList(0, nVerts1);
                else
                    mesh.DrawLineStrip(0, nVerts1);
			}
			else
			{
                if (*m_pState->var_pf_wave_usedots)
                {
                    mesh.DrawPointList(0, nBreak1);
                    mesh.DrawPointList(nBreak1, nVerts1-nBreak1);
                }
                else
                {
                    mesh.DrawLineStrip(0, nBreak1);
                    mesh.DrawLineStrip(nBreak1, nVerts1-nBreak1);
                }
			}
		}
	}
}

}
