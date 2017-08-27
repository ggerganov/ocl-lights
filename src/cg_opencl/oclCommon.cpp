/*! \file oclCommon.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#include "cg_opencl/oclCommon.h"

#include <cstdio>
#include <cstdarg>

OCL::Exception::Exception(const char *info, ...) {
    _info = new char [2*8192];
    va_list ap;
    va_start(ap, info);
    vsprintf(_info, info, ap);
    va_end(ap);
}

OCL::Exception::~Exception() throw() {
    delete [] _info; _info = NULL;
}
