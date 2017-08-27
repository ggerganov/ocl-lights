/*! \file app.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#include "app.h"

#include "cg_logger.h"
#include "cg_window2d.h"

#include "ui.h"
#include "geometry.h"

#include "imgui/imgui.h"

#include <GLFW/glfw3.h>

constexpr auto kTag = "App"; 

static void error_callback(int error, const char* description) {
    throw CG::Exception("GLFW Error %d: %s\n", error, description);
}

App::App() {
    _ui = std::make_shared<UI>();
    _geometry = std::make_shared<Geometry>();
    _window = std::make_shared<CG::Window2D>();

    _ui->_nLights = _geometry->_nLights;
    _ui->_nLightAngles = _geometry->_nLightAngles;
}

App::~App() {
    _ui.reset();
    _geometry.reset();
    _window.reset();

    glfwTerminate();
}

void App::init() {
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        throw CG::Exception("Error when initializing GLFW!\n");
    }
}

void App::terminate() {
    _ui->terminate();
}

void App::pollEvents() {
    glfwPollEvents();
}

void App::processKeyboard() {
    if (ImGui::GetIO().KeysDown[GLFW_KEY_ESCAPE]) {
        _shouldTerminate = true;
    }

    if (ImGui::IsMouseDown(GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        auto mPos = ImGui::GetMousePos();
        if (_ui->_isFullscreen) {
            _window->toScreenCoordinates(mPos.x, mPos.y);
        } else {
            _window->toViewportCordinates(0, mPos.x, mPos.y);
        }
        mPos.x = 2.0*mPos.x + 1.0;

        int size = 10;
        if (ImGui::GetIO().KeyShift) size = 50;
        //CG_INFO(0, "Mouse pressed at %g %g\n", mPos.x, mPos.y);

        _geometry->addObjectCircle(mPos.x, mPos.y, size, 1.0);
        _geometry->updateObjectsTexture();
    }
    if (ImGui::IsMouseDown(GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        auto mPos = ImGui::GetMousePos();
        if (_ui->_isFullscreen) {
            _window->toScreenCoordinates(mPos.x, mPos.y);
        } else {
            _window->toViewportCordinates(0, mPos.x, mPos.y);
        }
        mPos.x = 2.0*mPos.x + 1.0;

        int size = 10;
        if (ImGui::GetIO().KeyShift) size = 50;
        //CG_INFO(0, "Right Mouse pressed at %g %g\n", mPos.x, mPos.y);

        _geometry->addObjectCircle(mPos.x, mPos.y, size, 0.0);
        _geometry->updateObjectsTexture();
    }
}

void App::updateState() {
    static bool firstCall = true;
    if (firstCall) {
        _ui->setLights(_geometry->getLights());
        _ui->setOCLManager(_geometry->getOCLManager());
        firstCall = false;
    }

    if (_ui->_updateGeometry) {
        CG_IDBG(0, kTag, "Updating geometry\n");
        _geometry->_nLights = _ui->_nLights;
        _geometry->_nLightAngles = _ui->_nLightAngles;
        _geometry->allocate(_ui->_geometrySizeX, _ui->_geometrySizeY);
        _geometry->updateFloorTexture();
        _geometry->updateObjectsTexture();
        _geometry->finishOpenCL();
    }

    if (_ui->_clearGeometry) {
        CG_IDBG(0, kTag, "Clearing geometry\n");
        _geometry->clear();
        _geometry->updateObjectsTexture();
    }

    if (_window->shouldClose()) {
        CG_IDBG(0, kTag, "Main window closed\n");
        _shouldTerminate = true;
    }

    _ui->setViewport(_window);
    _ui->_updateGeometry = false;
    _ui->_clearGeometry = false;
}
