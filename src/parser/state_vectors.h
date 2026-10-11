//
//  state_vectors.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-29.
//

#ifndef __BEATDROP_PARSER_STATE_VECTORS_H__
#define __BEATDROP_PARSER_STATE_VECTORS_H__ 1

struct Vec3f
{
    float x, y, z;
};

struct Vec4f
{
    Vec4f() {};
    Vec4f( float fx, float fy, float fz, float fw )
    {
        x = fx; y = fy; z = fz; w = fw;
    }

    float x, y, z, w;
};

#endif
