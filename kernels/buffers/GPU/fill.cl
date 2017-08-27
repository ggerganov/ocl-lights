#include "../../utils.h"

__kernel void buffers_fill_float(
        __global float *arr,
                 uint   narr,
                 float  val
        ) {
    GET_WORK_DOMAIN(narr);

    for (; id < idmax; id += lsize) {
        arr[id] = val;
    }
}

__kernel void render_arr_to_texture_uchar(
        __write_only image2d_t  tex,
        __global     uchar     *arr,
                     uint       nx,
                     uint       ny
        ) {
    int2 texSize = get_image_dim(tex);

    GET_WORK_DOMAIN(texSize.x*texSize.y);

    for (; id < idmax; id += lsize) {
        GET_XY(id, texSize, ix, iy);

        // nearest neighbour
        int ax = (int)((((float)(ix))/texSize.x)*nx);
        int ay = (int)((((float)(iy))/texSize.y)*ny);

        float c = (arr[ay*nx + ax])/255.0f;
        write_imagef(tex, (int2) (ix, iy), (float4) (c, c, c, 1.0f));
    }
}

__kernel void render_arr_to_texture_float(
        __write_only image2d_t  tex,
        __global     float     *arr,
                     uint       nx,
                     uint       ny
        ) {
    int2 texSize = get_image_dim(tex);

    GET_WORK_DOMAIN(texSize.x*texSize.y);

    for (; id < idmax; id += lsize) {
        GET_XY(id, texSize, ix, iy);

        // nearest neighbour
        int ax = (int)((((float)(ix))/texSize.x)*nx);
        int ay = (int)((((float)(iy))/texSize.y)*ny);

        float c = (arr[ay*nx + ax]);
        write_imagef(tex, (int2) (ix, iy), (float4) (c, c, c, 1.0f));
    }
}

__kernel void render_arr_to_texture_float4(
        __write_only image2d_t  tex,
        __global     float4    *arr,
                     uint       nx,
                     uint       ny
        ) {
    int2 texSize = get_image_dim(tex);

    GET_WORK_DOMAIN(texSize.x*texSize.y);

    for (; id < idmax; id += lsize) {
        GET_XY(id, texSize, ix, iy);

        // nearest neighbour
        int ax = (int)((((float)(ix))/texSize.x)*nx);
        int ay = (int)((((float)(iy))/texSize.y)*ny);

        write_imagef(tex, (int2) (ix, iy), arr[ay*nx + ax]);
    }
}
