//
//  equation_drivers.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-29.
//

#ifndef __BEATDROP_PARSER_EQUATION_DRIVERS_H__
#define __BEATDROP_PARSER_EQUATION_DRIVERS_H__ 1

#include "state.h"
#include "frame_inputs.h"
#include "../ns-eel2-shim/ns-eel.h"

void LoadPerFrameEvallibVars(CState* pState, const FrameInputs& in);
void RunPerFrameEquations(CState* curState, CState* oldState, int code, const FrameInputs& in, float& fSnapPoint);
void LoadCustomWavePerFrameEvallibVars(CState* pState, int i, const FrameInputs& in);
void LoadCustomShapePerFrameEvallibVars(CState* pState, int i, int instance, const FrameInputs& in);

inline void EvaluatePerVertexEquations(CState* pState, float vertX, float vertY, float vertRad, float vertAng,
                                        bool bScreenDependentRenderMode, float fAspectX, float fAspectY,
                                        float& fZoom, float& fZoomExp, float& fRot, float& fWarp,
                                        float& fCX, float& fCY, float& fDX, float& fDY, float& fSX, float& fSY)
{
	if (pState->m_pp_codehandle)
	{
		// restore all the variables to their original states,
		//  run the user-defined equations,
		//  then move the results into local vars for computation as floats
		if (bScreenDependentRenderMode)
		{
			*pState->var_pv_x = (double)(vertX*0.5f+0.5f);
			*pState->var_pv_y = (double)(vertY*-0.5f+0.5f);
		}
		else
		{
			*pState->var_pv_x = (double)(vertX*0.5f*fAspectX+0.5f);
			*pState->var_pv_y = (double)(vertY*-0.5f*fAspectY+0.5f);
		}
		*pState->var_pv_rad		= (double)vertRad;
		*pState->var_pv_ang		= (double)vertAng;
		*pState->var_pv_zoom	= *pState->var_pf_zoom;
		*pState->var_pv_zoomexp	= *pState->var_pf_zoomexp;
		*pState->var_pv_rot		= *pState->var_pf_rot;
		*pState->var_pv_warp	= *pState->var_pf_warp;
		*pState->var_pv_cx		= *pState->var_pf_cx;
		*pState->var_pv_cy		= *pState->var_pf_cy;
		*pState->var_pv_dx		= *pState->var_pf_dx;
		*pState->var_pv_dy		= *pState->var_pf_dy;
		*pState->var_pv_sx		= *pState->var_pf_sx;
		*pState->var_pv_sy		= *pState->var_pf_sy;
		//*pState->var_pv_time		= *pState->var_pv_time;		// (these are all now initialized
		//*pState->var_pv_bass		= *pState->var_pv_bass;		//  just once per frame)
		//*pState->var_pv_mid		= *pState->var_pv_mid;
		//*pState->var_pv_treb		= *pState->var_pv_treb;
		//*pState->var_pv_bass_att	= *pState->var_pv_bass_att;
		//*pState->var_pv_mid_att	= *pState->var_pv_mid_att;
		//*pState->var_pv_treb_att	= *pState->var_pv_treb_att;

#ifndef _NO_EXPR_
		NSEEL_code_execute(pState->m_pp_codehandle);
#endif

		fZoom = (float)(*pState->var_pv_zoom);
		fZoomExp = (float)(*pState->var_pv_zoomexp);
		fRot  = (float)(*pState->var_pv_rot);
		fWarp = (float)(*pState->var_pv_warp);
		fCX   = (float)(*pState->var_pv_cx);
		fCY   = (float)(*pState->var_pv_cy);
		fDX   = (float)(*pState->var_pv_dx);
		fDY   = (float)(*pState->var_pv_dy);
		fSX   = (float)(*pState->var_pv_sx);
		fSY   = (float)(*pState->var_pv_sy);
	}
}

#endif
