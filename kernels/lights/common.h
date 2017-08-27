/*! \file common.h
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#pragma once

#include "../types.h"

#ifndef OPENCL_KERNEL_LANGUAGE
namespace CLIF {
#endif

typedef cl_float TypeObject;

struct st_TypeLight2D {
  cl_float4 color;
  cl_float2 dir;
  cl_float x0;
  cl_float y0;
  cl_float size;
  cl_float falloff;
  cl_float ang;
  cl_float intensity;
  cl_bool active;
  cl_uchar padding[7];
};

typedef struct st_TypeLight2D TypeLight2D;

#ifndef OPENCL_KERNEL_LANGUAGE
}
#endif
