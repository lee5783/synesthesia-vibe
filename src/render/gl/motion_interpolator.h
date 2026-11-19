//
//  motion_interpolator.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

#ifndef __BEATDROP_RENDER_GL_MOTION_INTERPOLATOR_H__
#define __BEATDROP_RENDER_GL_MOTION_INTERPOLATOR_H__ 1

#include "framebuffer.h"
#include "shader.h"
#include "vertex_array.h"

namespace render_gl
{

class MotionInterpolator
{
public:
    MotionInterpolator();

    enum class BuildState
    {
        Pending,
        Ready,
        Failed
    };

    void BeginCreate(int width, int height, int flowPixels);
    BuildState BuildNext();
    bool Resize(int width, int height, int flowPixels);
    void Destroy();
    void ResetHistory();
    void MarkCut() { m_cutPending = true; }
    bool Building() const { return m_buildStage >= 0 && m_buildStage < kBuildStageCount; }
    bool Ready() const { return m_buildStage == kBuildStageCount; }

    unsigned int CaptureFramebufferId() const { return m_slots[1 - m_newestIndex].Id(); }
    void CommitCapture(float simulationTime);

    void Present(float fraction, unsigned int targetFbo, bool useField);
    float PresentedTime(float fraction) const;

    bool HasHistory() const { return m_capturedCount > 0; }
    bool HasPrevious() const { return m_capturedCount > 1; }
    bool Valid() const { return Ready() && m_blend.Valid() && m_slots[0].Valid() && m_slots[1].Valid(); }
    bool FlowValid() const { return m_flowValid; }
    int FlowWidth() const { return m_flowValid ? m_luma[0][m_flowFinest].Width() : 0; }
    int FlowHeight() const { return m_flowValid ? m_luma[0][m_flowFinest].Height() : 0; }
    int FlowLevels() const { return m_flowValid ? m_flowLevels : 0; }

private:
    static constexpr int kMaxLumaLevels = 16;
    static constexpr int kMaxFlowLevels = 5;
    static constexpr int kBuildStageCount = 9;

    struct PassUniforms
    {
        int source;
        int target;
        int coarse;
        int previous;
        int newest;
        int forward;
        int backward;
        int field;
        int size;
        int coarseSize;
        int displaySize;
        int hasCoarse;
        int refine;
        int fromColor;
        int fraction;
        int scale;
        int alpha;
    };

    static void LocateUniforms(const Shader& shader, PassUniforms& uniforms);

    bool PlanFlow();
    bool BuildStage(int stage);
    bool CreateFlowTargets();
    bool CreateLumaSlot(int slot);
    bool CreateFieldTargets();
    void DestroyFlowTargets();
    void DestroyFlow();
    void BuildPyramid(int slot);
    void RunFlowStages();
    void EstimateFlow();
    void Match(int fromSlot, int toSlot, Framebuffer* fields);
    void Trust(int fromSlot, int toSlot);
    void Blend(const Framebuffer& previous, const Framebuffer& newest, float alpha, unsigned int targetFbo);
    void Warp(float fraction, unsigned int targetFbo);

    Shader m_blend;
    Shader m_downsample;
    Shader m_match;
    Shader m_trust;
    Shader m_field;
    Shader m_warp;
    PassUniforms m_blendUniforms;
    PassUniforms m_downsampleUniforms;
    PassUniforms m_matchUniforms;
    PassUniforms m_trustUniforms;
    PassUniforms m_fieldUniforms;
    PassUniforms m_warpUniforms;
    VertexArray m_emptyArray;
    Framebuffer m_slots[2];
    Framebuffer m_luma[2][kMaxLumaLevels];
    Framebuffer m_forward[kMaxFlowLevels];
    Framebuffer m_backward[kMaxFlowLevels];
    Framebuffer m_smoothForward;
    Framebuffer m_warpField;
    int m_buildWidth;
    int m_buildHeight;
    int m_buildFlowPixels;
    int m_buildStage;
    int m_levelWidths[kMaxLumaLevels];
    int m_levelHeights[kMaxLumaLevels];
    int m_lumaLevels;
    int m_flowFinest;
    int m_flowLevels;
    bool m_flowValid;
    int m_newestIndex;
    int m_capturedCount;
    bool m_fieldReady;
    bool m_pyramidBuilt[2];
    bool m_flowPlanned;
    bool m_cutPending;
    bool m_slotCut[2];
    float m_slotTime[2];
};

}

#endif
