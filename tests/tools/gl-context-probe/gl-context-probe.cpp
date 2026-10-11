//
//  gl-context-probe.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-30.
//

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

typedef void *(*GlGetProcAddressFn)(const char *);
typedef void(__stdcall *PfnGlClearColor)(float, float, float, float);
typedef void(__stdcall *PfnGlClear)(unsigned int);
typedef void(__stdcall *PfnGlReadPixels)(int, int, int, int, unsigned int, unsigned int, void *);
typedef const unsigned char *(__stdcall *PfnGlGetString)(unsigned int);
typedef void(__stdcall *PfnGlGetIntegerv)(unsigned int, int *);
typedef void(__stdcall *PfnGlEnable)(unsigned int);
typedef void(__stdcall *PfnGlDisable)(unsigned int);
typedef void(__stdcall *PfnGlScissor)(int, int, int, int);
typedef void(__stdcall *PfnGlViewport)(int, int, int, int);

namespace {

constexpr unsigned int kGlColorBufferBit = 0x00004000;
constexpr unsigned int kGlRgb = 0x1907;
constexpr unsigned int kGlUnsignedByte = 0x1401;
constexpr unsigned int kGlVendor = 0x1F00;
constexpr unsigned int kGlRenderer = 0x1F01;
constexpr unsigned int kGlVersion = 0x1F02;
constexpr unsigned int kGlShadingLanguageVersion = 0x8B8C;
constexpr unsigned int kGlContextProfileMask = 0x9126;
constexpr unsigned int kGlContextFlags = 0x821E;
constexpr unsigned int kGlContextCoreProfileBit = 0x00000001;
constexpr unsigned int kGlContextFlagForwardCompatibleBit = 0x00000001;
constexpr unsigned int kGlScissorTest = 0x0C11;

constexpr float kClearR = 0.25f;
constexpr float kClearG = 0.50f;
constexpr float kClearB = 0.75f;
constexpr float kClearA = 1.0f;
constexpr int kWindowWidth = 64;
constexpr int kWindowHeight = 64;

// Larger offscreen framebuffer used only for the PNG screenshot dump, so the
// image is legible at normal viewing size. The pixel-readback correctness
// check above still runs against the original kWindowWidth x kWindowHeight
// context.
constexpr int kShotWidth = 256;
constexpr int kShotHeight = 256;

struct QuadrantColor {
  float r, g, b;
};

// Four flat-colored quadrants, drawn with glScissor + glClear (both valid in
// a 3.3 Core profile with no shaders/VAOs needed) so the PNG visibly proves
// real per-region GPU rendering, not just a single full-window clear.
constexpr QuadrantColor kQuadrants[4] = {
    {0.90f, 0.20f, 0.20f},  // top-left: red
    {0.20f, 0.80f, 0.20f},  // top-right: green
    {0.20f, 0.40f, 0.95f},  // bottom-left: blue
    {0.95f, 0.85f, 0.15f},  // bottom-right: yellow
};

int FloatToByte(float value) {
  return static_cast<int>(value * 255.0f + 0.5f);
}

void PrintGlfwError(int code, const char *description) {
  std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
}

}  // namespace

int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);

  const char *screenshotPath = argc > 1 ? argv[1] : nullptr;

  glfwSetErrorCallback(PrintGlfwError);

  if (!glfwInit()) {
    std::fprintf(stderr, "gl-context-probe: glfwInit failed\n");
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

  GLFWwindow *window = glfwCreateWindow(kWindowWidth, kWindowHeight, "gl-context-probe", nullptr, nullptr);
  if (window == nullptr) {
    std::fprintf(stderr, "gl-context-probe: glfwCreateWindow failed (no 3.3 Core forward-compatible context)\n");
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);

  auto getProcAddress = reinterpret_cast<GlGetProcAddressFn>(glfwGetProcAddress);
  auto glGetString = reinterpret_cast<PfnGlGetString>(glfwGetProcAddress("glGetString"));
  auto glGetIntegerv = reinterpret_cast<PfnGlGetIntegerv>(glfwGetProcAddress("glGetIntegerv"));
  auto glClearColor = reinterpret_cast<PfnGlClearColor>(glfwGetProcAddress("glClearColor"));
  auto glClear = reinterpret_cast<PfnGlClear>(glfwGetProcAddress("glClear"));
  auto glReadPixels = reinterpret_cast<PfnGlReadPixels>(glfwGetProcAddress("glReadPixels"));
  (void)getProcAddress;

  if (glGetString == nullptr || glGetIntegerv == nullptr || glClearColor == nullptr || glClear == nullptr ||
      glReadPixels == nullptr) {
    std::fprintf(stderr, "gl-context-probe: glfwGetProcAddress failed to resolve core GL entry points\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  const unsigned char *vendor = glGetString(kGlVendor);
  const unsigned char *renderer = glGetString(kGlRenderer);
  const unsigned char *version = glGetString(kGlVersion);
  const unsigned char *shadingLanguageVersion = glGetString(kGlShadingLanguageVersion);

  int profileMask = 0;
  int contextFlags = 0;
  glGetIntegerv(kGlContextProfileMask, &profileMask);
  glGetIntegerv(kGlContextFlags, &contextFlags);

  std::printf("GL_VENDOR: %s\n", vendor != nullptr ? reinterpret_cast<const char *>(vendor) : "(null)");
  std::printf("GL_RENDERER: %s\n", renderer != nullptr ? reinterpret_cast<const char *>(renderer) : "(null)");
  std::printf("GL_VERSION: %s\n", version != nullptr ? reinterpret_cast<const char *>(version) : "(null)");
  std::printf("GL_SHADING_LANGUAGE_VERSION: %s\n",
              shadingLanguageVersion != nullptr ? reinterpret_cast<const char *>(shadingLanguageVersion) : "(null)");
  std::printf("GL_CONTEXT_PROFILE_MASK: 0x%08X (core=%s)\n", profileMask,
              (profileMask & kGlContextCoreProfileBit) != 0 ? "yes" : "no");
  std::printf("GL_CONTEXT_FLAGS: 0x%08X (forward_compatible=%s)\n", contextFlags,
              (contextFlags & kGlContextFlagForwardCompatibleBit) != 0 ? "yes" : "no");

  glClearColor(kClearR, kClearG, kClearB, kClearA);
  glClear(kGlColorBufferBit);

  unsigned char pixel[3] = {0, 0, 0};
  glReadPixels(kWindowWidth / 2, kWindowHeight / 2, 1, 1, kGlRgb, kGlUnsignedByte, pixel);

  const int expectedR = FloatToByte(kClearR);
  const int expectedG = FloatToByte(kClearG);
  const int expectedB = FloatToByte(kClearB);

  std::printf("Clear color (expected RGB): %d %d %d\n", expectedR, expectedG, expectedB);
  std::printf("Readback pixel (actual RGB): %d %d %d\n", pixel[0], pixel[1], pixel[2]);

  const bool readbackMatches = pixel[0] == expectedR && pixel[1] == expectedG && pixel[2] == expectedB;
  std::printf("Readback matches clear color: %s\n", readbackMatches ? "yes" : "no");

  bool screenshotOk = true;
  if (screenshotPath != nullptr) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow *shotWindow = glfwCreateWindow(kShotWidth, kShotHeight, "gl-context-probe-screenshot", nullptr, nullptr);
    if (shotWindow == nullptr) {
      std::fprintf(stderr, "gl-context-probe: glfwCreateWindow failed for the screenshot context\n");
      screenshotOk = false;
    } else {
      glfwMakeContextCurrent(shotWindow);

      auto shotGlClearColor = reinterpret_cast<PfnGlClearColor>(glfwGetProcAddress("glClearColor"));
      auto shotGlClear = reinterpret_cast<PfnGlClear>(glfwGetProcAddress("glClear"));
      auto shotGlReadPixels = reinterpret_cast<PfnGlReadPixels>(glfwGetProcAddress("glReadPixels"));
      auto shotGlEnable = reinterpret_cast<PfnGlEnable>(glfwGetProcAddress("glEnable"));
      auto shotGlDisable = reinterpret_cast<PfnGlDisable>(glfwGetProcAddress("glDisable"));
      auto shotGlScissor = reinterpret_cast<PfnGlScissor>(glfwGetProcAddress("glScissor"));
      auto shotGlViewport = reinterpret_cast<PfnGlViewport>(glfwGetProcAddress("glViewport"));

      if (shotGlClearColor == nullptr || shotGlClear == nullptr || shotGlReadPixels == nullptr ||
          shotGlEnable == nullptr || shotGlDisable == nullptr || shotGlScissor == nullptr ||
          shotGlViewport == nullptr) {
        std::fprintf(stderr, "gl-context-probe: glfwGetProcAddress failed to resolve entry points for the screenshot context\n");
        screenshotOk = false;
      } else {
        shotGlViewport(0, 0, kShotWidth, kShotHeight);
        shotGlEnable(kGlScissorTest);

        const int halfW = kShotWidth / 2;
        const int halfH = kShotHeight / 2;
        // GL's origin is bottom-left, so index 0/1 (top row visually) are the
        // upper half of the framebuffer, i.e. the higher Y scissor rect.
        const int quadrantX[4] = {0, halfW, 0, halfW};
        const int quadrantY[4] = {halfH, halfH, 0, 0};

        for (int i = 0; i < 4; ++i) {
          shotGlScissor(quadrantX[i], quadrantY[i], halfW, halfH);
          shotGlClearColor(kQuadrants[i].r, kQuadrants[i].g, kQuadrants[i].b, 1.0f);
          shotGlClear(kGlColorBufferBit);
        }

        shotGlDisable(kGlScissorTest);

        std::vector<unsigned char> pixels(static_cast<size_t>(kShotWidth) * kShotHeight * 3);
        shotGlReadPixels(0, 0, kShotWidth, kShotHeight, kGlRgb, kGlUnsignedByte, pixels.data());

        // glReadPixels returns bottom-up rows; flip to top-down for a
        // normally-oriented PNG.
        std::vector<unsigned char> flipped(pixels.size());
        const size_t rowBytes = static_cast<size_t>(kShotWidth) * 3;
        for (int row = 0; row < kShotHeight; ++row) {
          const unsigned char *src = pixels.data() + static_cast<size_t>(kShotHeight - 1 - row) * rowBytes;
          unsigned char *dst = flipped.data() + static_cast<size_t>(row) * rowBytes;
          std::memcpy(dst, src, rowBytes);
        }

        const int pngWriteOk =
            stbi_write_png(screenshotPath, kShotWidth, kShotHeight, 3, flipped.data(), static_cast<int>(rowBytes));
        if (pngWriteOk == 0) {
          std::fprintf(stderr, "gl-context-probe: stbi_write_png failed for %s\n", screenshotPath);
          screenshotOk = false;
        } else {
          std::printf("Screenshot written: %s (%dx%d, 4-quadrant scissor+clear test pattern)\n", screenshotPath,
                       kShotWidth, kShotHeight);
        }
      }

      glfwDestroyWindow(shotWindow);
    }
  }

  glfwDestroyWindow(window);
  glfwTerminate();

  return (readbackMatches && screenshotOk) ? 0 : 1;
}
