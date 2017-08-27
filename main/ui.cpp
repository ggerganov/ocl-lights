/*! \file ui.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#include "ui.h"

#include "cg_window2d.h"

#include "cg_opencl/oclBaseManager.h"

#include "data.h"

#include "imgui/imgui.h"
#include "imgui/examples/opengl2_example/imgui_impl_glfw.h"

void UI::init(std::shared_ptr<CG::Window2D> window, bool setCallbacks) {
    ImGui_ImplGlfw_Init(window->getGLFWWindow(), setCallbacks);
}

void UI::render() {
    ImGui_ImplGlfw_NewFrame();

    renderWindowViewport();
    renderWindowControls();
    renderWindowKernelEdit();

    ImGui::Render();
}

void UI::terminate() {
    ImGui_ImplGlfw_Shutdown();
}

void UI::setViewport(std::shared_ptr<CG::Window2D> window) {
    window->setViewport(0, _viewportX0, _viewportY0, _viewportDx, _viewportDy);
}

void UI::setLights(std::shared_ptr<Data::Lights> lights) {
    _lights = lights;
}

void UI::setOCLManager(std::shared_ptr<OCL::BaseManager> oclm) {
    _oclm = oclm;
}

void UI::renderWindowViewport() {
    if (!_isFullscreen) {
        static bool isMovable = false;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::SetNextWindowPos(ImVec2(350, 7), ImGuiSetCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1085, 645), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("Viewport", &_isFullscreen, (isMovable) ? 0 : ImGuiWindowFlags_NoMove);
        auto wPos = ImGui::GetWindowPos();
        isMovable = (ImGui::GetMousePos().y - wPos.y < _windowHeader);
        _viewportX0 = wPos.x;
        _viewportY0 = wPos.y + _windowHeader;
        _viewportDx = ImGui::GetWindowWidth();
        _viewportDy = ImGui::GetWindowHeight() - _windowHeader;
        ImGui::End();
        ImGui::PopStyleColor(1);
    }
}

void UI::renderWindowControls() {
    ImGui::SetNextWindowPos(ImVec2(7, 7), ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(334, 645), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("Controls");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    if (ImGui::SliderInt("Gridsize X", &_geometrySizeX, 128, 4096)) { _geometrySizeY = _geometrySizeX; }
    if (ImGui::SliderInt("Gridsize Y", &_geometrySizeY, 128, 4096)) { _geometrySizeX = _geometrySizeY; }
    ImGui::SliderInt("Lights", &_nLights, 0, 32);
    ImGui::SliderInt("Light angles", &_nLightAngles, 16, 2048);
    if (ImGui::Button("Update")) { _updateGeometry = true; } ImGui::SameLine();
    if (ImGui::Button("Clear")) { _clearGeometry = true; }

    if (ImGui::Checkbox("Fullscreen", &_isFullscreen)) {}

    if (auto lights = _lights.lock()) {
        if (ImGui::CollapsingHeader("Lights##lights_properties", 0, true, true)) {
            for (size_t i = 0; i < lights->size(); ++i) {
                ImGui::SetNextTreeNodeOpen(true, ImGuiSetCond_FirstUseEver);
                if (ImGui::TreeNode((void*)(intptr_t)i, "Light %ld", i)) {
                    ImGui::PushID(i);
                    renderLightProperties(lights->at(i));
                    ImGui::PopID();
                    ImGui::TreePop();
                }
            }
        }
    }

    ImGui::End();
}

void UI::renderWindowKernelEdit() {
    ImGui::SetNextWindowPos(ImVec2(7, 655), ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1400, 400), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("CL Kernel Edit");
    if (auto oclm = _oclm.lock()) {
        const auto & kernelTree = oclm->getKernelTree();
        for (const auto & kFile : kernelTree) {
            ImGui::SetNextTreeNodeOpen(true, ImGuiSetCond_FirstUseEver);
            if (ImGui::TreeNode(kFile.first.c_str())) {
                for (const auto & kName : kFile.second) {
                    ImGui::Text("%s, %d",
                            kName.second.c_str(),
                            oclm->getKernel(kName.second).getSelectedWorkgroups());
                }
                ImGui::TreePop();
            }
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No CL Manager available!");
    }
    ImGui::End();
}

void UI::renderLightProperties(Data::Light & light) {
    ImGui::Text("Pos: %.4f %.4f", light.x0, light.y0);
    ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 1.0f);
}
