/*! \file geometry.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#include "geometry.h"

#include "data.h"

#include "cg_opencl/oclBaseManager.h"

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#ifdef _WIN32
  #include <windows.h>
#endif
#include <GL/gl.h>
#endif

#include <cmath>

struct Geometry::Texture2D {
    Texture2D() {}
    Texture2D(int sizeX, int sizeY) { setDimensions(sizeX, sizeY); }

    ~Texture2D() {}

    void setDimensions(int sizeX, int sizeY) {
        _sizeX = sizeX;
        _sizeY = sizeY;
    }

    int _sizeX = -1;
    int _sizeY = -1;

    GLsizei id = 0;
    GLuint glid = 0;
};

struct Geometry::Data {
    Data() {
        _objects = std::make_shared<::Data::Objects>();
        _lights = std::make_shared<::Data::Lights>();
        _lightDistance = std::make_shared<::Data::LightDistance>();
    }

    std::shared_ptr<::Data::Objects>        _objects;
    std::shared_ptr<::Data::Lights>         _lights;
    std::shared_ptr<::Data::LightDistance>  _lightDistance;
};

Geometry::Geometry() {
    _oclm = std::make_shared<OCL::BaseManager>();

    _data = std::make_shared<Geometry::Data>();
}

Geometry::~Geometry() {
}

void Geometry::configure(const std::string & fname) {
    _oclm->configure(fname);
}

void Geometry::allocate(int sizeX, int sizeY) {
    _sizeX = sizeX;
    _sizeY = sizeY;

    _data->_objects->resize(_sizeX*_sizeY, 0.0f);

    _oclm->allocateOpenCLBuffer("objects",
            (OCL::BaseManager::CLFlags) (OCL::BaseManager::CLFlags::MEM_READ_WRITE | OCL::BaseManager::CLFlags::MEM_COPY_HOST_PTR),
            _sizeX*sizeY*sizeof(CLIF::TypeObject), _data->_objects->data());

    _data->_lights->resize(_nLights);
    for (int l = 0; l < _nLights; ++l) {
        _data->_lights->at(l).color = { {1.0f, 1.0f, 1.0f, 1.0f} };
        _data->_lights->at(l).dir = { {1.0, 0.0} };
        _data->_lights->at(l).x0 = 0.0;
        _data->_lights->at(l).y0 = 0.0;
        _data->_lights->at(l).size = (rand()%200)*0.0001 + 0.001;
        _data->_lights->at(l).falloff = 0.3;
        _data->_lights->at(l).ang = 2.0*M_PI;
        _data->_lights->at(l).intensity = 0.25;
        _data->_lights->at(l).active = true;
    }

    _oclm->allocateOpenCLBuffer("lights",
            (OCL::BaseManager::CLFlags) (OCL::BaseManager::CLFlags::MEM_READ_WRITE | OCL::BaseManager::CLFlags::MEM_COPY_HOST_PTR),
            _nLights*sizeof(CLIF::TypeLight2D), _data->_lights->data());

    _data->_lightDistance->resize(_nLights*_nLightAngles, 0.0f);

    _oclm->allocateOpenCLBuffer("lightDistance",
            (OCL::BaseManager::CLFlags) (OCL::BaseManager::CLFlags::MEM_READ_WRITE | OCL::BaseManager::CLFlags::MEM_COPY_HOST_PTR),
            _nLights*_nLightAngles*sizeof(cl_float), _data->_lightDistance->data());

    {
        Texture2D &t = _textures["tex_floor"];
        if (t.glid) { glDeleteTextures(1, &t.glid); t.glid = 0; }
        t.setDimensions(_sizeX, _sizeY);

        glGenTextures(1, &t.glid);
        glBindTexture(GL_TEXTURE_2D, t.glid);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _sizeX, _sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        _oclm->allocateOpenCLTexture2D("tex_floor", (OCL::BaseManager::CLFlags) OCL::BaseManager::CLFlags::MEM_READ_WRITE, t.glid);
    }

    {
        Texture2D &t = _textures["tex_data"];
        if (t.glid) { glDeleteTextures(1, &t.glid); t.glid = 0; }
        t.setDimensions(_sizeX, _sizeY);

        glGenTextures(1, &t.glid);
        glBindTexture(GL_TEXTURE_2D, t.glid);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _sizeX, _sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        _oclm->allocateOpenCLTexture2D("tex_data", (OCL::BaseManager::CLFlags) (OCL::BaseManager::CLFlags::MEM_WRITE), t.glid);
    }

    {
        Texture2D &t = _textures["tex_shadowmap"];
        if (t.glid) { glDeleteTextures(1, &t.glid); t.glid = 0; }
        t.setDimensions(_sizeX/4, _sizeY/4);

        glGenTextures(1, &t.glid);
        glBindTexture(GL_TEXTURE_2D, t.glid);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t._sizeX, t._sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        _oclm->allocateOpenCLTexture2D("tex_shadowmap", (OCL::BaseManager::CLFlags) OCL::BaseManager::CLFlags::MEM_WRITE, t.glid);
    }
}

void Geometry::updateFloorTexture() {
    _oclm->setKernelArgAsBuffer("drawFloor", 0, "tex_floor");

    _oclm->acquireGLObject("tex_floor");
    _oclm->runKernel2D("drawFloor", _sizeX, _sizeY, 1, 1);
    _oclm->releaseGLObject("tex_floor");
}

void Geometry::updateObjectsTexture() {
    _oclm->writeBuffer("objects", CL_FALSE, _sizeX*_sizeY*sizeof(CLIF::TypeObject), _data->_objects->data());

    _oclm->setKernelArgAsBuffer("drawObjects", 0, "tex_data");
    _oclm->setKernelArgAsBuffer("drawObjects", 1, "objects");

    _oclm->acquireGLObject("tex_data");
    _oclm->runKernel2D("drawObjects", _sizeX, _sizeY, 1, 1);
    _oclm->releaseGLObject("tex_data");
}

void Geometry::updateLights() {
    float t = _timer.time()*0.1;
    for (int l = 1; l <= _nLights; ++l) {
        _data->_lights->at(l-1).x0 = 0.5*sin(t*l + l);
        _data->_lights->at(l-1).y0 = 0.8*cos(t*0.5*l + l);
    }

    _oclm->writeBuffer("lights", CL_TRUE, _nLights*sizeof(CLIF::TypeLight2D), _data->_lights->data());
}

void Geometry::calcShadowMap() {
    _oclm->fillBufferFloat("lightDistance", 100.0f, _nLights*_nLightAngles);
    //clFinish(_oclQueue);

    cl_uint nx = _sizeX;
    cl_uint ny = _sizeY;

    _oclm->setKernelArgAsBuffer("calcDistance2", 0, "objects");
    _oclm->setKernelArgAsBuffer("calcDistance2", 1, "lights");
    _oclm->setKernelArgAsBuffer("calcDistance2", 2, "lightDistance");
    _oclm->setKernelArg("calcDistance2", 3, sizeof(cl_int),  &_nLights);
    _oclm->setKernelArg("calcDistance2", 4, sizeof(cl_int),  &_nLightAngles);
    _oclm->setKernelArg("calcDistance2", 5, sizeof(cl_uint), &nx);
    _oclm->setKernelArg("calcDistance2", 6, sizeof(cl_uint), &ny);
    _oclm->runKernelSelected("calcDistance2");

    _oclm->acquireGLObject("tex_shadowmap");

    nx = _textures["tex_shadowmap"]._sizeX;
    ny = _textures["tex_shadowmap"]._sizeY;
    _oclm->setKernelArgAsBuffer("calcShadowMap2", 0, "tex_shadowmap");
    _oclm->setKernelArgAsBuffer("calcShadowMap2", 1, "lights");
    _oclm->setKernelArgAsBuffer("calcShadowMap2", 2, "lightDistance");
    _oclm->setKernelArg("calcShadowMap2", 3, sizeof(cl_int),  &_nLights);
    _oclm->setKernelArg("calcShadowMap2", 4, sizeof(cl_int),  &_nLightAngles);
    _oclm->setKernelArg("calcShadowMap2", 5, sizeof(cl_uint), &nx);
    _oclm->setKernelArg("calcShadowMap2", 6, sizeof(cl_uint), &ny);
    _oclm->runKernelSelected("calcShadowMap2");

    _oclm->releaseGLObject("tex_shadowmap");
}

void Geometry::finishOpenCL() { _oclm->finish(); }

void Geometry::renderScene() {
    glClearColor(0.1, 0.15, 0.20, 0.1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _textures["tex_floor"].glid);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(0, 1); glVertex2f(-1.0f,  1.0f);
    glTexCoord2f(1, 1); glVertex2f( 0.0f,  1.0f);
    glTexCoord2f(1, 0); glVertex2f( 0.0f, -1.0f);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, _textures["tex_data"].glid);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(0, 1); glVertex2f(-1.0f,  1.0f);
    glTexCoord2f(1, 1); glVertex2f( 0.0f,  1.0f);
    glTexCoord2f(1, 0); glVertex2f( 0.0f, -1.0f);
    glEnd();

    glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, _textures["tex_shadowmap"].glid);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(0, 1); glVertex2f(-1.0f,  1.0f);
    glTexCoord2f(1, 1); glVertex2f( 0.0f,  1.0f);
    glTexCoord2f(1, 0); glVertex2f( 0.0f, -1.0f);
    glEnd();
}

void Geometry::renderShadowMap() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, _textures["tex_shadowmap"].glid);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-0.0f, -1.0f);
    glTexCoord2f(0, 1); glVertex2f(-0.0f,  1.0f);
    glTexCoord2f(1, 1); glVertex2f( 1.0f,  1.0f);
    glTexCoord2f(1, 0); glVertex2f( 1.0f, -1.0f);
    glEnd();
}

void Geometry::addObjectCircle(double fx, double fy, int r, float val) {
    int x = 0.5*(fx + 1.0)*_sizeX;
    int y = 0.5*(fy + 1.0)*_sizeY;

    auto & objects = *_data->_objects;
    for (int iy = -r; iy <= r; ++iy) {
        for (int ix = -r; ix <= r; ++ix) {
            if ((x+ix < 0) || (x+ix >= _sizeX)) continue;
            if ((y+iy < 0) || (y+iy >= _sizeY)) continue;
            if (ix*ix + iy*iy > r*r) continue;
            objects[(y+iy)*_sizeX + (x+ix)] = val;
        }
    }
}

void Geometry::clear() {
    auto & objects = *_data->_objects;
    for (int i = 0; i < _sizeX*_sizeY; ++i) objects[i] = 0;
}

std::shared_ptr<Data::Lights> Geometry::getLights() {
    return _data->_lights;
}

std::shared_ptr<OCL::BaseManager> Geometry::getOCLManager() {
    return _oclm;
}
