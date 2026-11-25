//
//  snapshot_writer.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-24.
//

#ifndef __SYNESTHESIA_VIBE_WINDOWS_SNAPSHOT_WRITER_H__
#define __SYNESTHESIA_VIBE_WINDOWS_SNAPSHOT_WRITER_H__ 1

#include <cstdint>
#include <string>
#include <vector>

namespace svw
{

bool ReadFramebufferTopDown(uint32_t framebuffer, int width, int height, std::vector<uint8_t>& outRgba);
bool WritePngUtf8(const char* path, const std::vector<uint8_t>& rgba, int width, int height, std::string& error);

}

#endif
