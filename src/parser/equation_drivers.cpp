//
//  equation_drivers.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-29.
//

#include "equation_drivers.h"
#include "math_macros.h"
#include <algorithm>

void LoadPerFrameEvallibVars(CState* pState, const FrameInputs& in)
{
    // load the 'var_pf_*' variables in this CState object with the correct values.
    // for vars that affect pixel motion, that means evaluating them at time==-1,
    //    (i.e. no blending w/blendto value); the blending of the file dx/dy
    //    will be done *after* execution of the per-vertex code.
    // for vars that do NOT affect pixel motion, evaluate them at the current time,
    //    so that if they're blending, both states see the blended value.

    // 1. vars that affect pixel motion: (eval at time==-1)
	*pState->var_pf_zoom		= (double)pState->m_fZoom.eval(-1);//GetTime());
	*pState->var_pf_zoomexp		= (double)pState->m_fZoomExponent.eval(-1);//GetTime());
	*pState->var_pf_rot			= (double)pState->m_fRot.eval(-1);//GetTime());
	*pState->var_pf_warp		= (double)pState->m_fWarpAmount.eval(-1);//GetTime());
	*pState->var_pf_cx			= (double)pState->m_fRotCX.eval(-1);//GetTime());
	*pState->var_pf_cy			= (double)pState->m_fRotCY.eval(-1);//GetTime());
	*pState->var_pf_dx			= (double)pState->m_fXPush.eval(-1);//GetTime());
	*pState->var_pf_dy			= (double)pState->m_fYPush.eval(-1);//GetTime());
	*pState->var_pf_sx			= (double)pState->m_fStretchX.eval(-1);//GetTime());
	*pState->var_pf_sy			= (double)pState->m_fStretchY.eval(-1);//GetTime());
	// read-only:
	*pState->var_pf_time		= (double)(in.time - in.startTime);
	*pState->var_pf_fps         = (double)in.fps;
	*pState->var_pf_bass		= (double)in.mysound.imm_rel[0];
	*pState->var_pf_mid			= (double)in.mysound.imm_rel[1];
	*pState->var_pf_treb		= (double)in.mysound.imm_rel[2];
	*pState->var_pf_bass_att	= (double)in.mysound.avg_rel[0];
	*pState->var_pf_mid_att		= (double)in.mysound.avg_rel[1];
	*pState->var_pf_treb_att	= (double)in.mysound.avg_rel[2];
	*pState->var_pf_frame		= (double)in.frame;
	//*pState->var_pf_monitor     = 0;   -leave this as it was set in the per-frame INIT code!
    for (int vi=0; vi<NUM_Q_VAR; vi++)
	    *pState->var_pf_q[vi]	= pState->q_values_after_init_code[vi];//0.0f;
    *pState->var_pf_monitor     = pState->monitor_after_init_code;
	*pState->var_pf_progress    = (in.time - in.presetStartTime) / (in.nextPresetTime - in.presetStartTime);

	// 2. vars that do NOT affect pixel motion: (eval at time==now)
	*pState->var_pf_decay		= (double)pState->m_fDecay.eval(in.time);
	*pState->var_pf_wave_a		= (double)pState->m_fWaveAlpha.eval(in.time);
	*pState->var_pf_wave_r		= (double)pState->m_fWaveR.eval(in.time);
	*pState->var_pf_wave_g		= (double)pState->m_fWaveG.eval(in.time);
	*pState->var_pf_wave_b		= (double)pState->m_fWaveB.eval(in.time);
	*pState->var_pf_wave_x		= (double)pState->m_fWaveX.eval(in.time);
	*pState->var_pf_wave_y		= (double)pState->m_fWaveY.eval(in.time);
	*pState->var_pf_wave_mystery= (double)pState->m_fWaveParam.eval(in.time);
	*pState->var_pf_wave_mode   = (double)pState->m_nWaveMode;	//?!?! -why won't it work if set to pState->m_nWaveMode???
	*pState->var_pf_ob_size		= (double)pState->m_fOuterBorderSize.eval(in.time);
	*pState->var_pf_ob_r		= (double)pState->m_fOuterBorderR.eval(in.time);
	*pState->var_pf_ob_g		= (double)pState->m_fOuterBorderG.eval(in.time);
	*pState->var_pf_ob_b		= (double)pState->m_fOuterBorderB.eval(in.time);
	*pState->var_pf_ob_a		= (double)pState->m_fOuterBorderA.eval(in.time);
	*pState->var_pf_ib_size		= (double)pState->m_fInnerBorderSize.eval(in.time);
	*pState->var_pf_ib_r		= (double)pState->m_fInnerBorderR.eval(in.time);
	*pState->var_pf_ib_g		= (double)pState->m_fInnerBorderG.eval(in.time);
	*pState->var_pf_ib_b		= (double)pState->m_fInnerBorderB.eval(in.time);
	*pState->var_pf_ib_a		= (double)pState->m_fInnerBorderA.eval(in.time);
	*pState->var_pf_mv_x        = (double)pState->m_fMvX.eval(in.time);
	*pState->var_pf_mv_y        = (double)pState->m_fMvY.eval(in.time);
	*pState->var_pf_mv_dx       = (double)pState->m_fMvDX.eval(in.time);
	*pState->var_pf_mv_dy       = (double)pState->m_fMvDY.eval(in.time);
	*pState->var_pf_mv_l        = (double)pState->m_fMvL.eval(in.time);
	*pState->var_pf_mv_r        = (double)pState->m_fMvR.eval(in.time);
	*pState->var_pf_mv_g        = (double)pState->m_fMvG.eval(in.time);
	*pState->var_pf_mv_b        = (double)pState->m_fMvB.eval(in.time);
	*pState->var_pf_mv_a        = (double)pState->m_fMvA.eval(in.time);
	*pState->var_pf_echo_zoom   = (double)pState->m_fVideoEchoZoom.eval(in.time);
	*pState->var_pf_echo_alpha  = (double)pState->m_fVideoEchoAlpha.eval(in.time);
	*pState->var_pf_echo_orient = (double)pState->m_nVideoEchoOrientation;
    // new in v1.04:
    *pState->var_pf_wave_usedots  = (double)pState->m_bWaveDots;
    *pState->var_pf_wave_thick    = (double)pState->m_bWaveThick;
    *pState->var_pf_wave_additive = (double)pState->m_bAdditiveWaves;
    *pState->var_pf_wave_brighten = (double)pState->m_bMaximizeWaveColor;
    *pState->var_pf_darken_center = (double)pState->m_bDarkenCenter;
    *pState->var_pf_gamma         = (double)pState->m_fGammaAdj.eval(in.time);
    *pState->var_pf_wrap          = (double)pState->m_bTexWrap;
    *pState->var_pf_invert        = (double)pState->m_bInvert;
    *pState->var_pf_brighten      = (double)pState->m_bBrighten;
    *pState->var_pf_darken        = (double)pState->m_bDarken;
    *pState->var_pf_solarize      = (double)pState->m_bSolarize;
    *pState->var_pf_meshx         = (double)in.gridX;
    *pState->var_pf_meshy         = (double)in.gridY;
    *pState->var_pf_pixelsx       = (double)in.pixelsX;
    *pState->var_pf_pixelsy       = (double)in.pixelsY;
	if (in.bScreenDependentRenderMode)
	{
		*pState->var_pf_aspectx = 1;
		*pState->var_pf_aspecty = 1;

	}
	else
	{
		*pState->var_pf_aspectx = (double)in.invAspectX;
		*pState->var_pf_aspecty = (double)in.invAspectY;
	}

    // new in v2.0:
    *pState->var_pf_blur1min      = (double)pState->m_fBlur1Min.eval(in.time);
    *pState->var_pf_blur2min      = (double)pState->m_fBlur2Min.eval(in.time);
    *pState->var_pf_blur3min      = (double)pState->m_fBlur3Min.eval(in.time);
    *pState->var_pf_blur1max      = (double)pState->m_fBlur1Max.eval(in.time);
    *pState->var_pf_blur2max      = (double)pState->m_fBlur2Max.eval(in.time);
    *pState->var_pf_blur3max      = (double)pState->m_fBlur3Max.eval(in.time);
    *pState->var_pf_blur1_edge_darken = (double)pState->m_fBlur1EdgeDarken.eval(in.time);
	// new in BeatDrop v1.4.1:
	*pState->var_pf_mousex        = (double)in.mouseX;
	*pState->var_pf_mousey        = (double)in.mouseY;
	*pState->var_pf_mousedown     = in.mouseDown ? 1.0 : 0.0;
	*pState->var_pf_mouseclick    = in.mouseClicked > 0 ? 1.0 : 0.0;
	*pState->var_pf_ctrllt        = in.ctrlLeft ? 1.0 : 0.0;
	*pState->var_pf_ctrlrt        = in.ctrlRight ? 1.0 : 0.0;
	*pState->var_pf_ctrldn        = in.ctrlDown ? 1.0 : 0.0;
	*pState->var_pf_ctrlup        = in.ctrlUp ? 1.0 : 0.0;
	// new in BeatDrop v1.5:
	*pState->var_pf_year          = in.year;
	*pState->var_pf_month         = in.month;
	*pState->var_pf_day           = in.day;
	*pState->var_pf_weekday       = in.weekday;
	*pState->var_pf_hour          = in.hour;
	*pState->var_pf_minute        = in.minute;
	*pState->var_pf_second        = in.second;
	*pState->var_pf_milliseconds  = in.milliseconds;
	*pState->var_pf_totalseconds  = in.totalSeconds;
}

void RunPerFrameEquations(CState* curState, CState* oldState, int code, const FrameInputs& in, float& fSnapPoint)
{
	// run per-frame calculations

    /*
      code is only valid when blending.
          OLDcomp ~ blend-from preset has a composite shader;
          NEWwarp ~ blend-to preset has a warp shader; etc.

      code OLDcomp NEWcomp OLDwarp NEWwarp
        0
        1            1
        2                            1
        3            1               1
        4     1
        5     1      1
        6     1                      1
        7     1      1               1
        8                    1
        9            1       1
        10                   1       1
        11           1       1       1
        12    1              1
        13    1      1       1
        14    1              1       1
        15    1      1       1       1
    */

    // when blending booleans (like darken, invert, etc) for pre-shader presets,
    // if blending to/from a pixel-shader preset, we can tune the snap point
    // (when it changes during the blend) for a less jumpy transition:
    fSnapPoint = 0.5f;
    if (curState->m_bBlending)
    {
        switch(code)
        {
        case 4:
        case 6:
        case 12:
        case 14:
            // old preset (only) had a comp shader
            fSnapPoint = -0.01f;
            break;
        case 1:
        case 3:
        case 9:
        case 11:
            // new preset (only) has a comp shader
            fSnapPoint = 1.01f;
            break;
        case 0:
        case 2:
        case 8:
        case 10:
            // neither old or new preset had a comp shader
            fSnapPoint = 0.5f;
            break;
        case 5:
        case 7:
        case 13:
        case 15:
            // both old and new presets use a comp shader - so it won't matter
            fSnapPoint = 0.5f;
            break;
        }
    }

	int num_reps = (curState->m_bBlending) ? 2 : 1;
	for (int rep=0; rep<num_reps; rep++)
	{
		CState *pState;

		if (rep==0)
			pState = curState;
		else
			pState = oldState;

		// values that will affect the pixel motion (and will be automatically blended
		//	LATER, when the results of 2 sets of these params creates 2 different U/V
		//  meshes that get blended together.)
        LoadPerFrameEvallibVars(pState, in);

		// also do just a once-per-frame init for the *per-**VERTEX*** *READ-ONLY* variables
		// (the non-read-only ones will be reset/restored at the start of each vertex)
		*pState->var_pv_time		= *pState->var_pf_time;
		*pState->var_pv_fps         = *pState->var_pf_fps;
		*pState->var_pv_frame		= *pState->var_pf_frame;
		*pState->var_pv_progress    = *pState->var_pf_progress;
		*pState->var_pv_bass		= *pState->var_pf_bass;
		*pState->var_pv_mid			= *pState->var_pf_mid;
		*pState->var_pv_treb		= *pState->var_pf_treb;
		*pState->var_pv_bass_att	= *pState->var_pf_bass_att;
		*pState->var_pv_mid_att		= *pState->var_pf_mid_att;
		*pState->var_pv_treb_att	= *pState->var_pf_treb_att;
		*pState->var_pv_mousex      = *pState->var_pf_mousex;
		*pState->var_pv_mousey      = *pState->var_pf_mousey;
		*pState->var_pv_mousedown   = *pState->var_pf_mousedown;
		*pState->var_pv_mouseclick  = *pState->var_pf_mouseclick;
		*pState->var_pv_ctrllt      = *pState->var_pf_ctrllt;
		*pState->var_pv_ctrlrt      = *pState->var_pf_ctrlrt;
		*pState->var_pv_ctrldn      = *pState->var_pf_ctrldn;
		*pState->var_pv_ctrlup      = *pState->var_pf_ctrlup;
        *pState->var_pv_meshx       = (double)in.gridX;
        *pState->var_pv_meshy       = (double)in.gridY;
        *pState->var_pv_pixelsx     = (double)in.pixelsX;
        *pState->var_pv_pixelsy     = (double)in.pixelsY;
		if (in.bScreenDependentRenderMode)
		{
			*pState->var_pv_aspectx = 1;
			*pState->var_pv_aspecty = 1;
		}
		else
		{
			*pState->var_pv_aspectx = (double)in.invAspectX;
			*pState->var_pv_aspecty = (double)in.invAspectY;
		}
        //*pState->var_pv_monitor     = *pState->var_pf_monitor;

		// execute once-per-frame expressions:
#ifndef _NO_EXPR_
		if (pState->m_pf_codehandle)
		{
			if (pState->m_pf_codehandle)
			{
				NSEEL_code_execute(pState->m_pf_codehandle);
			}
		}
#endif

        // save some things for next frame:
        pState->monitor_after_init_code = *pState->var_pf_monitor;

        // save some things for per-vertex code:
        for (int vi=0; vi<NUM_Q_VAR; vi++)
            *pState->var_pv_q[vi] = *pState->var_pf_q[vi];

        // (a few range checks:)
        *pState->var_pf_gamma     = std::max(0.0  , std::min(    8.0, *pState->var_pf_gamma    ));
        *pState->var_pf_echo_zoom = std::max(0.001, std::min( 1000.0, *pState->var_pf_echo_zoom));

		/*
        if (m_pState->m_bRedBlueStereo || m_bAlways3D)
		{
			// override wave colors
			*pState->var_pf_wave_r = 0.35f*(*pState->var_pf_wave_r) + 0.65f;
			*pState->var_pf_wave_g = 0.35f*(*pState->var_pf_wave_g) + 0.65f;
			*pState->var_pf_wave_b = 0.35f*(*pState->var_pf_wave_b) + 0.65f;
		}
        */
	}

	if (curState->m_bBlending)
	{
		// For all variables that do NOT affect pixel motion, blend them NOW,
        // so later the user can just access m_pState->m_pf_whatever.
		double mix  = (double)CosineInterp(curState->m_fBlendProgress);
		double mix2 = 1.0 - mix;
        *curState->var_pf_decay        = mix*(*curState->var_pf_decay       ) + mix2*(*oldState->var_pf_decay       );
        *curState->var_pf_wave_a       = mix*(*curState->var_pf_wave_a      ) + mix2*(*oldState->var_pf_wave_a      );
        *curState->var_pf_wave_r       = mix*(*curState->var_pf_wave_r      ) + mix2*(*oldState->var_pf_wave_r      );
        *curState->var_pf_wave_g       = mix*(*curState->var_pf_wave_g      ) + mix2*(*oldState->var_pf_wave_g      );
        *curState->var_pf_wave_b       = mix*(*curState->var_pf_wave_b      ) + mix2*(*oldState->var_pf_wave_b      );
        *curState->var_pf_wave_x       = mix*(*curState->var_pf_wave_x      ) + mix2*(*oldState->var_pf_wave_x      );
        *curState->var_pf_wave_y       = mix*(*curState->var_pf_wave_y      ) + mix2*(*oldState->var_pf_wave_y      );
        *curState->var_pf_wave_mystery = mix*(*curState->var_pf_wave_mystery) + mix2*(*oldState->var_pf_wave_mystery);
		// wave_mode: exempt (integer)
        *curState->var_pf_ob_size      = mix*(*curState->var_pf_ob_size     ) + mix2*(*oldState->var_pf_ob_size     );
        *curState->var_pf_ob_r         = mix*(*curState->var_pf_ob_r        ) + mix2*(*oldState->var_pf_ob_r        );
        *curState->var_pf_ob_g         = mix*(*curState->var_pf_ob_g        ) + mix2*(*oldState->var_pf_ob_g        );
        *curState->var_pf_ob_b         = mix*(*curState->var_pf_ob_b        ) + mix2*(*oldState->var_pf_ob_b        );
        *curState->var_pf_ob_a         = mix*(*curState->var_pf_ob_a        ) + mix2*(*oldState->var_pf_ob_a        );
        *curState->var_pf_ib_size      = mix*(*curState->var_pf_ib_size     ) + mix2*(*oldState->var_pf_ib_size     );
        *curState->var_pf_ib_r         = mix*(*curState->var_pf_ib_r        ) + mix2*(*oldState->var_pf_ib_r        );
        *curState->var_pf_ib_g         = mix*(*curState->var_pf_ib_g        ) + mix2*(*oldState->var_pf_ib_g        );
        *curState->var_pf_ib_b         = mix*(*curState->var_pf_ib_b        ) + mix2*(*oldState->var_pf_ib_b        );
        *curState->var_pf_ib_a         = mix*(*curState->var_pf_ib_a        ) + mix2*(*oldState->var_pf_ib_a        );
        *curState->var_pf_mv_x         = mix*(*curState->var_pf_mv_x        ) + mix2*(*oldState->var_pf_mv_x        );
        *curState->var_pf_mv_y         = mix*(*curState->var_pf_mv_y        ) + mix2*(*oldState->var_pf_mv_y        );
        *curState->var_pf_mv_dx        = mix*(*curState->var_pf_mv_dx       ) + mix2*(*oldState->var_pf_mv_dx       );
        *curState->var_pf_mv_dy        = mix*(*curState->var_pf_mv_dy       ) + mix2*(*oldState->var_pf_mv_dy       );
        *curState->var_pf_mv_l         = mix*(*curState->var_pf_mv_l        ) + mix2*(*oldState->var_pf_mv_l        );
        *curState->var_pf_mv_r         = mix*(*curState->var_pf_mv_r        ) + mix2*(*oldState->var_pf_mv_r        );
        *curState->var_pf_mv_g         = mix*(*curState->var_pf_mv_g        ) + mix2*(*oldState->var_pf_mv_g        );
        *curState->var_pf_mv_b         = mix*(*curState->var_pf_mv_b        ) + mix2*(*oldState->var_pf_mv_b        );
        *curState->var_pf_mv_a         = mix*(*curState->var_pf_mv_a        ) + mix2*(*oldState->var_pf_mv_a        );
		*curState->var_pf_echo_zoom    = mix*(*curState->var_pf_echo_zoom   ) + mix2*(*oldState->var_pf_echo_zoom   );
		*curState->var_pf_echo_alpha   = mix*(*curState->var_pf_echo_alpha  ) + mix2*(*oldState->var_pf_echo_alpha  );
        *curState->var_pf_echo_orient  = (mix < fSnapPoint) ? *oldState->var_pf_echo_orient : *curState->var_pf_echo_orient;
        // added in v1.04:
        *curState->var_pf_wave_usedots = (mix < fSnapPoint) ? *oldState->var_pf_wave_usedots  : *curState->var_pf_wave_usedots ;
        *curState->var_pf_wave_thick   = (mix < fSnapPoint) ? *oldState->var_pf_wave_thick    : *curState->var_pf_wave_thick   ;
        *curState->var_pf_wave_additive= (mix < fSnapPoint) ? *oldState->var_pf_wave_additive : *curState->var_pf_wave_additive;
        *curState->var_pf_wave_brighten= (mix < fSnapPoint) ? *oldState->var_pf_wave_brighten : *curState->var_pf_wave_brighten;
        *curState->var_pf_darken_center= (mix < fSnapPoint) ? *oldState->var_pf_darken_center : *curState->var_pf_darken_center;
        *curState->var_pf_gamma        = mix*(*curState->var_pf_gamma       ) + mix2*(*oldState->var_pf_gamma       );
        *curState->var_pf_wrap         = (mix < fSnapPoint) ? *oldState->var_pf_wrap          : *curState->var_pf_wrap         ;
        *curState->var_pf_invert       = (mix < fSnapPoint) ? *oldState->var_pf_invert        : *curState->var_pf_invert       ;
        *curState->var_pf_brighten     = (mix < fSnapPoint) ? *oldState->var_pf_brighten      : *curState->var_pf_brighten     ;
        *curState->var_pf_darken       = (mix < fSnapPoint) ? *oldState->var_pf_darken        : *curState->var_pf_darken       ;
        *curState->var_pf_solarize     = (mix < fSnapPoint) ? *oldState->var_pf_solarize      : *curState->var_pf_solarize     ;
        // added in v2.0:
        *curState->var_pf_blur1min  = mix*(*curState->var_pf_blur1min ) + mix2*(*oldState->var_pf_blur1min );
        *curState->var_pf_blur2min  = mix*(*curState->var_pf_blur2min ) + mix2*(*oldState->var_pf_blur2min );
        *curState->var_pf_blur3min  = mix*(*curState->var_pf_blur3min ) + mix2*(*oldState->var_pf_blur3min );
        *curState->var_pf_blur1max  = mix*(*curState->var_pf_blur1max ) + mix2*(*oldState->var_pf_blur1max );
        *curState->var_pf_blur2max  = mix*(*curState->var_pf_blur2max ) + mix2*(*oldState->var_pf_blur2max );
        *curState->var_pf_blur3max  = mix*(*curState->var_pf_blur3max ) + mix2*(*oldState->var_pf_blur3max );
        *curState->var_pf_blur1_edge_darken = mix*(*curState->var_pf_blur1_edge_darken) + mix2*(*oldState->var_pf_blur1_edge_darken);
		// added in BeatDrop v1.4.1:
		*curState->var_pf_mousex       = mix * (*curState->var_pf_mousex) + mix2 * (*oldState->var_pf_mousex);
		*curState->var_pf_mousey       = mix * (*curState->var_pf_mousey) + mix2 * (*oldState->var_pf_mousey);
		*curState->var_pf_mousedown    = (mix < fSnapPoint) ? *oldState->var_pf_mousedown : *curState->var_pf_mousedown;
		*curState->var_pf_mouseclick   = (mix < fSnapPoint) ? *oldState->var_pf_mouseclick : *curState->var_pf_mouseclick;
		// added in BeatDrop v1.5:
		*curState->var_pf_year         = mix*(*curState->var_pf_year) + mix2*(*oldState->var_pf_year);
		*curState->var_pf_month        = mix*(*curState->var_pf_month) + mix2*(*oldState->var_pf_month);
		*curState->var_pf_day          = mix*(*curState->var_pf_day) + mix2*(*oldState->var_pf_day);
		*curState->var_pf_weekday      = mix*(*curState->var_pf_weekday) + mix2*(*oldState->var_pf_weekday);
		*curState->var_pf_hour         = mix*(*curState->var_pf_hour) + mix2*(*oldState->var_pf_hour);
		*curState->var_pf_minute       = mix*(*curState->var_pf_minute) + mix2*(*oldState->var_pf_minute);
		*curState->var_pf_second       = mix*(*curState->var_pf_second) + mix2*(*oldState->var_pf_second);
		*curState->var_pf_milliseconds = mix*(*curState->var_pf_milliseconds) + mix2*(*oldState->var_pf_milliseconds);
		*curState->var_pf_totalseconds = mix*(*curState->var_pf_totalseconds) + mix2*(*oldState->var_pf_totalseconds);
    }
}

void LoadCustomShapePerFrameEvallibVars(CState* pState, int i, int instance, const FrameInputs& in)
{
	*pState->m_shape[i].var_pf_time		= (double)(in.time - in.startTime);
	*pState->m_shape[i].var_pf_frame	= (double)in.frame;
	*pState->m_shape[i].var_pf_fps      = (double)in.fps;
	*pState->m_shape[i].var_pf_progress = (in.time - in.presetStartTime) / (in.nextPresetTime - in.presetStartTime);
	*pState->m_shape[i].var_pf_bass		= (double)in.mysound.imm_rel[0];
	*pState->m_shape[i].var_pf_mid		= (double)in.mysound.imm_rel[1];
	*pState->m_shape[i].var_pf_treb		= (double)in.mysound.imm_rel[2];
	*pState->m_shape[i].var_pf_bass_att	= (double)in.mysound.avg_rel[0];
	*pState->m_shape[i].var_pf_mid_att	= (double)in.mysound.avg_rel[1];
	*pState->m_shape[i].var_pf_treb_att	= (double)in.mysound.avg_rel[2];
    int vi;
    for (vi=0; vi<NUM_Q_VAR; vi++)
        *pState->m_shape[i].var_pf_q[vi] = *pState->var_pf_q[vi];
    for (vi=0; vi<NUM_T_VAR; vi++)
	    *pState->m_shape[i].var_pf_t[vi] = pState->m_shape[i].t_values_after_init_code[vi];
	*pState->m_shape[i].var_pf_x        = pState->m_shape[i].x;
	*pState->m_shape[i].var_pf_y        = pState->m_shape[i].y;
	*pState->m_shape[i].var_pf_rad      = pState->m_shape[i].rad;
	*pState->m_shape[i].var_pf_ang      = pState->m_shape[i].ang;
	*pState->m_shape[i].var_pf_tex_zoom = pState->m_shape[i].tex_zoom;
	*pState->m_shape[i].var_pf_tex_ang  = pState->m_shape[i].tex_ang;
	*pState->m_shape[i].var_pf_sides    = pState->m_shape[i].sides;
    *pState->m_shape[i].var_pf_additive = pState->m_shape[i].additive;
    *pState->m_shape[i].var_pf_textured = pState->m_shape[i].textured;
    *pState->m_shape[i].var_pf_instances = pState->m_shape[i].instances;
    *pState->m_shape[i].var_pf_instance = instance;
    *pState->m_shape[i].var_pf_thick    = pState->m_shape[i].thickOutline;
	*pState->m_shape[i].var_pf_r        = pState->m_shape[i].r;
	*pState->m_shape[i].var_pf_g        = pState->m_shape[i].g;
	*pState->m_shape[i].var_pf_b        = pState->m_shape[i].b;
	*pState->m_shape[i].var_pf_a        = pState->m_shape[i].a;
	*pState->m_shape[i].var_pf_r2       = pState->m_shape[i].r2;
	*pState->m_shape[i].var_pf_g2       = pState->m_shape[i].g2;
	*pState->m_shape[i].var_pf_b2       = pState->m_shape[i].b2;
	*pState->m_shape[i].var_pf_a2       = pState->m_shape[i].a2;
	*pState->m_shape[i].var_pf_border_r = pState->m_shape[i].border_r;
	*pState->m_shape[i].var_pf_border_g = pState->m_shape[i].border_g;
	*pState->m_shape[i].var_pf_border_b = pState->m_shape[i].border_b;
	*pState->m_shape[i].var_pf_border_a = pState->m_shape[i].border_a;
}

void LoadCustomWavePerFrameEvallibVars(CState* pState, int i, const FrameInputs& in)
{
	*pState->m_wave[i].var_pf_time		= (double)(in.time - in.startTime);
	*pState->m_wave[i].var_pf_frame		= (double)in.frame;
	*pState->m_wave[i].var_pf_fps       = (double)in.fps;
	*pState->m_wave[i].var_pf_progress  = (in.time - in.presetStartTime) / (in.nextPresetTime - in.presetStartTime);
	*pState->m_wave[i].var_pf_bass		= (double)in.mysound.imm_rel[0];
	*pState->m_wave[i].var_pf_mid		= (double)in.mysound.imm_rel[1];
	*pState->m_wave[i].var_pf_treb		= (double)in.mysound.imm_rel[2];
	*pState->m_wave[i].var_pf_bass_att	= (double)in.mysound.avg_rel[0];
	*pState->m_wave[i].var_pf_mid_att	= (double)in.mysound.avg_rel[1];
	*pState->m_wave[i].var_pf_treb_att	= (double)in.mysound.avg_rel[2];
    int vi;
    for (vi=0; vi<NUM_Q_VAR; vi++)
	    *pState->m_wave[i].var_pf_q[vi] = *pState->var_pf_q[vi];
    for (vi=0; vi<NUM_T_VAR; vi++)
	    *pState->m_wave[i].var_pf_t[vi] = pState->m_wave[i].t_values_after_init_code[vi];
	*pState->m_wave[i].var_pf_r         = pState->m_wave[i].r;
	*pState->m_wave[i].var_pf_g         = pState->m_wave[i].g;
	*pState->m_wave[i].var_pf_b         = pState->m_wave[i].b;
	*pState->m_wave[i].var_pf_a         = pState->m_wave[i].a;
    *pState->m_wave[i].var_pf_samples   = pState->m_wave[i].samples;
}
