/*! \file app.h
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#pragma once

#include <memory>

namespace CG {
class Window2D;
}

class UI;
class Geometry;

class App {
public:
    App();
    ~App();

    void init();
    void terminate();

    void pollEvents();
    void processKeyboard();
    void updateState();

    inline std::shared_ptr<UI> getUI() { return _ui; }
    inline std::shared_ptr<Geometry> getGeometry() { return _geometry; }
    inline std::shared_ptr<CG::Window2D> getWindow() { return _window; }

    inline bool shouldTerminate() const { return _shouldTerminate; }

    bool _shouldTerminate = false;

private:
    std::shared_ptr<UI> _ui;
    std::shared_ptr<Geometry> _geometry;
    std::shared_ptr<CG::Window2D> _window;
};
