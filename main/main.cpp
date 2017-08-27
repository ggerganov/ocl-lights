/*! \file main.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#include "cg_logger.h"
#include "cg_timer.h"
#include "cg_config.h"
#include "cg_window2d.h"

#include "cg_opencl/oclCommon.h"
#include "cg_opencl/oclProfiler.h"

#include "app.h"
#include "ui.h"
#include "geometry.h"

int main(int argc, char **argv) {
    CG::Logger::getInstance().configure("data/logger.cfg", "Logger");

    CG_INFO(0, "Usage: %s\n", argv[0]);
    if (argc < 1) {
        return -1;
    }

    OCL_PROFILING_SET_PARAMETERS("kernel_ALL", "", 1, 0)
    OCL_PROFILING_SET_PARAMETERS("kernel_drawFloor", "", 1, 0)
    OCL_PROFILING_SET_PARAMETERS("kernel_drawObjects", "", 1, 0)
    OCL_PROFILING_SET_PARAMETERS("kernel_calcDistance", "", 1, 0)
    OCL_PROFILING_SET_PARAMETERS("kernel_calcDistance2", "", 1, 0)
    OCL_PROFILING_SET_PARAMETERS("kernel_calcShadowMap", "", 1, 0)
    OCL_PROFILING_SET_PARAMETERS("kernel_calcShadowMap2", "", 1, 0)

    OCL_PROFILING_SET_PARAMETERS("oclBuffer_read_ALL", "", 1, 0)
    OCL_PROFILING_SET_PARAMETERS("oclBuffer_write_ALL", "", 1, 0)
    OCL_PROFILING_SET_PARAMETERS("oclBuffer_write_lights", "", 1, 0)
    OCL_PROFILING_SET_PARAMETERS("oclBuffer_fillFloat_ALL", "", 1, 0)

    int nFrames = 0;
    double curFPS = 0.0;
    CG::Timer timerFPS; timerFPS.start();

    try {
        App app;
        app.init();

        app.getWindow()->initialize(1440, 1200, "Lights", true);
        app.getWindow()->makeContextActive();
        app.getWindow()->setSwapInterval(0);

        app.getUI()->init(app.getWindow(), true);

        app.getGeometry()->configure("");
        app.getGeometry()->allocate(1024, 1024);
        app.getGeometry()->updateFloorTexture();
        app.getGeometry()->updateObjectsTexture();
        app.getGeometry()->finishOpenCL();

        while (true) {
            app.pollEvents();
            app.processKeyboard();
            app.updateState();
            if (app.shouldTerminate()) break;

            app.getWindow()->makeContextActive();
            if (app.getWindow()->wasWindowSizeChanged()) {
                app.getWindow()->updateWindowSize();
            }

            app.getWindow()->render();

            app.getGeometry()->updateLights();
            app.getGeometry()->calcShadowMap();
            app.getGeometry()->finishOpenCL();

            if (!app.getUI()->_isFullscreen) app.getWindow()->applyViewport(0);
            app.getGeometry()->renderScene();
            app.getGeometry()->renderShadowMap();

            app.getUI()->render();

            app.getWindow()->swapBuffers();

            nFrames++;
            if (timerFPS.time() > 3.0) {
                curFPS = 1.0/(timerFPS.time()/nFrames);
                CG_INFO(0, "FPS = %g\n", curFPS);
                timerFPS.start();
                nFrames = 0;

                OCL_PROFILING_PRINT_ALL()
                OCL_PROFILING_RESET_ALL()
            }
        }

        app.terminate();
    } catch (CG::Exception &ex) {
        CG_FATAL(0, "%s\n", ex._info);
    } catch (OCL::Exception &ex) {
        CG_FATAL(0, "%s\n", ex._info);
    }

    return 0;
}
