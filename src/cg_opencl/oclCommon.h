/*! \file oclCommon.h
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#pragma once

#include <exception>

namespace OCL {
class Exception : public std::exception {
public:
    Exception(const char *info, ...);
    ~Exception() throw();

    char *_info; //!< Information about the Exception
};
}
