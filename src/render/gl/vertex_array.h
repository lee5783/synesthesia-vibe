//
//  vertex_array.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#ifndef __BEATDROP_RENDER_GL_VERTEX_ARRAY_H__
#define __BEATDROP_RENDER_GL_VERTEX_ARRAY_H__ 1

namespace render_gl
{

class VertexArray
{
public:
    VertexArray();
    ~VertexArray();

    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    bool Create();
    void Destroy();

    bool UploadVertices(const void* data, int byteCount, unsigned int usage);
    bool UploadIndices(const void* data, int byteCount, unsigned int usage);

    void Bind() const;

    unsigned int Id() const { return m_vao; }
    unsigned int VertexBufferId() const { return m_vbo; }
    unsigned int IndexBufferId() const { return m_ibo; }
    bool Valid() const { return m_vao != 0; }

private:
    unsigned int m_vao;
    unsigned int m_vbo;
    unsigned int m_ibo;
};

}

#endif