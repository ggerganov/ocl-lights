/*! \file types.h
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#pragma once

#ifdef OPENCL_KERNEL_LANGUAGE

typedef uchar     cl_uchar;
typedef uchar4    cl_uchar4;
typedef int       cl_int;
typedef uint      cl_uint;
typedef bool      cl_bool;
typedef float     cl_float;
typedef float2    cl_float2;
typedef float3    cl_float3;
typedef float4    cl_float4;

typedef struct { uint x; uint c; } cl_rng_state;
#else
#ifdef __APPLE__
    #include <OpenCL/opencl.h>
#else
    #include <CL/opencl.h>
#endif
//typedef cl_uchar  my_uchar;
//typedef cl_uchar4 my_uchar4;
//typedef cl_int    my_int;
//typedef cl_uint   my_uint;
//typedef cl_bool   my_bool;
//typedef cl_float  my_float;
//typedef cl_float3 my_float3;
//typedef cl_float2 my_float2;
//typedef cl_float4 my_float4;

//typedef struct { cl_uint x; cl_uint c; } cl_rng_state;
#endif
