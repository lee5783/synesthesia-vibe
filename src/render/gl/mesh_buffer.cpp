//
//  mesh_buffer.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#include "mesh_buffer.h"
#include "gl_error.h"
#include "gl_headers.h"
#include "mesh.h"

#include <cstddef>

namespace render_gl
{

namespace
{

constexpr int kVertexStride = static_cast<int>(sizeof(render_core::Vertex));
constexpr size_t kPositionOffset = offsetof(render_core::Vertex, x);
constexpr size_t kColorOffset = offsetof(render_core::Vertex, Diffuse);
constexpr size_t kUvOffset = offsetof(render_core::Vertex, tu);
constexpr size_t kUvOrigOffset = offsetof(render_core::Vertex, tu_orig);
constexpr size_t kRadAngOffset = offsetof(render_core::Vertex, rad);

#if defined(SYNESTHESIA_VIBE_GLES)
constexpr int kColorComponents = 4;
#else
constexpr int kColorComponents = GL_BGRA;
#endif

}

MeshBuffer::MeshBuffer()
    : m_vertexCapacity(0)
    , m_indexCapacity(0)
{
}

bool MeshBuffer::Create()
{
    Destroy();

    if (!m_array.Create())
        return false;

    m_array.UploadVertices(nullptr, 0, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kVertexStride, reinterpret_cast<const void*>(kPositionOffset));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, kColorComponents, GL_UNSIGNED_BYTE, GL_TRUE, kVertexStride, reinterpret_cast<const void*>(kColorOffset));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, kVertexStride, reinterpret_cast<const void*>(kUvOffset));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, kVertexStride, reinterpret_cast<const void*>(kUvOrigOffset));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, kVertexStride, reinterpret_cast<const void*>(kRadAngOffset));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_array.IndexBufferId());

    if (!CheckGlError("MeshBuffer::Create"))
    {
        Destroy();
        return false;
    }

    return true;
}

void MeshBuffer::Destroy()
{
    m_array.Destroy();
    m_vertexCapacity = 0;
    m_indexCapacity = 0;
}

bool MeshBuffer::UploadVertices(const void* data, int vertexCount)
{
    if (!m_array.Valid() || vertexCount < 0)
        return false;

    m_array.Bind();
    glBindBuffer(GL_ARRAY_BUFFER, m_array.VertexBufferId());

    if (vertexCount > m_vertexCapacity)
    {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertexCount) * kVertexStride, data, GL_DYNAMIC_DRAW);
        m_vertexCapacity = vertexCount;
    }
    else
    {
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertexCount) * kVertexStride, data);
    }

    return CheckGlError("MeshBuffer::UploadVertices");
}

bool MeshBuffer::UploadIndices(const int* indices, int indexCount)
{
    if (!m_array.Valid() || indexCount < 0)
        return false;

    m_array.Bind();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_array.IndexBufferId());
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indexCount) * static_cast<GLsizeiptr>(sizeof(int)), indices, GL_STATIC_DRAW);
    m_indexCapacity = indexCount;

    return CheckGlError("MeshBuffer::UploadIndices");
}

void MeshBuffer::DrawTriangleList(int vertexCount) const
{
    m_array.Bind();
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}

void MeshBuffer::DrawTriangleStrip(int vertexCount) const
{
    m_array.Bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, vertexCount);
}

void MeshBuffer::DrawTriangleFan(int vertexCount) const
{
    m_array.Bind();
    glDrawArrays(GL_TRIANGLE_FAN, 0, vertexCount);
}

void MeshBuffer::DrawLineList(int vertexCount) const
{
    m_array.Bind();
    glDrawArrays(GL_LINES, 0, vertexCount);
}

void MeshBuffer::DrawLineStrip(int firstVertex, int vertexCount) const
{
    m_array.Bind();
    glDrawArrays(GL_LINE_STRIP, firstVertex, vertexCount);
}

void MeshBuffer::DrawPointList(int firstVertex, int vertexCount) const
{
    m_array.Bind();
    glDrawArrays(GL_POINTS, firstVertex, vertexCount);
}

void MeshBuffer::DrawIndexedTriangles(int indexCount) const
{
    DrawIndexedTriangles(indexCount, 0);
}

void MeshBuffer::DrawIndexedTriangles(int indexCount, int firstIndex) const
{
    if (indexCount < 0 || firstIndex < 0 || firstIndex + indexCount > m_indexCapacity)
        return;

    m_array.Bind();
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT,
        reinterpret_cast<const void*>(static_cast<size_t>(firstIndex) * sizeof(int)));
}

}
