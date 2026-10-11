//
//  show_to_user.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "show_to_user.h"
#include "blend_state.h"
#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace render_gl
{

namespace
{

constexpr int kQuadVertexCount = 4;

uint32_t PackColor01(float r, float g, float b)
{
    const auto channel = [](float v)
    {
        return static_cast<uint32_t>(static_cast<int>(v * 255.0f)) & 0xFFu;
    };

    return 0xFF000000u | (channel(r) << 16) | (channel(g) << 8) | channel(b);
}

void SetQuadExtents(render_core::Vertex (&quad)[kQuadVertexCount], float extentX, float extentY)
{
    quad[0].x = -extentX;
    quad[1].x = extentX;
    quad[2].x = -extentX;
    quad[3].x = extentX;
    quad[0].y = extentY;
    quad[1].y = extentY;
    quad[2].y = -extentY;
    quad[3].y = -extentY;
}

void SetQuadTint(render_core::Vertex (&quad)[kQuadVertexCount], const float (&shade)[kQuadVertexCount][3], float scale)
{
    for (int k = 0; k < kQuadVertexCount; k++)
        quad[k].Diffuse = PackColor01(scale * shade[k][0], scale * shade[k][1], scale * shade[k][2]);
}

void DrawQuad(MeshBuffer& mesh, const render_core::Vertex (&quad)[kQuadVertexCount])
{
    mesh.UploadVertices(quad, kQuadVertexCount);
    mesh.DrawTriangleStrip(kQuadVertexCount);
}

void SetBlendEnabled(BlendFactor src, BlendFactor dst)
{
    SetBlendState(true, src, dst);
}

}

void ShowToUserNoShaders(const ShowToUserInput& input, const DrawProgram& textured,
    const DrawProgram& untextured, MeshBuffer& quadMesh, Texture& source)
{
    render_core::Vertex v3[kQuadVertexCount] = {};

    const float onePlusInvWidth = 1.0f + 1.0f / static_cast<float>(input.width);
    const float onePlusInvHeight = 1.0f + 1.0f / static_cast<float>(input.height);

    SetQuadExtents(v3, onePlusInvWidth, onePlusInvHeight);

    const float aspect = static_cast<float>(input.width) / (static_cast<float>(input.height) * input.invAspectY);
    float xAspectMult = 1.0f;
    float yAspectMult = 1.0f;

    if (!input.screenDependentRenderMode)
    {
        if (aspect > 1)
            yAspectMult = aspect;
        else
            xAspectMult = 1.0f / aspect;
    }

    for (render_core::Vertex& vert : v3)
    {
        vert.x *= xAspectMult;
        vert.y *= yAspectMult;
    }

    float shade[kQuadVertexCount][3] = {
        { 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f } };

    if (input.shaderAmount > 0.001f)
    {
        for (int i = 0; i < kQuadVertexCount; i++)
        {
            shade[i][0] = 0.6f + 0.3f * sinf(input.time * 30.0f * 0.0143f + 3 + i * 21 + input.randStart[3]);
            shade[i][1] = 0.6f + 0.3f * sinf(input.time * 30.0f * 0.0107f + 1 + i * 13 + input.randStart[1]);
            shade[i][2] = 0.6f + 0.3f * sinf(input.time * 30.0f * 0.0129f + 6 + i * 9 + input.randStart[2]);

            float maxComponent = ((shade[i][0] > shade[i][1]) ? shade[i][0] : shade[i][1]);

            if (shade[i][2] > maxComponent)
                maxComponent = shade[i][2];

            for (int k = 0; k < 3; k++)
            {
                shade[i][k] /= maxComponent;
                shade[i][k] = 0.5f + 0.5f * shade[i][k];
            }

            for (int k = 0; k < 3; k++)
                shade[i][k] = shade[i][k] * input.shaderAmount + 1.0f * (1.0f - input.shaderAmount);

            v3[i].Diffuse = PackColor01(shade[i][0], shade[i][1], shade[i][2]);
        }
    }
    else
    {
        for (render_core::Vertex& vert : v3)
            vert.Diffuse = 0xFFFFFFFFu;
    }

    source.SetWrapRepeat(input.wrap);

    textured.Bind();
    textured.SetUvOffset(0.0f, 0.0f);
    source.Bind();

    const float fVideoEchoZoom = input.echoZoom;
    const float fVideoEchoAlpha = input.echoAlpha;
    const int nVideoEchoOrientation = input.echoOrient;
    const float fGammaAdj = input.gamma;

    if (fVideoEchoAlpha > 0.001f)
    {
        SetBlendEnabled(BlendFactor::One, BlendFactor::Zero);

        for (int i = 0; i < 2; i++)
        {
            const float fZoom = (i == 0) ? 1.0f : fVideoEchoZoom;

            const float tempLo = 0.5f - 0.5f / fZoom;
            const float tempHi = 0.5f + 0.5f / fZoom;
            v3[0].tu = tempLo;
            v3[0].tv = tempHi;
            v3[1].tu = tempHi;
            v3[1].tv = tempHi;
            v3[2].tu = tempLo;
            v3[2].tv = tempLo;
            v3[3].tu = tempHi;
            v3[3].tv = tempLo;

            if (i == 1)
            {
                for (int j = 0; j < kQuadVertexCount; j++)
                {
                    if (nVideoEchoOrientation % 2)
                        v3[j].tu = 1.0f - v3[j].tu;

                    if (nVideoEchoOrientation >= 2)
                        v3[j].tv = 1.0f - v3[j].tv;
                }
            }

            const float mix = (i == 1) ? fVideoEchoAlpha : 1.0f - fVideoEchoAlpha;

            SetQuadTint(v3, shade, mix);
            DrawQuad(quadMesh, v3);

            if (i == 0)
                SetBlendEnabled(BlendFactor::One, BlendFactor::One);

            if (fGammaAdj > 0.001f)
            {
                const int nRedraws = static_cast<int>(fGammaAdj - 0.0001f);

                for (int nRedraw = 0; nRedraw < nRedraws; nRedraw++)
                {
                    float gamma;

                    if (nRedraw == nRedraws - 1)
                        gamma = fGammaAdj - static_cast<float>(static_cast<int>(fGammaAdj - 0.0001f));
                    else
                        gamma = 1.0f;

                    SetQuadTint(v3, shade, gamma * mix);
                    DrawQuad(quadMesh, v3);
                }
            }
        }
    }
    else
    {
        v3[0].tu = 0;
        v3[1].tu = 1;
        v3[2].tu = 0;
        v3[3].tu = 1;
        v3[0].tv = 1;
        v3[1].tv = 1;
        v3[2].tv = 0;
        v3[3].tv = 0;

        SetBlendOpaque();

        const int nPasses = static_cast<int>(fGammaAdj - 0.001f) + 1;

        for (int nPass = 0; nPass < nPasses; nPass++)
        {
            float gamma;

            if (nPass == nPasses - 1)
                gamma = fGammaAdj - static_cast<float>(nPass);
            else
                gamma = 1.0f;

            SetQuadTint(v3, shade, gamma);
            DrawQuad(quadMesh, v3);

            if (nPass == 0)
                SetBlendEnabled(BlendFactor::One, BlendFactor::One);
        }
    }

    render_core::Vertex flat[kQuadVertexCount] = {};

    SetQuadExtents(flat, onePlusInvWidth, onePlusInvHeight);

    for (render_core::Vertex& vert : flat)
        vert.Diffuse = 0xFFFFFFFFu;

    const bool anyFilter = input.brighten || input.darken || input.solarize || input.invert;

    if (!anyFilter)
    {
        SetBlendOpaque();
        return;
    }

    untextured.Bind();
    untextured.SetUvOffset(0.0f, 0.0f);

    if (input.brighten)
    {
        SetBlendEnabled(BlendFactor::InvDstColor, BlendFactor::Zero);
        DrawQuad(quadMesh, flat);

        SetBlendEnabled(BlendFactor::Zero, BlendFactor::DstColor);
        DrawQuad(quadMesh, flat);

        SetBlendEnabled(BlendFactor::InvDstColor, BlendFactor::Zero);
        DrawQuad(quadMesh, flat);
    }

    if (input.darken)
    {
        SetBlendEnabled(BlendFactor::Zero, BlendFactor::DstColor);
        DrawQuad(quadMesh, flat);
    }

    if (input.solarize)
    {
        SetBlendEnabled(BlendFactor::Zero, BlendFactor::InvDstColor);
        DrawQuad(quadMesh, flat);

        SetBlendEnabled(BlendFactor::DstColor, BlendFactor::One);
        DrawQuad(quadMesh, flat);
    }

    if (input.invert)
    {
        SetBlendEnabled(BlendFactor::InvDstColor, BlendFactor::Zero);
        DrawQuad(quadMesh, flat);
    }

    SetBlendOpaque();
}

}
