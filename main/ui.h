/*! \file ui.h
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#pragma once

#include <memory>

namespace OCL {
class BaseManager;
}

namespace CG {
class Window2D;
}

namespace Data {
struct Light;
struct Lights;
}

class UI {
public:
    void init(std::shared_ptr<CG::Window2D> window, bool setCallbacks);
    void render();
    void terminate();

    void setViewport(std::shared_ptr<CG::Window2D> window);
    void setLights(std::shared_ptr<Data::Lights> lights);
    void setOCLManager(std::shared_ptr<OCL::BaseManager> oclm);

    bool _updateGeometry = false;
    bool _clearGeometry = false;
    bool _isFullscreen = false;

    int _geometrySizeX = 1024;
    int _geometrySizeY = 1024;

    int _nLights = -1;
    int _nLightAngles = -1;

private:
    const float _windowHeader = 20.0f;

    void renderWindowViewport();
    void renderWindowControls();
    void renderWindowKernelEdit();

    void renderLightProperties(Data::Light & light);

    std::weak_ptr<Data::Lights> _lights;
    std::weak_ptr<OCL::BaseManager> _oclm;

    float _viewportX0 = 0.0f;
    float _viewportY0 = 0.0f;
    float _viewportDx = 0.0f;
    float _viewportDy = 0.0f;
};
