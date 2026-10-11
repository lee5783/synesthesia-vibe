//
//  math_macros.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-29.
//

#ifndef __BEATDROP_PLATFORM_MATH_MACROS_H__
#define __BEATDROP_PLATFORM_MATH_MACROS_H__ 1

#include <cmath>

#define CosineInterp(x) (0.5f - 0.5f*cosf((x) * 3.1415926535898f))
#define InvCosineInterp(x) (acosf(1.0f - 2.0f*(x))/3.1415926535898f)

#endif
