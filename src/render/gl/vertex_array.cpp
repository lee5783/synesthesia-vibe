//
//  vertex_array.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include "vertex_array.h"
#include "gl_error.h"
#include "gl_object_counts.h"
#include "gl_headers.h"

namespace render_gl
{

VertexArray::VertexArray()
    : m_vao(0)
    , m_vbo(0)
    , m_ibo(0)
{
}

VertexArray::~VertexArray()
{
    Destroy();
}

bool VertexArray::Create()
{
    Destroy();

    glGenVertexArrays(1, &m_vao);
    NoteGlObjectCreated(GlObjectKind::VertexArray);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ibo);
    NoteGlObjectCreated(GlObjectKind::Buffer, 2);

    if (!CheckGlError("VertexArray::Create"))
    {
        Destroy();
        return false;
    }

    Bind();
    return true;
}

void VertexArray::Destroy()
{
    if (m_ibo != 0)
    {
        glDeleteBuffers(1, &m_ibo);
        NoteGlObjectDeleted(GlObjectKind::Buffer);
        m_ibo = 0;
    }

    if (m_vbo != 0)
    {
        glDeleteBuffers(1, &m_vbo);
        NoteGlObjectDeleted(GlObjectKind::Buffer);
        m_vbo = 0;
    }

    if (m_vao != 0)
    {
        glDeleteVertexArrays(1, &m_vao);
        NoteGlObjectDeleted(GlObjectKind::VertexArray);
        m_vao = 0;
    }
}

bool VertexArray::UploadVertices(const void* data, int byteCount, unsigned int usage)
{
    if (m_vbo == 0)
        return false;

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, byteCount, data, usage);

    return CheckGlError("VertexArray::UploadVertices");
}

bool VertexArray::UploadIndices(const void* data, int byteCount, unsigned int usage)
{
    if (m_ibo == 0)
        return false;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, byteCount, data, usage);

    return CheckGlError("VertexArray::UploadIndices");
}

void VertexArray::Bind() const
{
    glBindVertexArray(m_vao);
}

}