#include "../common.h"

__kernel void drawFloor(
    __write_only image2d_t imgFloor
    ) {
  const uint x_coord = get_global_id(0);
  const uint y_coord = get_global_id(1);

  uchar c = (x_coord % 10 > 8 || y_coord % 10 > 8) ? 187 : 255;
  uchar r = c;
  uchar g = c;
  uchar b = c;

  write_imagef(imgFloor,
      (int2)   (x_coord, y_coord),
      (float4) (r, g, b, 255)/255.f);
}

__kernel void drawObjects(
    __write_only image2d_t   imgObjects,
    __global     TypeObject *objects
    ) {
  const uint x_coord = get_global_id(0);
  const uint y_coord = get_global_id(1);
  const uint width = get_global_size(0);

  uchar r = 255;
  uchar g = 0;
  uchar b = 0;
  uchar a = (objects[y_coord*width + x_coord] > 0.0f) ? 255 : 0;

  write_imagef(imgObjects,
      (int2)   (x_coord, y_coord),
      (float4) (r, g, b, a)/255.f);
}
