//
//  capture.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#ifndef __BEATDROP_RENDER_GL_CAPTURE_H__
#define __BEATDROP_RENDER_GL_CAPTURE_H__ 1

#include <vector>

namespace render_gl
{

bool ReadbackRgbTopDown(int width, int height, std::vector<unsigned char>& outPixels);

bool WritePngRgbTopDown(const char* szPath, int width, int height, const std::vector<unsigned char>& pixelsTopDown);

}

#endif