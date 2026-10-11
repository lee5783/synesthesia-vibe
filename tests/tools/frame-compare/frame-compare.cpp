//
//  frame-compare.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-27.
//

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO_REDEFINE
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

constexpr int kChannels = 3;
constexpr int kWindowSize = 11;
constexpr double kWindowSigma = 1.5;
constexpr double kDynamicRange = 255.0;
constexpr int kDefaultDiffGain = 4;
constexpr int kCompositePanels = 3;

struct LoadedImage {
  int width = 0;
  int height = 0;
  std::vector<unsigned char> pixels;
};

bool LoadPng(const char *path, LoadedImage &out, std::string &error) {
  int width = 0;
  int height = 0;
  int sourceChannels = 0;
  unsigned char *data = stbi_load(path, &width, &height, &sourceChannels, kChannels);
  if (data == nullptr) {
    error = stbi_failure_reason() != nullptr ? stbi_failure_reason() : "unknown decode failure";
    return false;
  }
  out.width = width;
  out.height = height;
  out.pixels.assign(data, data + static_cast<size_t>(width) * height * kChannels);
  stbi_image_free(data);
  return true;
}

std::vector<double> MakeGaussianKernel1D(int size, double sigma) {
  std::vector<double> kernel(size);
  const int half = size / 2;
  double sum = 0.0;
  for (int i = 0; i < size; ++i) {
    const double x = static_cast<double>(i - half);
    const double value = std::exp(-(x * x) / (2.0 * sigma * sigma));
    kernel[i] = value;
    sum += value;
  }
  for (int i = 0; i < size; ++i) {
    kernel[i] /= sum;
  }
  return kernel;
}

int ClampIndex(int index, int limit) {
  return std::min(std::max(index, 0), limit - 1);
}

std::vector<double> GaussianBlur(const std::vector<double> &plane, int width, int height,
                                  const std::vector<double> &kernel1D) {
  const int half = static_cast<int>(kernel1D.size()) / 2;
  std::vector<double> horizontal(static_cast<size_t>(width) * height, 0.0);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      double accumulator = 0.0;
      for (int k = -half; k <= half; ++k) {
        const int sourceX = ClampIndex(x + k, width);
        accumulator += plane[static_cast<size_t>(y) * width + sourceX] * kernel1D[k + half];
      }
      horizontal[static_cast<size_t>(y) * width + x] = accumulator;
    }
  }
  std::vector<double> vertical(static_cast<size_t>(width) * height, 0.0);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      double accumulator = 0.0;
      for (int k = -half; k <= half; ++k) {
        const int sourceY = ClampIndex(y + k, height);
        accumulator += horizontal[static_cast<size_t>(sourceY) * width + x] * kernel1D[k + half];
      }
      vertical[static_cast<size_t>(y) * width + x] = accumulator;
    }
  }
  return vertical;
}

struct ChannelMetrics {
  double minSsim = 1.0;
  double meanSsim = 1.0;
  int maxAbsDiff = 0;
  double meanAbsDiff = 0.0;
  long overTolerancePixels = 0;
};

struct FirstViolation {
  bool found = false;
  int x = 0;
  int y = 0;
  int diff = 0;
};

ChannelMetrics CompareChannel(const LoadedImage &a, const LoadedImage &b, int channel,
                               int tolerance, FirstViolation &firstViolation) {
  const int width = a.width;
  const int height = a.height;
  const size_t pixelCount = static_cast<size_t>(width) * height;

  std::vector<double> planeA(pixelCount);
  std::vector<double> planeB(pixelCount);
  std::vector<double> planeAB(pixelCount);
  std::vector<double> planeAA(pixelCount);
  std::vector<double> planeBB(pixelCount);

  ChannelMetrics metrics;
  double ssimSum = 0.0;
  double absDiffSum = 0.0;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t index = static_cast<size_t>(y) * width + x;
      const size_t pixelBase = index * kChannels + channel;
      const double valueA = static_cast<double>(a.pixels[pixelBase]);
      const double valueB = static_cast<double>(b.pixels[pixelBase]);
      planeA[index] = valueA;
      planeB[index] = valueB;
      planeAB[index] = valueA * valueB;
      planeAA[index] = valueA * valueA;
      planeBB[index] = valueB * valueB;

      const int diff = static_cast<int>(std::abs(valueA - valueB));
      absDiffSum += diff;
      if (diff > metrics.maxAbsDiff) {
        metrics.maxAbsDiff = diff;
      }
      if (diff > tolerance) {
        ++metrics.overTolerancePixels;
        if (!firstViolation.found) {
          firstViolation.found = true;
          firstViolation.x = x;
          firstViolation.y = y;
          firstViolation.diff = diff;
        }
      }
    }
  }
  metrics.meanAbsDiff = absDiffSum / static_cast<double>(pixelCount);

  const std::vector<double> kernel1D = MakeGaussianKernel1D(kWindowSize, kWindowSigma);
  const std::vector<double> muA = GaussianBlur(planeA, width, height, kernel1D);
  const std::vector<double> muB = GaussianBlur(planeB, width, height, kernel1D);
  const std::vector<double> muAB = GaussianBlur(planeAB, width, height, kernel1D);
  const std::vector<double> muAA = GaussianBlur(planeAA, width, height, kernel1D);
  const std::vector<double> muBB = GaussianBlur(planeBB, width, height, kernel1D);

  const double c1 = (0.01 * kDynamicRange) * (0.01 * kDynamicRange);
  const double c2 = (0.03 * kDynamicRange) * (0.03 * kDynamicRange);

  metrics.minSsim = 1.0;
  for (size_t index = 0; index < pixelCount; ++index) {
    const double meanA = muA[index];
    const double meanB = muB[index];
    const double varA = muAA[index] - meanA * meanA;
    const double varB = muBB[index] - meanB * meanB;
    const double covAB = muAB[index] - meanA * meanB;

    const double numerator = (2.0 * meanA * meanB + c1) * (2.0 * covAB + c2);
    const double denominator = (meanA * meanA + meanB * meanB + c1) * (varA + varB + c2);
    const double ssim = denominator != 0.0 ? numerator / denominator : 1.0;

    ssimSum += ssim;
    if (ssim < metrics.minSsim) {
      metrics.minSsim = ssim;
    }
  }
  metrics.meanSsim = ssimSum / static_cast<double>(pixelCount);
  return metrics;
}

void PrintUsage(const char *programName) {
  std::fprintf(stderr,
      "Usage: %s <a.png> <b.png> [--tolerance N] [--composite-out <path>] [--diff-gain N]\n\n"
      "Reports SSIM (11x11 Gaussian window, sigma=1.5) and per-channel absolute-difference\n"
      "statistics between two PNGs, per RGB channel (alpha ignored).\n\n"
      "  --tolerance N       Per-channel abs-diff tolerance in 0..255 units (default: 0, i.e.\n"
      "                      byte-identical is required). Any pixel exceeding this on any\n"
      "                      channel makes the tool report FAIL and exit non-zero.\n"
      "  --composite-out P   Write a 3-panel composite PNG (a | b | amplified abs-diff) to P.\n"
      "                      The diff panel scales every channel difference by --diff-gain.\n"
      "  --diff-gain N       Amplification for the composite diff panel (default: 4).\n\n"
      "Exit codes:\n"
      "  0   dimensions match and no pixel exceeds the tolerance\n"
      "  1   dimensions match but at least one pixel exceeds the tolerance\n"
      "  2   usage error, decode failure, a size mismatch, or a composite write failure\n",
      programName);
}

bool WriteComposite(const char *path, const LoadedImage &a, const LoadedImage &b, int diffGain,
                    std::string &error) {
  if (path == nullptr || path[0] == '\0') {
    error = "empty composite path";
    return false;
  }

  const int panelWidth = a.width;
  const int panelHeight = a.height;
  const int compositeWidth = panelWidth * kCompositePanels;
  const size_t compositeRowBytes = static_cast<size_t>(compositeWidth) * kChannels;
  std::vector<unsigned char> composite(compositeRowBytes * static_cast<size_t>(panelHeight), 0);

  for (int y = 0; y < panelHeight; ++y) {
    for (int x = 0; x < panelWidth; ++x) {
      const size_t sourceIndex =
          (static_cast<size_t>(y) * panelWidth + static_cast<size_t>(x)) * kChannels;
      const size_t goldenIndex = static_cast<size_t>(y) * compositeRowBytes +
                                 static_cast<size_t>(x) * kChannels;
      const size_t actualIndex = goldenIndex + static_cast<size_t>(panelWidth) * kChannels;
      const size_t diffIndex = actualIndex + static_cast<size_t>(panelWidth) * kChannels;

      for (int channel = 0; channel < kChannels; ++channel) {
        const int goldenValue = static_cast<int>(a.pixels[sourceIndex + channel]);
        const int actualValue = static_cast<int>(b.pixels[sourceIndex + channel]);
        const int amplified = std::min(std::abs(goldenValue - actualValue) * diffGain, 255);

        composite[goldenIndex + channel] = static_cast<unsigned char>(goldenValue);
        composite[actualIndex + channel] = static_cast<unsigned char>(actualValue);
        composite[diffIndex + channel] = static_cast<unsigned char>(amplified);
      }
    }
  }

  if (stbi_write_png(path, compositeWidth, panelHeight, kChannels, composite.data(),
                     static_cast<int>(compositeRowBytes)) == 0) {
    error = "stbi_write_png failed";
    return false;
  }

  return true;
}

} // namespace

int main(int argc, char **argv) {
  std::vector<std::string> positional;
  std::string compositePath;
  int tolerance = 0;
  int diffGain = kDefaultDiffGain;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      return 0;
    }
    if (arg == "--tolerance") {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "error: --tolerance requires a value\n");
        PrintUsage(argv[0]);
        return 2;
      }
      tolerance = std::atoi(argv[++i]);
      continue;
    }
    if (arg == "--composite-out") {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "error: --composite-out requires a path\n");
        PrintUsage(argv[0]);
        return 2;
      }
      compositePath = argv[++i];
      continue;
    }
    if (arg == "--diff-gain") {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "error: --diff-gain requires a value\n");
        PrintUsage(argv[0]);
        return 2;
      }
      diffGain = std::atoi(argv[++i]);
      if (diffGain < 1) {
        diffGain = 1;
      }
      continue;
    }
    positional.push_back(arg);
  }

  if (positional.size() != 2) {
    PrintUsage(argv[0]);
    return 2;
  }

  LoadedImage imageA;
  LoadedImage imageB;
  std::string error;
  if (!LoadPng(positional[0].c_str(), imageA, error)) {
    std::fprintf(stderr, "error: failed to decode '%s': %s\n", positional[0].c_str(), error.c_str());
    return 2;
  }
  if (!LoadPng(positional[1].c_str(), imageB, error)) {
    std::fprintf(stderr, "error: failed to decode '%s': %s\n", positional[1].c_str(), error.c_str());
    return 2;
  }

  std::printf("frame-compare: '%s' vs '%s' (tolerance=%d)\n",
      positional[0].c_str(), positional[1].c_str(), tolerance);

  if (imageA.width != imageB.width || imageA.height != imageB.height) {
    std::printf("size mismatch: %dx%d vs %dx%d\n",
        imageA.width, imageA.height, imageB.width, imageB.height);
    return 2;
  }

  std::printf("size: %dx%d\n", imageA.width, imageA.height);

  static const char *kChannelNames[kChannels] = {"R", "G", "B"};
  bool anyOverTolerance = false;
  for (int channel = 0; channel < kChannels; ++channel) {
    FirstViolation firstViolation;
    const ChannelMetrics metrics = CompareChannel(imageA, imageB, channel, tolerance, firstViolation);
    std::printf(
        "channel %s: ssim min=%.6f mean=%.6f | abs-diff max=%d mean=%.4f | over-tolerance=%ld\n",
        kChannelNames[channel], metrics.minSsim, metrics.meanSsim,
        metrics.maxAbsDiff, metrics.meanAbsDiff, metrics.overTolerancePixels);
    if (firstViolation.found) {
      std::printf("  first over-tolerance pixel: (x=%d, y=%d) diff=%d\n",
          firstViolation.x, firstViolation.y, firstViolation.diff);
      anyOverTolerance = true;
    }
  }

  if (!compositePath.empty()) {
    std::string compositeError;
    if (!WriteComposite(compositePath.c_str(), imageA, imageB, diffGain, compositeError)) {
      std::fprintf(stderr, "error: failed to write composite '%s': %s\n",
          compositePath.c_str(), compositeError.c_str());
      return 2;
    }
    std::printf("composite: '%s' size=%dx%d panels=golden,actual,abs-diff gain=%d\n",
        compositePath.c_str(), imageA.width * kCompositePanels, imageA.height, diffGain);
  }

  if (anyOverTolerance) {
    std::printf("result: FAIL (at least one pixel exceeds tolerance=%d)\n", tolerance);
    return 1;
  }

  std::printf("result: PASS\n");
  return 0;
}
