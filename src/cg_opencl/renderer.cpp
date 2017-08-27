/*! \file renderer.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#include "cg_opencl/renderer.h"

#include <GLFW/glfw3.h>
#ifdef __APPLE__
    #include <OpenGL/gl.h>
#endif

#include <map>
#include <string>

namespace OCL {

namespace RendererCLGLStatic {
class Texture2D {
public:
    Texture2D() {}
    Texture2D(int sizeX, int sizeY) { setDimensions(sizeX, sizeY); }

    ~Texture2D() {}

    void setDimensions(int sizeX, int sizeY) {
        _sizeX = sizeX;
        _sizeY = sizeY;
    }

    int _sizeX = -1;
    int _sizeY = -1;

    GLuint glid = 0;
};
}

using RendererCLGLStatic::Texture2D;

class RendererCLGL::ContainerTexture2D :
    public std::map<std::string, Texture2D> {
public:
    ContainerTexture2D() {};
};

RendererCLGL::RendererCLGL() {
    _textures2D = new ContainerTexture2D();
}

RendererCLGL::~RendererCLGL() {
    if (_textures2D) delete  _textures2D;
}

void RendererCLGL::allocateTexture2DCLGL(
    const std::string &texName,
    int32_t sizeX,
    int32_t sizeY,
    uint32_t type) {
    Texture2D &t = (*_textures2D)[texName];
    if (t.glid) { glDeleteTextures(1, &t.glid); t.glid = 0; }
    t.setDimensions(sizeX, sizeY);

    glGenTextures(1, &t.glid);
    glBindTexture(GL_TEXTURE_2D, t.glid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, type, t._sizeX, t._sizeY, 0, type, GL_UNSIGNED_BYTE, 0);

    allocateOpenCLTexture2D(texName, (CLFlags) MEM_WRITE, t.glid);
}

void RendererCLGL::bindTexture2D(const std::string &texName) {
    Texture2D &t = (*_textures2D)[texName];
    glBindTexture(GL_TEXTURE_2D, t.glid);
}

void RendererCLGL::finishOpenCL() {
    finish();
}

void RendererCLGL::render() {
}
}
