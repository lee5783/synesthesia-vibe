//
//  custom_waves.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "custom_waves.h"
#include "blend_state.h"
#include "equation_drivers.h"
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

constexpr int NUM_WAVEFORM_SAMPLES = 480;

using std::max;
using std::min;

// does a better-than-linear smooth on a wave.  Roughly doubles the # of points.
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

void DrawCustomWaves(const CustomWavesInput& input, const DrawProgram& program, MeshBuffer& mesh)
{
    CState* const m_pState = input.state;
    CState* const m_pOldState = input.oldState;
    const td_soundinfo& m_sound = *input.sound;
    const bool m_bScreenDependentRenderMode = input.screenDependentRenderMode;
    const float m_fInvAspectX = input.invAspectX;
    const float m_fInvAspectY = input.invAspectY;
    const int m_nTexSizeX = input.texSizeX;
    const int m_nTexSizeY = input.texSizeY;

    program.Bind();

    // note: read in all sound data from CPluginShell's m_sound
	int num_reps = (m_pState->m_bBlending) ? 2 : 1;
	for (int rep=0; rep<num_reps; rep++)
	{
        CState *pState = (rep==0) ? m_pState : m_pOldState;
        float alpha_mult = 1;
        if (num_reps==2)
            alpha_mult = (rep==0) ? m_pState->m_fBlendProgress : (1-m_pState->m_fBlendProgress);

        for (int i=0; i<MAX_CUSTOM_WAVES; i++)
        {
            if (pState->m_wave[i].enabled)
            {
                // 1. execute per-frame code
                ::LoadCustomWavePerFrameEvallibVars(pState, i, *input.frameInputs);

			    // 2.a. do just a once-per-frame init for the *per-point* *READ-ONLY* variables
			    //     (the non-read-only ones will be reset/restored at the start of each vertex)
			    *pState->m_wave[i].var_pp_time		= *pState->m_wave[i].var_pf_time;
			    *pState->m_wave[i].var_pp_fps       = *pState->m_wave[i].var_pf_fps;
			    *pState->m_wave[i].var_pp_frame		= *pState->m_wave[i].var_pf_frame;
		        *pState->m_wave[i].var_pp_progress  = *pState->m_wave[i].var_pf_progress;
			    *pState->m_wave[i].var_pp_bass		= *pState->m_wave[i].var_pf_bass;
			    *pState->m_wave[i].var_pp_mid		= *pState->m_wave[i].var_pf_mid;
			    *pState->m_wave[i].var_pp_treb		= *pState->m_wave[i].var_pf_treb;
			    *pState->m_wave[i].var_pp_bass_att	= *pState->m_wave[i].var_pf_bass_att;
			    *pState->m_wave[i].var_pp_mid_att	= *pState->m_wave[i].var_pf_mid_att;
			    *pState->m_wave[i].var_pp_treb_att	= *pState->m_wave[i].var_pf_treb_att;

				NSEEL_code_execute(pState->m_wave[i].m_pf_codehandle);

                for (int vi=0; vi<NUM_Q_VAR; vi++)
                    *pState->m_wave[i].var_pp_q[vi] = *pState->m_wave[i].var_pf_q[vi];
                for (int vi=0; vi<NUM_T_VAR; vi++)
                    *pState->m_wave[i].var_pp_t[vi] = *pState->m_wave[i].var_pf_t[vi];

                // A custom wave can generate up to 512 vertices, but waveform
                // input has only NUM_WAVEFORM_SAMPLES valid samples (480 in
                // this build).  Keep those two limits separate: larger custom
                // waves resample the input instead of reading past it.
                const int maxOutputSamples = 512;
                const int maxInputSamples = pState->m_wave[i].bSpectrum ? 512 : NUM_WAVEFORM_SAMPLES;
                int nSamples = min(maxOutputSamples, max(0, (int)*pState->m_wave[i].var_pf_samples));
                const int separator = min(nSamples - 1, max(0, pState->m_wave[i].sep));

                if ((nSamples >= 2) || (pState->m_wave[i].bUseDots && nSamples >= 1))
                {
                    int j;
                    float tempdata[2][512];
                    float mult = ((pState->m_wave[i].bSpectrum) ? 0.15f : 0.004f) * pState->m_wave[i].scaling * pState->m_fWaveScale.eval(-1);
                    const float *pdata1 = (pState->m_wave[i].bSpectrum) ? m_sound.fSpectrum[0] : m_sound.fWaveform[0];
                    const float *pdata2 = (pState->m_wave[i].bSpectrum) ? m_sound.fSpectrum[1] : m_sound.fWaveform[1];

                    // "samples" controls output vertices.  In spectrum mode,
                    // "sep" trims high-frequency input samples.  Oscilloscope
                    // waves ignore sep and resample only when they request more
                    // vertices than the 480-sample input can provide.
                    float sampleScale = 1.0f;
                    if (pState->m_wave[i].bSpectrum)
                        sampleScale = (float)(maxInputSamples - separator) / (float)nSamples;
                    else if (nSamples > maxInputSamples)
                        sampleScale = (float)maxInputSamples / (float)nSamples;

                    float mix1 = powf(pState->m_wave[i].smoothing*0.98f, 0.5f);  // lower exponent -> more default smoothing
                    float mix2 = 1-mix1;
                    // SMOOTHING:
                    tempdata[0][0] = pdata1[0];
                    tempdata[1][0] = pdata2[0];
                    for (j=1; j<nSamples; j++)
                    {
                        const int pcmSample = (int)(j * sampleScale);
                        tempdata[0][j] = pdata1[pcmSample]*mix2 + tempdata[0][j-1]*mix1;
                        tempdata[1][j] = pdata2[pcmSample]*mix2 + tempdata[1][j-1]*mix1;
                    }
                    // smooth again, backwards: [this fixes the asymmetry of the beginning & end..]
                    for (j=nSamples-2; j>=0; j--)
                    {
                        tempdata[0][j] = tempdata[0][j]*mix2 + tempdata[0][j+1]*mix1;
                        tempdata[1][j] = tempdata[1][j]*mix2 + tempdata[1][j+1]*mix1;
                    }
                    // finally, scale to final size:
                    for (j=0; j<nSamples; j++)
                    {
                        tempdata[0][j] *= mult;
                        tempdata[1][j] *= mult;
                    }

                    // 2. for each point, execute per-point code

                    WFVERTEX v[1024];
                    memset(v, 0, sizeof(v));
                    float j_mult = 1.0f/(float)(nSamples-1);
                    for (j=0; j<nSamples; j++)
                    {
                        float t = j*j_mult;
                        float value1 = tempdata[0][j];
                        float value2 = tempdata[1][j];
                        *pState->m_wave[i].var_pp_sample = t;
                        *pState->m_wave[i].var_pp_value1 = value1;
                        *pState->m_wave[i].var_pp_value2 = value2;
                        *pState->m_wave[i].var_pp_x      = 0.5f + value1;
                        *pState->m_wave[i].var_pp_y      = 0.5f + value2;
                        *pState->m_wave[i].var_pp_r      = *pState->m_wave[i].var_pf_r;
                        *pState->m_wave[i].var_pp_g      = *pState->m_wave[i].var_pf_g;
                        *pState->m_wave[i].var_pp_b      = *pState->m_wave[i].var_pf_b;
                        *pState->m_wave[i].var_pp_a      = *pState->m_wave[i].var_pf_a;

                        #ifndef _NO_EXPR_
                            NSEEL_code_execute(pState->m_wave[i].m_pp_codehandle);
                        #endif

						if (m_bScreenDependentRenderMode)
						{
							v[j].x = (float)(*pState->m_wave[i].var_pp_x*2-1);
							v[j].y = (float)(*pState->m_wave[i].var_pp_y*-2+1);
						}
						else
						{
							v[j].x = (float)(*pState->m_wave[i].var_pp_x*2-1)*m_fInvAspectX;
							v[j].y = (float)(*pState->m_wave[i].var_pp_y*-2+1)*m_fInvAspectY;
						}
                        v[j].z = 0;
                        v[j].Diffuse =
                            ((((int)(*pState->m_wave[i].var_pp_a * 255 * alpha_mult)) & 0xFF) << 24) |
                            ((((int)(*pState->m_wave[i].var_pp_r * 255)) & 0xFF) << 16) |
                            ((((int)(*pState->m_wave[i].var_pp_g * 255)) & 0xFF) <<  8) |
                            ((((int)(*pState->m_wave[i].var_pp_b * 255)) & 0xFF)      );
                    }

                    // 3. smooth it
                    WFVERTEX v2[2048];
                    memset(v2, 0, sizeof(v2));
                    WFVERTEX *pVerts = v;
                    if (!pState->m_wave[i].bUseDots)
                    {
                        nSamples = SmoothWave(v, nSamples, v2);
                        pVerts = v2;
                    }

                    // 4. draw it
                    SetBlendState(true, BlendFactor::SrcAlpha, pState->m_wave[i].bAdditive ? BlendFactor::One : BlendFactor::InvSrcAlpha);

                    float ptsize = (float)((m_nTexSizeX >= 1024) ? 2 : 1) + (pState->m_wave[i].bDrawThick ? 1 : 0);
                    if (pState->m_wave[i].bUseDots)
                        SetPointSize(ptsize);

                    int its = (pState->m_wave[i].bDrawThick && !pState->m_wave[i].bUseDots) ? 4 : 1;
		            float x_inc = 2.0f / (float)m_nTexSizeX;
		            float y_inc = 2.0f / (float)m_nTexSizeY;
                    for (int it=0; it<its; it++)
                    {
			            switch(it)
			            {
			            case 0: break;
			            case 1: for (j=0; j<nSamples; j++) pVerts[j].x += x_inc; break;		// draw fat dots
			            case 2: for (j=0; j<nSamples; j++) pVerts[j].y += y_inc; break;		// draw fat dots
			            case 3: for (j=0; j<nSamples; j++) pVerts[j].x -= x_inc; break;		// draw fat dots
			            }
                        mesh.UploadVertices(pVerts, nSamples);
                        if (pState->m_wave[i].bUseDots)
                            mesh.DrawPointList(0, nSamples);
                        else
                            mesh.DrawLineStrip(0, nSamples);
                    }

                    ptsize = 1.0f;
                    if (pState->m_wave[i].bUseDots)
                        SetPointSize(ptsize);
                }
            }
        }
    }

	SetBlendOpaque();
}

}
