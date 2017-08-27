/*! \file data.h
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#pragma once

#include "../kernels/lights/common.h"

#include <vector>

namespace Data {
struct Objects : public std::vector<CLIF::TypeObject> {};

struct Light : public CLIF::TypeLight2D {};
struct Lights : public std::vector<Light> {};

struct LightDistance : public std::vector<cl_float> {};
}
