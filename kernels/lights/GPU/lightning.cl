#include "../common.h"

void atomic_min_global(volatile global float *source, const float operand);

void atomic_min_global(volatile global float *source, const float operand) {
  union {
    unsigned int intVal;
    float floatVal;
  } newVal;
  union {
    unsigned int intVal;
    float floatVal;
  } prevVal;

  do {
    prevVal.floatVal = *source;
    newVal.floatVal = min(prevVal.floatVal, operand);
  } while (atomic_cmpxchg((volatile global unsigned int *)source, prevVal.intVal, newVal.intVal) != prevVal.intVal);
}

__kernel void calcDistance(
    __global   TypeObject  *objects,
    __constant TypeLight2D *lights,
    __global   float       *lightDistance,
               int          nLights,
               int          nLightAngles
    ) {
  const uint x_coord = get_global_id(0);
  const uint y_coord = get_global_id(1);

  const uint width = get_global_size(0);
  const uint height = get_global_size(1);

  if (objects[y_coord*width + x_coord] < 0.5f) return;

  float fxmin = 2.0f*((float)(x_coord) + 0.0f)/width  - 1.0f;
  float fymin = 2.0f*((float)(y_coord) + 0.0f)/height - 1.0f;
  float fxmax = 2.0f*((float)(x_coord) + 1.0f)/width  - 1.0f;
  float fymax = 2.0f*((float)(y_coord) + 1.0f)/height - 1.0f;

  for (int l = 0; l < nLights; ++l) {
    float dx, dy, ang, dist = 0.0f;
    int iang1, iang2, iang3, iang4;

    dx = fxmin - lights[l].x0;
    dy = fymin - lights[l].y0;
    ang = atan2(dy, dx) + M_PI_F;
    iang1 = (ang/(2.0f*M_PI_F))*nLightAngles;
    dist += (dx*dx + dy*dy);

    dx = fxmax - lights[l].x0;
    dy = fymin - lights[l].y0;
    ang = atan2(dy, dx) + M_PI_F;
    iang2 = (ang/(2.0f*M_PI_F))*nLightAngles;
    dist += (dx*dx + dy*dy);

    dx = fxmin - lights[l].x0;
    dy = fymax - lights[l].y0;
    ang = atan2(dy, dx) + M_PI_F;
    iang3 = (ang/(2.0f*M_PI_F))*nLightAngles;
    dist += (dx*dx + dy*dy);

    dx = fxmax - lights[l].x0;
    dy = fymax - lights[l].y0;
    ang = atan2(dy, dx) + M_PI_F;
    iang4 = (ang/(2.0f*M_PI_F))*nLightAngles;
    dist += (dx*dx + dy*dy);

    dist *= 0.25f;
    int imin = min(min(min(iang1, iang2), iang3), iang4);
    int imax = max(max(max(iang1, iang2), iang3), iang4);
    if ((imax - imin) > nLightAngles/2) { imax = imin; imin = 0; }
    for (int i = imin; i <= imax; ++i) {
      atomic_min_global(lightDistance + l*nLightAngles + i, dist);
    }
  }
}

__kernel void calcDistance2(
    __global   TypeObject  *objects,
    __constant TypeLight2D *lights,
    __global   float       *lightDistance,
               int          nLights,
               int          nLightAngles,
               uint         sizeX,
               uint         sizeY
    ) {
  const uint lid = get_local_id(0);
  const uint gid = get_group_id(0);

  const uint lsize = get_local_size(0);
  const uint ngrps = get_num_groups(0);

  uint nPerGroup = (sizeX*sizeY + ngrps - 1)/ngrps;

  uint id = mad24(gid, nPerGroup, lid);
  uint idmax = min(mul24((gid+1), nPerGroup), sizeX*sizeY);

  for (; id < idmax; id += lsize) {
    uint x_coord = id;
    uint y_coord = x_coord/sizeX; x_coord -= mul24(y_coord, sizeX);

    if (x_coord == 0 || x_coord == sizeX - 1) continue;
    if (y_coord == 0 || y_coord == sizeY - 1) continue;
    if (objects[y_coord*sizeX + x_coord] < 0.5f) continue;
    if (
      objects[(y_coord-1)*sizeX + (x_coord)] > 0.5f &&
      objects[(y_coord+1)*sizeX + (x_coord)] > 0.5f &&
      objects[(y_coord)*sizeX   + (x_coord-1)] > 0.5f &&
      objects[(y_coord)*sizeX   + (x_coord+1)] > 0.5f
      ) continue;

    float fxmin = 2.0f*((float)(x_coord) + 0.0f)/sizeX - 1.0f;
    float fymin = 2.0f*((float)(y_coord) + 0.0f)/sizeY - 1.0f;
    float fxmax = 2.0f*((float)(x_coord) + 1.0f)/sizeX - 1.0f;
    float fymax = 2.0f*((float)(y_coord) + 1.0f)/sizeY - 1.0f;

    for (int l = 0; l < nLights; ++l) {
      float dx, dy, ang, dist = 0.0f;
      int iang, imin = nLightAngles, imax = 0;

      dx = fxmin - lights[l].x0;
      dy = fymin - lights[l].y0;
      ang = atan2pi(dy, dx) + 1.0f;
      iang = 0.5f*ang*nLightAngles;
      dist += (dx*dx + dy*dy);
      imin = iang;
      imax = iang;

      dx = fxmax - lights[l].x0;
      dy = fymin - lights[l].y0;
      ang = atan2pi(dy, dx) + 1.0f;
      iang = 0.5f*ang*nLightAngles;
      dist += (dx*dx + dy*dy);
      imin = min(imin, iang);
      imax = max(imax, iang);

      dx = fxmin - lights[l].x0;
      dy = fymax - lights[l].y0;
      ang = atan2pi(dy, dx) + 1.0f;
      iang = 0.5f*ang*nLightAngles;
      dist += (dx*dx + dy*dy);
      imin = min(imin, iang);
      imax = max(imax, iang);

      dx = fxmax - lights[l].x0;
      dy = fymax - lights[l].y0;
      ang = atan2pi(dy, dx) + 1.0f;
      iang = 0.5f*ang*nLightAngles;
      dist += (dx*dx + dy*dy);
      imin = min(imin, iang);
      imax = max(imax, iang);

      dist *= 0.25f;

      int cnt = imax - imin;
      if (cnt > nLightAngles/2) { cnt = imin + nLightAngles - imax; imin = imax; }

      while (cnt >= 0) {
        atomic_min_global(lightDistance + l*nLightAngles + imin, dist);
        ++imin; if (imin >= nLightAngles) imin = 0;
        --cnt;
      }
    }
  }
}

__kernel void calcShadowMap(
    __write_only image2d_t   imgShadow,
    __constant   TypeLight2D *lights,
    __global     float       *lightDistance,
                 int          nLights,
                 int          nLightAngles
    ) {
  const uint x_coord = get_global_id(0);
  const uint y_coord = get_global_id(1);

  const uint width = get_global_size(0);
  const uint height = get_global_size(1);

  float fx = 2.0f*((float)(x_coord) + 0.5f)/width  - 1.0f;
  float fy = 2.0f*((float)(y_coord) + 0.5f)/height - 1.0f;

  float res = 0.0f;

  for (int l = 0; l < nLights; ++l) {
    float dx = fx - lights[l].x0;
    float dy = fy - lights[l].y0;
    float dist = (dx*dx + dy*dy);

    float intensity = lights[l].intensity*native_powr(1.0f + dist, -2.0f/lights[l].falloff);
    if (intensity < 0.001f) continue;

    float ang = atan2(dy, dx) + M_PI_F;
    int iang = (ang/(2.0f*M_PI_F))*nLightAngles;

    res += (dist < lightDistance[l*nLightAngles + iang]) ? intensity : 0.0f;
  }

  write_imagef(imgShadow, (int2) (x_coord, y_coord), (float4) (0.0f, 1.0f, 1.0f, res));
}

#define SOFT_SIZE (2)
__kernel void calcShadowMap2(
    __write_only image2d_t   imgShadow,
    __constant   TypeLight2D *lights,
    __global     float       *lightDistance,
                 int          nLights,
                 int          nLightAngles,
                 uint         sizeX,
                 uint         sizeY
    ) {
  const uint lid = get_local_id(0);
  const uint gid = get_group_id(0);

  const uint lsize = get_local_size(0);
  const uint ngrps = get_num_groups(0);

  uint nPerGroup = (sizeX*sizeY + ngrps - 1)/ngrps;

  uint id = mad24(gid, nPerGroup, lid);
  uint idmax = min(mul24((gid+1), nPerGroup), sizeX*sizeY);

  float iSizeX = 1.0f/sizeX;
  float iSizeY = 1.0f/sizeY;

  for (; id < idmax; id += lsize) {
    uint x_coord = id;
    uint y_coord = x_coord/sizeX; x_coord -= mul24(y_coord, sizeX);

    float fx = 2.0f*((float)(x_coord) + 0.5f)*iSizeX - 1.0f;
    float fy = 2.0f*((float)(y_coord) + 0.5f)*iSizeY - 1.0f;

    float res = 0.1f;

    for (int l = 0; l < nLights; ++l) {
      float dx = fx - lights[l].x0;
      float dy = fy - lights[l].y0;
      float dist = (dx*dx + dy*dy);

      if (dist < lights[l].size*lights[l].size) { res = 1.0f; break; }

      float intensity = lights[l].intensity*native_powr(1.0f + dist, -2.0f/lights[l].falloff);
      if (intensity < 0.01f) continue;

      float ang = atan2pi(dy, dx) + 1.0f;
      int iang = 0.5f*ang*nLightAngles;

      float stot = 0.0f;
      float wsum = 0.0f;
      int ia = iang - SOFT_SIZE;
      if (ia < 0) ia += nLightAngles;
      for (iang = -SOFT_SIZE; iang <= SOFT_SIZE; ++iang) {
         float scur = (dist < lightDistance[l*nLightAngles + ia]) ? 1.0f : max(1.0f - 50.0f*(dist - lightDistance[l*nLightAngles + ia]), 0.0f);

         float fd = (float)(abs(iang))/(SOFT_SIZE+1);
         float wcur = max(1.0f - fd/(sizeX*lights[l].size*dist), 0.0f);

         stot += scur*wcur;
         wsum += wcur;

        ++ia; if (ia >= nLightAngles) ia = 0;
      }
      res += intensity*stot/wsum;
    }

    write_imagef(imgShadow, (int2) (x_coord, y_coord), (float4) (0.0f, 1.0f, 1.0f, res));
  }

}
