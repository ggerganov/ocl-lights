/*! \file geometry.h
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#pragma once

#include "cg_timer.h"

#include <memory>
#include <map>

namespace Data {
struct Lights;
}

namespace OCL {
class BaseManager;
}

class Geometry {
public:
    Geometry();
    ~Geometry();

    void configure(const std::string & fname);

    void allocate(int sizeX, int sizeY);

    void updateFloorTexture();
    void updateObjectsTexture();
    void updateLights();

    void calcShadowMap();

    void finishOpenCL();

    void renderScene();
    void renderShadowMap();

    void addObjectCircle(double fx, double fy, int r, float val);
    void clear();

    std::shared_ptr<Data::Lights> getLights();
    std::shared_ptr<OCL::BaseManager> getOCLManager();

    int _sizeX = -1;
    int _sizeY = -1;

    int _nLights = 8;
    int _nLightAngles = 512;

private:
    CG::Timer _timer;

    std::shared_ptr<OCL::BaseManager> _oclm;

    struct Data;
    std::shared_ptr<Data> _data;

    struct Texture2D;
    std::map<std::string, Texture2D> _textures;
};


