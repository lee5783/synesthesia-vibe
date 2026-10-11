//
//  bindings_check.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_SHADER_TRANSLATE_CHECK_BINDINGS_CHECK_H__
#define __BEATDROP_SHADER_TRANSLATE_CHECK_BINDINGS_CHECK_H__ 1

#include <string>
#include <vector>

#include "shader_translator.h"

struct PresetShaderTexts
{
    std::string name;
    std::string warp;
    std::string comp;
};

int RunBindingsCheck(const std::vector<PresetShaderTexts>& presets, const std::string& resourcesDir, const std::string& goldenDir,
    render_gl::PresetShaderCache& cache);

#endif
