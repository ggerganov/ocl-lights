/*! \file oclBaseManager.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#include "cg_opencl/oclConstants.h"
#include "cg_opencl/oclBaseManager.h"
#include "cg_opencl/oclProfiler.h"

#include "cg_logger.h"
#include "cg_timer.h"

#ifndef __APPLE__
#include <CL/opencl.h>

#include <GL/glx.h>
#include <GL/gl.h>
#else
#include <OpenCL/cl_gl_ext.h>
#include <OpenCL/opencl.h>

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#endif

constexpr auto kLogTag = OCL::Constants::LogTags::kBaseManager;

namespace {
std::string uppercase(const std::string &s) {
    std::string res(s);
    for (size_t i = 0; i < res.length(); ++i) {
        if (res[i] >= 'a' && res[i] <= 'z') res[i] -= 32;
    }
    return res;
}
}

namespace OCL {

struct BaseManager::Buffer {
    cl_mem V = 0;
};

struct BaseManager::Device : public BaseManager::IDevice {
    Device() {}
    virtual ~Device() override {}

    virtual int getComputeUnits() const override { return computeUnits; }

    std::string name       = std::string("noname");
    std::string vendorName = std::string("noname");

    cl_uint computeUnits = 4;
    size_t maxWorkgroup  = 256;

    cl_platform_id platformID = 0;
    cl_device_id   deviceID   = 0;

    unsigned int optimumWorkgroups     = 16;
    unsigned int optimumWorkgroupSize  = 16;
};

struct BaseManager::Kernel : public BaseManager::IKernel {
    Kernel() {}
    virtual ~Kernel() override {}

    virtual int getSelectedWorkgroups() const override { return selectedWorkgroups; }

    int nArgs = 0;
    cl_kernel K = 0;

    size_t bestWorkgroups    = 16;
    size_t bestWorkgroupSize = 16;

    size_t selectedWorkgroups = 16;
    size_t selectedWorkgroupSize = 16;

    size_t maxWorkgroupSize = 256;
    size_t workgroupSizeMultiple = 32;

    cl_ulong privateMemUsed = 0;
    cl_ulong localMemUsed   = 0;
};

struct BaseManager::KernelContainer : public std::map<std::string, BaseManager::Kernel> {};
struct BaseManager::BufferContainer : public std::map<std::string, BaseManager::Buffer> {};

struct BaseManager::BuildConfiguration {
    bool                               recompile;
    std::string                        buildArguments;
    std::map<std::string, std::string> buildArgumentsForVendor;
};

struct BaseManager::OpenCLSupport {
    bool clEnqueueFillBuffer = false;
};

struct BaseManager::Data {
    cl_mem_flags toCLFags(int flags) const {
        cl_mem_flags res = 0;

        if ((flags & MEM_READ) && !(flags & MEM_WRITE)) res |= CL_MEM_READ_ONLY;
        if (!(flags & MEM_READ) && (flags & MEM_WRITE)) res |= CL_MEM_WRITE_ONLY;
        if ((flags & MEM_READ) && (flags & MEM_WRITE)) res |= CL_MEM_READ_WRITE;
        if (flags & MEM_COPY_HOST_PTR) res |= CL_MEM_COPY_HOST_PTR;

        return res;
    }

    void compileOrLoadProgram(
            const char* fname,
            cl_program &program,
            cl_context &context,
            cl_device_id &device) {
        cl_int ret;

        std::string args("-I "); args += _kernelPath; args += " ";
        args += _buildConfiguration.buildArguments;
        for (std::map<std::string, std::string>::iterator it =  _buildConfiguration.buildArgumentsForVendor.begin();
                it != _buildConfiguration.buildArgumentsForVendor.end(); ++it) {
            if (uppercase(getSelectedDevice().vendorName).find(uppercase(it->first)) != std::string::npos) {
                args += it->second;
            }
        }

        std::string fnameBin(fname); fnameBin += ".bin";
        FILE *fpbin = fopen(fnameBin.c_str(), "rb");
        if (fpbin == NULL || _buildConfiguration.recompile) {
            char *source_str;
            size_t source_size;
            source_str = new char[1000000];

            FILE *fp = fopen(fname, "r");
            if (!fp) throw Exception("[OCLM] Failed to open file '%s'.", fname);

            source_size = fread(source_str, 1, 1000000, fp);
            fclose(fp);

            program = clCreateProgramWithSource(context, 1, (const char **)&source_str, (const size_t *)&source_size, &ret);
            if (ret != CL_SUCCESS || !program) {
                throw Exception("[OCLM] Unable to create OpenCL program from source. (ret = %d)", ret);
            }

            delete [] source_str;

            ret = clBuildProgram(program, 1, &device, args.c_str(), NULL, NULL);

            {
                size_t len;
                clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
                std::vector<char> buffer(len, 0);
                clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, len, buffer.data(), &len);
                std::string log(buffer.data());
                if (ret != CL_SUCCESS) {
                    throw Exception("[OCLM] %d %s\n[OCLM] Failed to build OpenCL program. (ret = %d)", len, log.c_str(), ret);
                }
                CG_INFOC(10, "\n%s\n\n", log.c_str());
            }

            cl_uint nDevices = 0;
            ret = clGetProgramInfo(program, CL_PROGRAM_NUM_DEVICES, sizeof(cl_uint), &nDevices, NULL);

            if (nDevices == 1) {
                std::vector<size_t> programBinarySizes(nDevices);
                ret = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t) * nDevices, programBinarySizes.data(), NULL);
                if (ret != CL_SUCCESS) {
                    throw Exception("[OCLM] Unable to query program binary size. (ret = %d)", ret);
                }

                std::vector<std::vector<unsigned char>> programBinaries(1);
                for (cl_uint i = 0; i < 1; i++) {
                    programBinaries[i].resize(programBinarySizes[i]);
                }

                ret = clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(unsigned char*) * 1, programBinaries.data(), NULL);
                if (ret != CL_SUCCESS) {
                    throw Exception("[OCLM] Unable to query program binaries. (ret = %d)", ret);
                }

                FILE *fpsave = fopen(fnameBin.c_str(), "wb");
                fwrite(programBinaries[0].data(), 1, programBinarySizes[0], fpsave);
                fclose(fpsave);
            }
        } else {
            size_t binarySize;
            fseek(fpbin, 0, SEEK_END);
            binarySize = ftell(fpbin);
            rewind(fpbin);

            unsigned char *programBinary = new unsigned char[binarySize];
            size_t res = fread(programBinary, 1, binarySize, fpbin);
            fclose(fpbin);

            if (res != binarySize) {
                delete [] programBinary;
                throw Exception("[OCLM] Error while reading kernel binary file '%s'. (res = %ld)", fnameBin.c_str(), res);
            }

            cl_int binaryStatus;
            program = clCreateProgramWithBinary(context, 1, &device, &binarySize, (const unsigned char**)&programBinary, &binaryStatus, &ret);

            delete [] programBinary;
            if (ret != CL_SUCCESS) {
                throw Exception("[OCLM] Unable to create program with binary from '%s'. (ret = %d)", fnameBin.c_str(), ret);
            }

            if (binaryStatus != CL_SUCCESS) {
                throw Exception("[OCLM] Invalid binary for device. (ret = %d)", binaryStatus);
            }

            ret = clBuildProgram(program, 0, NULL, args.c_str(), NULL, NULL);

            {
                size_t len;
                clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
                std::vector<char>buffer(len);
                clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, len, buffer.data(), &len);
                std::string log(buffer.data());
                if (ret != CL_SUCCESS) {
                    throw Exception("[OCLM] %d %s\n[OCLM] Failed to build OpenCL program executable. (ret = %d)", len, log.c_str(), ret);
                }
                CG_INFOC(10, "\n%s\n\n", log.c_str());
            }

        }
    }

    std::vector<Device> & getDevices() {
        return (_deviceType == CPU_TYPE) ? _devicesCPU : _devicesGPU;
    }

    const std::vector<Device> & getDevices() const {
        return (_deviceType == CPU_TYPE) ? _devicesCPU : _devicesGPU;
    }

    Device & getSelectedDevice() {
        return getDevices().at(_selectedDeviceId);
    }

    const Device & getSelectedDevice() const {
        return getDevices().at(_selectedDeviceId);
    }

    cl_kernel getCLKernel(const std::string & kname) const {
        return _kernels.at(kname).K;
    }

    cl_context       _oclContext = 0;
    cl_command_queue _oclQueue   = 0;
    cl_device_id     _oclDevice  = 0;

    bool        _initialized = false;
    DeviceType  _deviceType = UNKNOWN;
    std::string _kernelPath = "./";

    KernelContainer _kernels;
    BufferContainer _buffers;

    size_t _selectedDeviceId;
    std::vector<Device> _devicesCPU;
    std::vector<Device> _devicesGPU;

    KernelTree _kernelTree;
    BuildConfiguration _buildConfiguration;
    OpenCLSupport _support;
};

BaseManager::BaseManager() :
    _data(new Data()),
    _initialized(_data->_initialized),
    _deviceType(_data->_deviceType),
    _kernelPath(_data->_kernelPath),
    _selectedDeviceId(_data->_selectedDeviceId),
    _devicesCPU(_data->_devicesCPU),
    _devicesGPU(_data->_devicesGPU),
    _kernels(_data->_kernels),
    _buffers(_data->_buffers),
    _kernelTree(_data->_kernelTree),
    _buildConfiguration(_data->_buildConfiguration),
    _support(_data->_support) {}

BaseManager::~BaseManager() {}

void BaseManager::configure(const std::string & /*fname*/) {
    configureDefault();
}

void BaseManager::configureDefault() {
    int deviceID = 0;
    _deviceType = GPU_TYPE;

    _buildConfiguration.recompile = true;
    _buildConfiguration.buildArguments = "-D OPENCL_KERNEL_LANGUAGE -I ./kernels/lights/GPU -I ./kernels/render/GPU";

    std::string kpath = "./kernels/";

    scanForAvailableDevices();
    initialize(_deviceType, deviceID, true);
    setKernelPath(kpath);

    addKernelToLoad("buffers/GPU/fill.cl", "buffers_fill_float", "buffers_fill_float");

    addKernelToLoad("lights/GPU/geometry.cl", "drawFloor", "drawFloor");
    addKernelToLoad("lights/GPU/geometry.cl", "drawObjects", "drawObjects");

    addKernelToLoad("lights/GPU/lightning.cl", "calcDistance2", "calcDistance2");
    addKernelToLoad("lights/GPU/lightning.cl", "calcShadowMap2", "calcShadowMap2");
    loadKernels();

    listKernelInformation();

    //INFO("deviceType      = %d\n", _deviceType);
    //INFO("deviceID        = %d\n", deviceID);
    //INFO("recompile       = %d\n", (int) _buildConfiguration.recompile);
    //INFO("build arguments = '%s'\n", _buildConfiguration.buildArguments.c_str());

    //for (std::map<std::string, std::string>::iterator it  = _buildConfiguration.buildArgumentsForVendor.begin();
    //                                                  it != _buildConfiguration.buildArgumentsForVendor.end(); ++it) {
    //  INFO("build args for '%s' = '%s'\n", it->first.c_str(), it->second.c_str());
    //}
    //INFO("kernel path     = '%s'\n", kpath.c_str());
}

void BaseManager::scanForAvailableDevices() {
    cl_int ret;

    char value[1024];
    size_t valueSize;

    cl_uint platformCount = 0;
    std::vector<cl_platform_id> platforms;

    cl_uint deviceCount = 0;
    std::vector<cl_device_id> devices;

    _devicesCPU.clear();
    _devicesGPU.clear();

    // get all platforms
    ret = clGetPlatformIDs(0, NULL, &platformCount);
    if (platformCount == 0) throw Exception("No available OpenCL platforms on this machine");
    if (ret != CL_SUCCESS) throw Exception("clGetPlatformIDs() failed, ret = %d", ret);

    platforms.resize(platformCount);
    ret = clGetPlatformIDs(platformCount, platforms.data(), NULL);
    if (ret != CL_SUCCESS) throw Exception("clGetPlatformIDs() failed, ret = %d", ret);

    for (cl_uint i = 0; i < platformCount; i++) {
        deviceCount = 0;

        // get all CPU devices
        ret = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_CPU, 0, NULL, &deviceCount);
        if (deviceCount == 0) continue;
        if (ret != CL_SUCCESS) throw Exception("clGetDeviceIDs() failed, ret = %d", ret);

        devices.resize(deviceCount);
        ret = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_CPU, deviceCount, devices.data(), NULL);
        if (ret != CL_SUCCESS) throw Exception("clGetDeviceIDs() failed, ret = %d", ret);

        // for each device print critical attributes
        for (cl_uint j = 0; j < deviceCount; j++) {
            Device curDev;

            // device name
            clGetDeviceInfo(devices[j], CL_DEVICE_NAME, 0, NULL, &valueSize);
            clGetDeviceInfo(devices[j], CL_DEVICE_NAME, valueSize, value, NULL);
            curDev.name = std::string(value);

            // vendor name
            clGetDeviceInfo(devices[j], CL_DEVICE_VENDOR, 0, NULL, &valueSize);
            clGetDeviceInfo(devices[j], CL_DEVICE_VENDOR, valueSize, value, NULL);
            curDev.vendorName = std::string(value);

            // compute units
            clGetDeviceInfo(devices[j], CL_DEVICE_MAX_COMPUTE_UNITS,
                            sizeof(curDev.computeUnits), &curDev.computeUnits, NULL);

            // max workgroup
            clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_GROUP_SIZE,
                            sizeof(curDev.maxWorkgroup), &curDev.maxWorkgroup, NULL);

            curDev.platformID = platforms[i];
            curDev.deviceID   = devices[j];

            curDev.optimumWorkgroups = curDev.computeUnits;
            curDev.optimumWorkgroupSize = 1;

            _devicesCPU.push_back(curDev);
        }
    }

    for (cl_uint i = 0; i < platformCount; i++) {
        deviceCount = 0;

        // get all GPU devices
        ret = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, 0, NULL, &deviceCount);
        if (deviceCount == 0) continue;
        if (ret != CL_SUCCESS) throw Exception("clGetDeviceIDs() failed, ret = %d", ret);

        devices.resize(deviceCount);
        ret = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, deviceCount, devices.data(), NULL);
        if (ret != CL_SUCCESS) throw Exception("clGetDeviceIDs() failed, ret = %d", ret);

        // for each device print critical attributes
        for (cl_uint j = 0; j < deviceCount; j++) {
            Device curDev;

            // device name
            clGetDeviceInfo(devices[j], CL_DEVICE_NAME, 0, NULL, &valueSize);
            clGetDeviceInfo(devices[j], CL_DEVICE_NAME, valueSize, value, NULL);
            curDev.name = std::string(value);

            // vendor name
            clGetDeviceInfo(devices[j], CL_DEVICE_VENDOR, 0, NULL, &valueSize);
            clGetDeviceInfo(devices[j], CL_DEVICE_VENDOR, valueSize, value, NULL);
            curDev.vendorName = std::string(value);

            // compute units
            clGetDeviceInfo(devices[j], CL_DEVICE_MAX_COMPUTE_UNITS,
                            sizeof(curDev.computeUnits), &curDev.computeUnits, NULL);

            // max workgroup
            clGetDeviceInfo(devices[j], CL_DEVICE_MAX_WORK_GROUP_SIZE,
                            sizeof(curDev.maxWorkgroup), &curDev.maxWorkgroup, NULL);

            curDev.platformID = platforms[i];
            curDev.deviceID   = devices[j];

            curDev.optimumWorkgroups = curDev.computeUnits;
            curDev.optimumWorkgroupSize = curDev.maxWorkgroup;

            _devicesGPU.push_back(curDev);
        }
    }
}

void BaseManager::listAvailableDevices(const int dtype) const {
    if (dtype == CPU_TYPE) {
        CG_IDBG(10, kLogTag, "There are %ld CPU devices on this machine:\n",
                _devicesCPU.size());
        for (int i = 0; i < (int) _devicesCPU.size(); ++i) {
            CG_IDBG(10, kLogTag, "  Device %d: %s\n \
            - Platform ID: %p\n \
            - Platform: %s\n \
            - Compute units: %u\n \
            - Max workgroup size: %lu\n \
            - Optimum workgroups: %d\n \
            - Optimum workgroup size: %d\n", i,
                    _devicesCPU[i].name.c_str(),
                    (void *) _devicesCPU[i].platformID,
                    _devicesCPU[i].vendorName.c_str(),
                    _devicesCPU[i].computeUnits,
                    _devicesCPU[i].maxWorkgroup,
                    _devicesCPU[i].optimumWorkgroups,
                    _devicesCPU[i].optimumWorkgroupSize);
        }
    } else if (dtype == GPU_TYPE) {
        CG_IDBG(10, kLogTag, "There are %ld GPU devices on this machine:\n",
                _devicesGPU.size());
        for (int i = 0; i < (int) _devicesGPU.size(); ++i) {
            CG_IDBG(10, kLogTag, "  Device %d: %s\n \
            - Platform ID: %p\n \
            - Platform: %s\n \
            - Compute units: %u\n \
            - Max workgroup size: %lu\n \
            - Optimum workgroups: %d\n \
            - Optimum workgroup size: %d\n", i,
                    _devicesGPU[i].name.c_str(),
                    (void *) _devicesGPU[i].platformID,
                    _devicesGPU[i].vendorName.c_str(),
                    _devicesGPU[i].computeUnits,
                    _devicesGPU[i].maxWorkgroup,
                    _devicesGPU[i].optimumWorkgroups,
                    _devicesGPU[i].optimumWorkgroupSize);
        }
    } else {
        throw Exception("Unknown dtype = %d", dtype);
    }
}

void BaseManager::listKernelInformation() const {
    CG_IDBG(10, kLogTag, "Listing kernel information:\n");
    for (const auto & kernel : _kernels) {
        CG_IDBG(10, kLogTag, "  Kernel '%s':\n", kernel.first.c_str());
        CG_IDBG(10, kLogTag, "    Best workgroups         = %lu\n", kernel.second.bestWorkgroups);
        CG_IDBG(10, kLogTag, "    Best workgroup size     = %ld\n", kernel.second.bestWorkgroupSize);
        CG_IDBG(10, kLogTag, "    Selected workgroups     = %lu\n", kernel.second.selectedWorkgroups);
        CG_IDBG(10, kLogTag, "    Selected workgroup size = %lu\n", kernel.second.selectedWorkgroupSize);
        CG_IDBG(10, kLogTag, "    Max workgoup size       = %lu\n", kernel.second.maxWorkgroupSize);
        CG_IDBG(10, kLogTag, "    Workgroup size multiple = %lu\n", kernel.second.workgroupSizeMultiple);
        CG_IDBG(10, kLogTag, "    Private mem used        = %llu\n", kernel.second.privateMemUsed);
        CG_IDBG(10, kLogTag, "    Local mem used          = %llu\n", kernel.second.localMemUsed);
    }
    CG_IDBG(10, kLogTag, "\n");
}

void BaseManager::initialize(DeviceType dtype, int did, bool clglInterop) {
    _deviceType  = dtype;
    auto & devices = _data->getDevices();

    if (did >= (int) devices.size()) {
        throw Exception("%s device with ID %d does not exist. Max ID = %d",
                        (dtype == CPU_TYPE) ? "CPU" : "GPU", did, (devices.size()-1));
    }

    cl_int ret;
    OCL_PROFILING_START("clCreateContext", true)
    if (clglInterop) {
        CG_IDBG(10, kLogTag, "Requested CL-GL interop. Will try to allocate interop context.\n");
#if defined(__APPLE__)
        CGLContextObj cgl_current_context = CGLGetCurrentContext();
        CGLShareGroupObj cgl_share_group = CGLGetShareGroup(cgl_current_context);

        cl_context_properties properties[] = {
            CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
            (cl_context_properties) cgl_share_group,
            0
        };
#else
        cl_context_properties properties[] = {
            CL_CONTEXT_PLATFORM, (cl_context_properties) devices[did].platformID,
#if defined(__linux__)
            CL_GL_CONTEXT_KHR, (cl_context_properties) glXGetCurrentContext(),
            CL_GLX_DISPLAY_KHR, (cl_context_properties) glXGetCurrentDisplay(),
#elif defined(WIN32)
            CL_GL_CONTEXT_KHR, (cl_context_properties) wglGetCurrentContext(),
            CL_WGL_HDC_KHR, (cl_context_properties) wglGetCurrentDC(),
#endif
            0
        };
#endif
        _data->_oclContext = clCreateContext(properties, 1, &devices[did].deviceID, NULL, NULL, &ret);
    } else {
        _data->_oclContext = clCreateContext(NULL, 1, &devices[did].deviceID, NULL, NULL, &ret);
    }
    OCL_PROFILING_STOP("clCreateContext", true)

    if (ret != CL_SUCCESS || !_data->_oclContext) {
        throw Exception("Unable to create OpenCL context. ret = %d\n", ret);
    }

    OCL_PROFILING_START("clCreateCommandQueue", true)
#if defined(CL_VERSION_2_0) && !defined(__linux__)
    _data->_oclQueue = clCreateCommandQueueWithProperties(_data->_oclContext, devices[did].deviceID, 0, &ret);
#else
    _data->_oclQueue = clCreateCommandQueue(_data->_oclContext, devices[did].deviceID, 0, &ret);
#endif
    OCL_PROFILING_STOP("clCreateCommandQueue", true)
    if (ret != CL_SUCCESS || !_data->_oclQueue) throw Exception("Unable to create OpenCL queue.");

    _data->_oclDevice = devices[did].deviceID;
    _selectedDeviceId = did;
    _initialized = true;

    auto & selectedDevice = _data->getSelectedDevice();

    CG_IDBG(10, kLogTag, "\n");
    CG_IDBG(10, kLogTag, "Using %s device '%s'.\n \
        - Optimum workgroups     = %d\n \
        - Optimum workgroup size = %d\n",
            (dtype == CPU_TYPE) ? "CPU" : "GPU",
            selectedDevice.name.c_str(),
            selectedDevice.optimumWorkgroups,
            selectedDevice.optimumWorkgroupSize);

    checkSupport();
}

void BaseManager::addKernelToLoad(const char *fname, const char *kname, const char *kid) {
    _kernelTree[fname].push_back(std::make_pair<std::string, std::string>(kname, kid));
}

void BaseManager::loadKernels() {
    cl_int ret;
    if (!_initialized) {
        throw Exception("[OCLM] Cannot load kernels before initializing OpenCL.");
    }

    CG_IDBG(10, kLogTag, "\n");
    CG_IDBG(10, kLogTag, "Loading kernels ...\n");
    for (const auto & node : _kernelTree) {
        std::string fname(_kernelPath); fname += node.first;

        CG_IDBG(10, kLogTag, "   Compiling or loading source file '%s' ...", fname.c_str());

        cl_program program;
        _data->compileOrLoadProgram(fname.c_str(), program, _data->_oclContext, _data->_oclDevice);

        for (int i = 0; i < (int) node.second.size(); ++i) {
            CG_IDBG(10, kLogTag, "     - Loading kernel '%s' ...", node.second[i].second.c_str());

            CG::Timer kTimer; kTimer.start();

            Kernel &curKernel = _kernels[node.second[i].second];
            curKernel.K = clCreateKernel(program, node.second[i].first.c_str(), &ret);
            if (ret != CL_SUCCESS || !curKernel.K) {
                throw Exception("[OCLM] Unable to create OpenCL '%s' kernel. (ret = %d)", node.second[i].first.c_str(), ret);
            }

            ret = clGetKernelWorkGroupInfo(
                      curKernel.K, _data->_oclDevice, CL_KERNEL_WORK_GROUP_SIZE,
                      sizeof(curKernel.maxWorkgroupSize), &(curKernel.maxWorkgroupSize), NULL);
            if (ret != CL_SUCCESS) {
                throw Exception("[OCLM] Unable to query CL_KERNEL_WORK_GROUP_SIZE for kernel '%s'. (ret = %d)", node.second[i].first.c_str(), ret);
            }

            ret = clGetKernelWorkGroupInfo(
                      curKernel.K, _data->_oclDevice, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
                      sizeof(curKernel.workgroupSizeMultiple), &(curKernel.workgroupSizeMultiple), NULL);
            if (ret != CL_SUCCESS) {
                throw Exception("[OCLM] Unable to query CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE for kernel '%s'. (ret = %d)", node.second[i].first.c_str(), ret);
            }

            ret = clGetKernelWorkGroupInfo(
                      curKernel.K, _data->_oclDevice, CL_KERNEL_LOCAL_MEM_SIZE,
                      sizeof(curKernel.localMemUsed), &(curKernel.localMemUsed), NULL);
            if (ret != CL_SUCCESS) {
                throw Exception("[OCLM] Unable to query CL_KERNEL_LOCAL_MEM_SIZE for kernel '%s'. (ret = %d)", node.second[i].first.c_str(), ret);
            }

            ret = clGetKernelWorkGroupInfo(
                      curKernel.K, _data->_oclDevice, CL_KERNEL_PRIVATE_MEM_SIZE,
                      sizeof(curKernel.privateMemUsed), &(curKernel.privateMemUsed), NULL);
            if (ret != CL_SUCCESS) {
                throw Exception("[OCLM] Unable to query CL_KERNEL_PRIVATE_MEM_SIZE for kernel '%s'. (ret = %d)", node.second[i].first.c_str(), ret);
            }

            auto & selectedDevice = _data->getSelectedDevice();

            curKernel.bestWorkgroups = selectedDevice.optimumWorkgroups;
            curKernel.bestWorkgroupSize = curKernel.maxWorkgroupSize;

            curKernel.selectedWorkgroups = selectedDevice.optimumWorkgroups;
            curKernel.selectedWorkgroupSize = curKernel.maxWorkgroupSize;

            CG_INFOC(10, "  took %g sec\n", kTimer.time());
        }
    }
    CG_IDBG(10, kLogTag, "\n");
}

void BaseManager::allocateMemory() {};

bool BaseManager::isInitialized() const { return _initialized; }
void BaseManager::setKernelPath(const std::string &kpath) { _kernelPath = kpath; }

int BaseManager::getOptimumWorkgroups() const {
    return _data->getSelectedDevice().optimumWorkgroups;
}

int BaseManager::getOptimumWorkgroupSize() const {
    return _data->getSelectedDevice().optimumWorkgroupSize;
}

void BaseManager::setOptimumWorkgroups(const int nwg) {
    _data->getSelectedDevice().optimumWorkgroups = nwg;
}

void BaseManager::setOptimumWorkgroupSize(const int wgs) {
    _data->getSelectedDevice().optimumWorkgroupSize = wgs;
}


void BaseManager::setKernelArg(
    const std::string &kname,
    const int aid,
    const int asize,
    void *a) {
    cl_int ret;

    ret = clSetKernelArg(_kernels[kname].K, aid, asize, a);
    if (ret != CL_SUCCESS) {
        throw Exception("Unable to set kernel argument %d for kernel '%s'. ret = %d",
                        aid, kname.c_str(), ret);
    }
}

void BaseManager::setKernelArgAsBuffer(
    const std::string &kname,
    const int aid,
    const std::string &bname) {
    cl_int ret;

    ret = clSetKernelArg(_kernels[kname].K, aid, sizeof(cl_mem), &_buffers[bname].V);
    if (ret != CL_SUCCESS) {
        throw Exception("Unable to set kernel argument %d for kernel '%s'. ret = %d",
                        aid, kname.c_str(), ret);
    }
}

const BaseManager::KernelTree & BaseManager::getKernelTree() const {
    return _kernelTree;
}

void BaseManager::printKernelTree() const {
    CG_IDBG(10, kLogTag, "\n");
    CG_IDBG(10, kLogTag, "Printing OpenCL kernel tree\n");
    for (const auto & node : _kernelTree) {
        std::string fname(_kernelPath); fname += node.first;
        CG_IDBG(10, kLogTag, "    OpenCL source file '%s':\n", fname.c_str());

        for (int i = 0; i < (int) node.second.size(); ++i) {
            CG_IDBG(10, kLogTag, "      - Kernel '%40s', sid = '%s'\n",
                    node.second[i].first.c_str(),
                    node.second[i].second.c_str());
        }
    }
    CG_IDBG(10, kLogTag, "\n");
}

void BaseManager::flush() {
    clFlush(_data->_oclQueue);
    clFinish(_data->_oclQueue);
}

void BaseManager::finish() {
    clFinish(_data->_oclQueue);
}

void BaseManager::fillBufferFloat(
    const std::string &bname,
    const cl_float f,
    const int bsize) {
    cl_int ret;

    flush();

    OCL_PROFILING_START("oclBuffer_fillFloat_ALL", true);
    OCL_PROFILING_START("oclBuffer_fillFloat_"+bname, true);

    if (_support.clEnqueueFillBuffer) {
        ret = clEnqueueFillBuffer(
                _data->_oclQueue, _buffers[bname].V, &f, sizeof(cl_float), 0, bsize*sizeof(cl_float),
                0, NULL, NULL);

        if (ret != CL_SUCCESS) {
            throw Exception("Unable to fill buffer '%s' with floats. ret = %d",
                    bname.c_str(), ret);
        }
    } else {
        if (_kernels.find(Constants::KernelNames::kBuffersFillFloat) == _kernels.end()) {
            throw Exception("No support for support clEnqueueFillBuffer and missing kernel '%s'\n",
                    Constants::KernelNames::kBuffersFillFloat);
        } else {
            cl_uint bufSize = bsize;
            cl_float fval = f;

            setKernelArg(Constants::KernelNames::kBuffersFillFloat, 0, sizeof(cl_mem), &_buffers[bname].V);
            setKernelArg(Constants::KernelNames::kBuffersFillFloat, 1, sizeof(cl_uint), &bufSize);
            setKernelArg(Constants::KernelNames::kBuffersFillFloat, 2, sizeof(cl_float), &fval);

            runKernelOptimum(Constants::KernelNames::kBuffersFillFloat);
        }
    }

    flush();

    OCL_PROFILING_STOP("oclBuffer_fillFloat_"+bname, true);
    OCL_PROFILING_STOP("oclBuffer_fillFloat_ALL", true);
}

void BaseManager::writeBuffer(
    const std::string &bname,
    const bool block,
    const int bsize,
    const void *bptr) {
    cl_int ret;

    flush();

    OCL_PROFILING_START("oclBuffer_write_ALL", true);
    OCL_PROFILING_START("oclBuffer_write_"+bname, true);

    ret = clEnqueueWriteBuffer(
              _data->_oclQueue, _buffers[bname].V, block, 0, bsize, bptr,
              0, NULL, NULL);

    if (ret != CL_SUCCESS) {
        throw Exception("Unable to write buffer '%s' to OpenCL device. ret = %d",
                        bname.c_str(), ret);
    }

    flush();

    OCL_PROFILING_STOP("oclBuffer_write_"+bname, true);
    OCL_PROFILING_STOP("oclBuffer_write_ALL", true);
}

void BaseManager::readBuffer(
    const std::string &bname,
    const bool block,
    const int bsize,
    void *bptr) {
    cl_int ret;

    flush();

    OCL_PROFILING_START("oclBuffer_read_ALL", true);
    OCL_PROFILING_START("oclBuffer_read_"+bname, true);

    ret = clEnqueueReadBuffer(
              _data->_oclQueue, _buffers[bname].V, block, 0, bsize, bptr,
              0, NULL, NULL);

    if (ret != CL_SUCCESS) {
        throw Exception("Unable to read buffer '%s' from OpenCL device. ret = %d",
                        bname.c_str(), ret);
    }

    flush();

    OCL_PROFILING_STOP("oclBuffer_read_"+bname, true);
    OCL_PROFILING_STOP("oclBuffer_read_ALL", true);
}

void BaseManager::copyBuffer(
    const std::string &srcname,
    const std::string &dstname,
    const int bsize) {
    cl_int ret;

    flush();

    OCL_PROFILING_START("oclBuffer_copy_ALL", true);
    OCL_PROFILING_START("oclBuffer_copy_"+srcname, true);

    ret = clEnqueueCopyBuffer(
              _data->_oclQueue,
              _buffers[srcname].V,
              _buffers[dstname].V,
              0, 0, bsize, 0, NULL, NULL);

    if (ret != CL_SUCCESS) {
        throw Exception("Unable to copy OpenCL buffer '%s' to buffer '%s'. ret = %d",
                        srcname.c_str(), dstname.c_str(), ret);
    }

    flush();

    OCL_PROFILING_STOP("oclBuffer_copy_"+srcname, true);
    OCL_PROFILING_STOP("oclBuffer_copy_ALL", true);
}

void BaseManager::copyImage(
    const std::string &srcname,
    const std::string &dstname,
    const int nx,
    const int ny,
    const int nz) {
    cl_int ret;

    flush();

    OCL_PROFILING_START("oclImage_copy_ALL", true);
    OCL_PROFILING_START("oclImage_copy_"+srcname, true);

    size_t src_origin[3] = {0, 0, 0};
    size_t dst_origin[3] = {0, 0, 0};
    size_t range[3] = {(size_t) nx, (size_t) ny, (size_t) nz};

    ret = clEnqueueCopyImage(
              _data->_oclQueue,
              _buffers[srcname].V,
              _buffers[dstname].V,
              src_origin, dst_origin, range, 0, NULL, NULL);

    if (ret != CL_SUCCESS) {
        throw Exception("Unable to copy OpenCL image '%s' to image '%s'. ret = %d",
                        srcname.c_str(), dstname.c_str(), ret);
    }

    flush();

    OCL_PROFILING_STOP("oclImage_copy_"+srcname, true);
    OCL_PROFILING_STOP("oclImage_copy_ALL", true);
}

void BaseManager::writeImageFloat3D(
    const std::string &iname,
    const bool block,
    const int nx,
    const int ny,
    const int nz,
    void *bptr) {
    cl_int ret;

    flush();

    OCL_PROFILING_START("oclBuffer_write_ALL", true);
    OCL_PROFILING_START("oclBuffer_write_"+iname, true);

    size_t origin[3] = {0, 0, 0};
    size_t range[3]  = {(size_t) nx, (size_t) ny, (size_t) nz};

    ret = clEnqueueWriteImage(
              _data->_oclQueue, _buffers[iname].V, block, origin, range,
              nx*sizeof(float), nx*ny*sizeof(float), bptr, 0, NULL, NULL);

    if (ret != CL_SUCCESS) {
        throw Exception("Unable to write OpenCL 3D image '%s'. ret = %d",
                        iname.c_str(), ret);
    }

    flush();

    OCL_PROFILING_STOP("oclBuffer_write_ALL", true);
    OCL_PROFILING_STOP("oclBuffer_write_"+iname, true);
}

void BaseManager::runKernel(
    const std::string &kname,
    const int nWorkgroups,
    const int workgroupSize) {
    CG_IDBG(20, kLogTag, "Running kernel '%s' (%d, %d) ... \n", kname.c_str(), nWorkgroups, workgroupSize);

    flush();

    OCL_PROFILING_START("kernel_ALL", true);
    OCL_PROFILING_START("kernel_" + kname, true);

    size_t local_item_size  =
        (workgroupSize == -1) ? _data->getSelectedDevice().optimumWorkgroupSize : workgroupSize;
    size_t global_item_size = nWorkgroups*workgroupSize;

    cl_int ret;

    ret = clEnqueueNDRangeKernel(
              _data->_oclQueue, _kernels[kname].K,
              1, NULL, &global_item_size, &local_item_size,
              0, NULL, NULL);
    if (ret != CL_SUCCESS) {
        throw Exception("Unable to run '%s' kernel. ret = %d", kname.c_str(), ret);
    }

    flush();

    OCL_PROFILING_STOP("kernel_" + kname, true);
    OCL_PROFILING_STOP("kernel_ALL", true);
}

void BaseManager::runKernelSelected(const std::string &kname) {
    CG_IDBG(20, kLogTag, "Running kernel '%s' with selected parameters (%ld, %ld)...\n", kname.c_str(),
            _kernels[kname].selectedWorkgroups,
            _kernels[kname].selectedWorkgroupSize);

    flush();

    OCL_PROFILING_START("kernel_ALL", true);
    OCL_PROFILING_START("kernel_" + kname, true);

    int ngrps = _kernels[kname].selectedWorkgroups;

    size_t local_item_size  = _kernels[kname].selectedWorkgroupSize;
    size_t global_item_size = local_item_size*ngrps;

    cl_int ret;

    ret = clEnqueueNDRangeKernel(
              _data->_oclQueue, _kernels[kname].K,
              1, NULL, &global_item_size, &local_item_size,
              0, NULL, NULL);
    if (ret != CL_SUCCESS) {
        throw Exception("Unable to run '%s' kernel. ret = %d", kname.c_str(), ret);
    }

    flush();

    OCL_PROFILING_STOP("kernel_" + kname, true);
    OCL_PROFILING_STOP("kernel_ALL", true);
}

void BaseManager::runKernelOptimum(const std::string &kname) {
    CG_IDBG(20, kLogTag, "Running kernel '%s' with optimum parameters (%ld, %ld)...\n", kname.c_str(),
            _kernels[kname].bestWorkgroups,
            _kernels[kname].bestWorkgroupSize);

    flush();

    OCL_PROFILING_START("kernel_ALL", true);
    OCL_PROFILING_START("kernel_" + kname, true);

    int ngrps = _kernels[kname].bestWorkgroups;

    size_t local_item_size  = _kernels[kname].bestWorkgroupSize;
    size_t global_item_size = local_item_size*ngrps;

    cl_int ret;

    ret = clEnqueueNDRangeKernel(
              _data->_oclQueue, _kernels[kname].K,
              1, NULL, &global_item_size, &local_item_size,
              0, NULL, NULL);
    if (ret != CL_SUCCESS) {
        throw Exception("Unable to run '%s' kernel. ret = %d", kname.c_str(), ret);
    }

    flush();

    OCL_PROFILING_STOP("kernel_" + kname, true);
    OCL_PROFILING_STOP("kernel_ALL", true);
}

void BaseManager::runKernelOptimum(
    const std::string &kname,
    const int nTotalWorkItems,
    const int nPerGroupAID,
    const int nPerWorkitemAID) {
    CG_IDBG(20, kLogTag, "Running kernel '%s' with optimum parameters (%d, %d)...\n", kname.c_str(),
            _data->getSelectedDevice().optimumWorkgroups,
            _data->getSelectedDevice().optimumWorkgroupSize);

    flush();

    OCL_PROFILING_START("kernel_ALL", true);
    OCL_PROFILING_START("kernel_" + kname, true);

    int ngrps = _data->getSelectedDevice().optimumWorkgroups;

    size_t local_item_size  = _data->getSelectedDevice().optimumWorkgroupSize;
    size_t global_item_size = local_item_size*ngrps;

    unsigned int nPerGroup    = (nTotalWorkItems + ngrps - 1)/ngrps;
    unsigned int nPerWorkitem = (nPerGroup+local_item_size-1)/local_item_size;

    cl_int ret;

    ret  = clSetKernelArg(_kernels[kname].K, nPerGroupAID, sizeof(unsigned int), &nPerGroup);
    ret |= clSetKernelArg(_kernels[kname].K, nPerWorkitemAID, sizeof(unsigned int), &nPerWorkitem);

    ret |= clEnqueueNDRangeKernel(
               _data->_oclQueue, _kernels[kname].K,
               1, NULL, &global_item_size, &local_item_size,
               0, NULL, NULL);
    if (ret != CL_SUCCESS) {
        throw Exception("Unable to run '%s' kernel. ret = %d", kname.c_str(), ret);
    }

    flush();

    OCL_PROFILING_STOP("kernel_" + kname, true);
    OCL_PROFILING_STOP("kernel_ALL", true);
}

void BaseManager::runKernel2D(
    const std::string &kname,
    const int globalx,
    const int globaly,
    const int localx,
    const int localy) {
    CG_IDBG(20, kLogTag, "Running kernel '%s' ... \n", kname.c_str());

    flush();

    OCL_PROFILING_START("kernel_ALL", true);
    OCL_PROFILING_START("kernel_" + kname, true);

    size_t local_item_size[2]  = {(size_t) localx,  (size_t) localy};
    size_t global_item_size[2] = {(size_t) globalx, (size_t) globaly};

    cl_int ret;

    ret = clEnqueueNDRangeKernel(
              _data->_oclQueue, _kernels[kname].K,
              2, NULL, global_item_size, local_item_size,
              0, NULL, NULL);
    if (ret != CL_SUCCESS) {
        throw Exception("Unable to run '%s' kernel. ret = %d", kname.c_str(), ret);
    }

    flush();

    OCL_PROFILING_STOP("kernel_" + kname, true);
    OCL_PROFILING_STOP("kernel_ALL", true);
}

void BaseManager::acquireGLObject(const std::string &oname) {
    cl_int ret;

    flush();

    OCL_PROFILING_START("oclGL_acquireObject_ALL", true);
    OCL_PROFILING_START("oclGL_acquireObject_"+oname, true);

    ret = clEnqueueAcquireGLObjects(
              _data->_oclQueue, 1, &_buffers[oname].V, 0, NULL, NULL);

    if (ret != CL_SUCCESS) {
        throw Exception("Unable to acquire GL object '%s'. ret = %d",
                        oname.c_str(), ret);
    }

    flush();

    OCL_PROFILING_STOP("oclGL_acquireObject_"+oname, true);
    OCL_PROFILING_STOP("oclGL_acquireObject_ALL", true);
}

void BaseManager::releaseGLObject(const std::string &oname) {
    cl_int ret;

    flush();

    OCL_PROFILING_START("oclGL_releaseObject_ALL", true);
    OCL_PROFILING_START("oclGL_releaseObject_"+oname, true);

    ret = clEnqueueReleaseGLObjects(
              _data->_oclQueue, 1, &_buffers[oname].V, 0, NULL, NULL);

    if (ret != CL_SUCCESS) {
        throw Exception("Unable to release GL object '%s'. ret = %d",
                        oname.c_str(), ret);
    }

    flush();

    OCL_PROFILING_STOP("oclGL_releaseObject_"+oname, true);
    OCL_PROFILING_STOP("oclGL_releaseObject_ALL", true);
}

void BaseManager::allocateOpenCLBuffer(
    const std::string  &bname,
    size_t        bufferSize,
    void         *bufferPtr
) {
    allocateOpenCLBuffer(
        bname,
        (CLFlags) (MEM_READ_WRITE | ((bufferPtr) ? MEM_COPY_HOST_PTR : MEM_NONE)),
        bufferSize,
        bufferPtr);
}

void BaseManager::allocateOpenCLBuffer(
    const std::string  &bname,
    const CLFlags       flags,
    size_t        bufferSize,
    void         *bufferPtr
) {
    cl_int ret;
    if (_buffers[bname].V) deallocateOpenCLObject(bname);

    CG_IDBG(10, kLogTag, "Creating OpenCL buffer '%s'. Size = %g MB\n",
            bname.c_str(), ((float)bufferSize)/(1024*1024));

    _buffers[bname].V = clCreateBuffer(
            _data->_oclContext, _data->toCLFags(flags),
            bufferSize, bufferPtr, &ret);
    if (ret != CL_SUCCESS || !_buffers[bname].V) {
        throw OCL::Exception("Unable to allocate OpenCL buffer '%s'. (ret = %d)",
                             bname.c_str(), ret);
    }
}

void BaseManager::allocateOpenCLTexture2D(
    const std::string  &tname,
    const CLFlags       flags,
    GLuint        glTexId) {
    cl_int ret;
    if (_buffers[tname].V) deallocateOpenCLObject(tname);

    CG_IDBG(10, kLogTag, "Creating OpenCL 2D texture '%s' from OpenGL texture\n", tname.c_str());

    _buffers[tname].V = clCreateFromGLTexture(
            _data->_oclContext, _data->toCLFags(flags),
            GL_TEXTURE_2D, 0, glTexId, &ret);
    if (ret != CL_SUCCESS || !_buffers[tname].V) {
        throw OCL::Exception("Unable to create CL texture object '%s' from GL texture.\n\
                  Either CL-GL interop was not requested when the OpenCL context was created or \n\
                  the used device does not support CL-GL interop at all. ret = %d", tname.c_str(), ret);
    }
}

void BaseManager::allocateOpenCLImage3D(
    const std::string  &iname,
    const int           nx,
    const int           ny,
    const int           nz,
    const CLFlags       flags,
    void               *bufferPtr) {
    cl_int ret;
    if (_buffers[iname].V) deallocateOpenCLObject(iname);

    cl_image_format img_fmt;
    img_fmt.image_channel_order = CL_INTENSITY;
    img_fmt.image_channel_data_type = CL_FLOAT;

    cl_image_desc img_desc;
    img_desc.image_type = CL_MEM_OBJECT_IMAGE3D;
    img_desc.image_width = nx;
    img_desc.image_height = ny;
    img_desc.image_depth = nz;
    img_desc.image_array_size = 1;
    img_desc.image_row_pitch = 0;
    img_desc.image_slice_pitch = 0;
    img_desc.num_mip_levels = 0;
    img_desc.num_samples = 0;
    img_desc.buffer = NULL;

    CG_IDBG(10, kLogTag, "Allocating 3D image on the GPU\n");
    _buffers[iname].V = clCreateImage(
            _data->_oclContext, _data->toCLFags(flags),
            &img_fmt, &img_desc, bufferPtr, &ret);
    if (ret != CL_SUCCESS || !_buffers[iname].V) {
        throw Exception("[OCLM] Unable to create OpenCL GPU 3D Image '%s'. (ret = %d)", iname.c_str(), ret);
    }
}

void BaseManager::deallocateOpenCLObject(const std::string & bname) {
    CG_IDBG(10, kLogTag, "Deallocating OpenCL object '%s'\n", bname.c_str());

    cl_int ret = clReleaseMemObject(_buffers[bname].V);
    if (ret != CL_SUCCESS) {
        throw OCL::Exception("Unable to deallocate OpenCL object '%s'. (ret = %d)",
                bname.c_str(), ret);
    }
}

void BaseManager::checkSupport() {
    CG_IDBG(10, kLogTag, "Checking for supported features\n");

    flush();

    {
        cl_int ret;
        std::string bname = "__support_clEnqueueFillBuffer";

        size_t nbuf = 16;
        size_t bufSize = nbuf*sizeof(cl_float);
        std::vector<cl_float> fbuf(nbuf, 0.0f);

        allocateOpenCLBuffer(bname,
                (CLFlags) (MEM_READ_WRITE | MEM_COPY_HOST_PTR),
                bufSize, fbuf.data());

        cl_float f = 123.0f;
        ret = clEnqueueFillBuffer(
                _data->_oclQueue, _buffers[bname].V,
                &f, sizeof(cl_float), 0, bufSize,
                0, NULL, NULL);

        if (ret != CL_SUCCESS) {
            throw Exception("Error when executing 'clEnqueueFillBuffer'. ret = %d",
                    ret);
        }

        readBuffer(bname, CL_TRUE, bufSize, fbuf.data());
        deallocateOpenCLObject(bname);

        bool isSupported = true;
        for (auto v : fbuf) {
            if (v != f) {
                isSupported = false;
                break;
            }
        }

        _support.clEnqueueFillBuffer = isSupported;

        CG_IDBG(10, kLogTag, " - clEnqueueFillBuffer: %s\n", isSupported ? "Yes" : "No");
    }
}

BaseManager::IKernel & BaseManager::getKernel(const std::string & kname) {
    return _kernels[kname];
}

}
