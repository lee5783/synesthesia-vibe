//
//  mesh_buffer.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#ifndef __BEATDROP_RENDER_GL_MESH_BUFFER_H__
#define __BEATDROP_RENDER_GL_MESH_BUFFER_H__ 1

#include "vertex_array.h"

namespace render_gl
{

class MeshBuffer
{
public:
    MeshBuffer();

    bool Create();
    void Destroy();

    bool UploadVertices(const void* data, int vertexCount);
    bool UploadIndices(const int* indices, int indexCount);

    void DrawTriangleList(int vertexCount) const;
    void DrawTriangleStrip(int vertexCount) const;
    void DrawTriangleFan(int vertexCount) const;
    void DrawLineList(int vertexCount) const;
    void DrawLineStrip(int firstVertex, int vertexCount) const;
    void DrawPointList(int firstVertex, int vertexCount) const;
    void DrawIndexedTriangles(int indexCount) const;
    void DrawIndexedTriangles(int indexCount, int firstIndex) const;

    bool Valid() const { return m_array.Valid(); }

private:
    VertexArray m_array;
    int m_vertexCapacity;
    int m_indexCapacity;
};

}

#endif
