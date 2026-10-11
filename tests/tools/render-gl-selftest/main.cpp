//
//  main.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gl_headers.h"

#include "capture.h"
#include "framebuffer.h"
#include "gl_error.h"

namespace
{
    constexpr int kDefaultWidth = 838;
    constexpr int kDefaultHeight = 441;
    constexpr int kClearRed = 51;
    constexpr int kClearGreen = 102;
    constexpr int kClearBlue = 153;
    constexpr float kClearAlpha = 1.0f;
    constexpr const char* kFrameFileName = "frame_0000.png";
    constexpr const char* kGlStringsFileName = "gl-strings.txt";

    const char* UsageLine()
    {
        return "usage: render-gl-selftest --out <dir> [--width N] [--height N]";
    }

    bool ParseIntArg(const char* text, int& outValue)
    {
        if (text == nullptr || text[0] == '\0')
            return false;

        char* end = nullptr;
        const long parsed = strtol(text, &end, 10);

        if (end == nullptr || *end != '\0' || parsed <= 0 || parsed > 65536)
            return false;

        outValue = static_cast<int>(parsed);
        return true;
    }

    int CountWrongPixels(const std::vector<unsigned char>& pixels)
    {
        int wrong = 0;

        for (size_t index = 0; index + 2 < pixels.size(); index += 3)
        {
            if (pixels[index] != kClearRed ||
                pixels[index + 1] != kClearGreen ||
                pixels[index + 2] != kClearBlue)
            {
                ++wrong;
            }
        }

        return wrong;
    }
}

int main(int argc, char** argv)
{
    std::string outDir;
    int width = kDefaultWidth;
    int height = kDefaultHeight;

    for (int index = 1; index < argc; ++index)
    {
        const char* arg = argv[index];

        if (strcmp(arg, "--out") == 0 && index + 1 < argc)
        {
            outDir = argv[++index];
        }
        else if (strcmp(arg, "--width") == 0 && index + 1 < argc)
        {
            if (!ParseIntArg(argv[++index], width))
            {
                fprintf(stderr, "render-gl-selftest: invalid --width value\n");
                fprintf(stderr, "%s\n", UsageLine());
                return 2;
            }
        }
        else if (strcmp(arg, "--height") == 0 && index + 1 < argc)
        {
            if (!ParseIntArg(argv[++index], height))
            {
                fprintf(stderr, "render-gl-selftest: invalid --height value\n");
                fprintf(stderr, "%s\n", UsageLine());
                return 2;
            }
        }
        else
        {
            fprintf(stderr, "render-gl-selftest: unrecognized argument \"%s\"\n", arg);
            fprintf(stderr, "%s\n", UsageLine());
            return 2;
        }
    }

    if (outDir.empty())
    {
        fprintf(stderr, "render-gl-selftest: --out is required\n");
        fprintf(stderr, "%s\n", UsageLine());
        return 2;
    }

    std::error_code dirError;
    std::filesystem::create_directories(outDir, dirError);

    if (dirError)
    {
        fprintf(stderr, "render-gl-selftest: cannot create output directory \"%s\": %s\n",
            outDir.c_str(), dirError.message().c_str());
        return 2;
    }

    glfwSetErrorCallback(nullptr);

    if (glfwInit() != GLFW_TRUE)
    {
        fprintf(stderr, "render-gl-selftest: glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(width, height, "render-gl-selftest", nullptr, nullptr);

    if (window == nullptr)
    {
        fprintf(stderr, "render-gl-selftest: glfwCreateWindow failed for %dx%d\n", width, height);
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    if (gladLoadGL(glfwGetProcAddress) == 0)
    {
        fprintf(stderr, "render-gl-selftest: gladLoadGL failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* shadingLanguageVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    if (vendor == nullptr)
        vendor = "(null)";
    if (renderer == nullptr)
        renderer = "(null)";
    if (version == nullptr)
        version = "(null)";
    if (shadingLanguageVersion == nullptr)
        shadingLanguageVersion = "(null)";

    printf("GL_VENDOR: %s\n", vendor);
    printf("GL_RENDERER: %s\n", renderer);
    printf("GL_VERSION: %s\n", version);
    printf("GL_SHADING_LANGUAGE_VERSION: %s\n", shadingLanguageVersion);

    const std::string glStringsPath = outDir + "/" + kGlStringsFileName;
    std::ofstream glStringsFile(glStringsPath, std::ios::out | std::ios::trunc | std::ios::binary);

    if (!glStringsFile.is_open())
    {
        fprintf(stderr, "render-gl-selftest: cannot write \"%s\"\n", glStringsPath.c_str());
        glfwDestroyWindow(window);
        glfwTerminate();
        return 2;
    }

    glStringsFile << "GL_VENDOR: " << vendor << "\n";
    glStringsFile << "GL_RENDERER: " << renderer << "\n";
    glStringsFile << "GL_VERSION: " << version << "\n";
    glStringsFile << "GL_SHADING_LANGUAGE_VERSION: " << shadingLanguageVersion << "\n";
    glStringsFile.close();

    int exitCode = EXIT_SUCCESS;

    {
        render_gl::Framebuffer framebuffer;

        if (!framebuffer.Create(width, height))
        {
            fprintf(stderr, "render-gl-selftest: Framebuffer::Create failed for %dx%d\n", width, height);
            exitCode = EXIT_FAILURE;
        }
        else
        {
            framebuffer.Bind();
            glClearColor(kClearRed / 255.0f, kClearGreen / 255.0f, kClearBlue / 255.0f, kClearAlpha);
            glClear(GL_COLOR_BUFFER_BIT);

            std::vector<unsigned char> pixelsTopDown;

            if (!render_gl::ReadbackRgbTopDown(width, height, pixelsTopDown))
            {
                fprintf(stderr, "render-gl-selftest: ReadbackRgbTopDown failed for %dx%d\n", width, height);
                exitCode = EXIT_FAILURE;
            }
            else
            {
                const int expectedPixels = width * height;
                const int wrongPixels = CountWrongPixels(pixelsTopDown);

                printf("Clear color (expected RGB): %d %d %d\n", kClearRed, kClearGreen, kClearBlue);
                printf("Readback pixels checked: %d\n", expectedPixels);
                printf("Readback pixels not matching clear color: %d\n", wrongPixels);
                printf("Readback matches clear color: %s\n", wrongPixels == 0 ? "yes" : "no");

                if (wrongPixels != 0)
                    exitCode = EXIT_FAILURE;

                const std::string pngPath = outDir + "/" + kFrameFileName;

                if (!render_gl::WritePngRgbTopDown(pngPath.c_str(), width, height, pixelsTopDown))
                {
                    fprintf(stderr, "render-gl-selftest: WritePngRgbTopDown failed for \"%s\"\n", pngPath.c_str());
                    exitCode = EXIT_FAILURE;
                }
                else
                {
                    printf("PNG written: %s (%dx%d, 3 channels)\n", pngPath.c_str(), width, height);
                }

                framebuffer.Unbind();
            }
        }

        const int drainedErrors = render_gl::DrainGlErrors("render-gl-selftest");

        printf("GL errors drained: %d\n", drainedErrors);

        if (drainedErrors != 0)
            exitCode = EXIT_FAILURE;
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    if (exitCode == EXIT_SUCCESS)
        printf("render-gl-selftest: OK out=%s\n", outDir.c_str());
    else
        printf("render-gl-selftest: FAIL out=%s\n", outDir.c_str());

    fflush(stdout);
    return exitCode;
}