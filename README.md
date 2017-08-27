# ocl-lights
Pixel-perfect 2D shadows on the GPU

<a href="http://www.youtube.com/watch?feature=player_embedded&v=m67TR-vd6lE" target="_blank"><img src="http://img.youtube.com/vi/m67TR-vd6lE/0.jpg" alt="CG++ Lights" width="360" height="270" border="10" /></a>

Calculated shadowmap for each frame is on the right. The shadowmap is computed on the GPU using OpenCL.

## Building & running

    git clone https://github.com/ggerganov/ocl-lights
    cd ocl-lights
    git submodule update --init
    make
    ./bin/ocl-lights
   
## Dependencies

- OpenGL
- OpenCL
- GLFW3
