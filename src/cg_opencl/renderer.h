/*! \file renderer.h
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#pragma once

#include "cg_opencl/oclBaseManager.h"

namespace OCL {

class RendererCLGL : public OCL::BaseManager {
public:
    RendererCLGL();
    ~RendererCLGL();

    virtual void allocateTexture2DCLGL(
        const std::string &texName,
        int32_t sizeX,
        int32_t sizeY,
        uint32_t type);

    virtual void bindTexture2D(const std::string &texName);

    void finishOpenCL();

    virtual void render();

private:
    class ContainerTexture2D;

    ContainerTexture2D *_textures2D;
};

}
