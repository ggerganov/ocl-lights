/*! \file oclBaseManager.h
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#pragma once

#include "oclCommon.h"

#include <vector>
#include <string>
#include <map>
#include <memory>

namespace OCL {

class BaseManager {
public: // Interfaces

    struct IDevice {
        virtual ~IDevice() {}
        virtual int getComputeUnits() const = 0;
    };

    struct IKernel {
        virtual ~IKernel() {}
        virtual int getSelectedWorkgroups() const = 0;
    };

    using KernelTree =
        std::map<std::string, std::vector<std::pair<std::string, std::string> > >;

public: // Core
    BaseManager();
    virtual ~BaseManager();

    enum DeviceType {
        UNKNOWN = -1,
        CPU_TYPE = 0,
        GPU_TYPE = 1,
    };

    enum CLFlags {
        MEM_NONE = 0,
        MEM_READ = 1,
        MEM_WRITE = 2,
        MEM_READ_WRITE = 3,
        MEM_COPY_HOST_PTR = 4,
    };

    virtual void configure(const std::string & fname);
    virtual void configureDefault();

    void scanForAvailableDevices();

    void listAvailableDevices(const int dtype) const;
    void listKernelInformation() const;
    void initialize(DeviceType dtype, int did, bool clglInterop = false);

    void addKernelToLoad(const char *fname, const char *kname, const char *kid);

    virtual void loadKernels();
    virtual void allocateMemory();

    bool isInitialized() const;
    void setKernelPath(const std::string &kpath);

    int getOptimumWorkgroups() const;
    int getOptimumWorkgroupSize() const;
    void setOptimumWorkgroups(const int nwg);
    void setOptimumWorkgroupSize(const int wgs);

    void setKernelArg(
        const std::string &kname,
        const int aid,
        const int asize,
        void *a);

    template <typename T>
    void setKernelArg(
        const std::string &kname,
        const int aid,
        T a) {
        setKernelArg(kname, aid, sizeof(a), (void *)(&a));
    }

    void setKernelArgAsBuffer(
        const std::string &kname,
        const int aid,
        const std::string &bname);

    const KernelTree & getKernelTree() const;
    void printKernelTree() const;

    void flush();
    void finish();

    void fillBufferFloat(
        const std::string &bname,
        const float f,
        const int bsize);

    void writeBuffer(
        const std::string &bname,
        const bool block,
        const int bsize,
        const void *bptr);

    void readBuffer(
        const std::string &bname,
        const bool block,
        const int bsize,
        void *bptr);

    void copyBuffer(
        const std::string &srcname,
        const std::string &dstname,
        const int bsize);

    void copyImage(
        const std::string &srcname,
        const std::string &dstname,
        const int nx,
        const int ny,
        const int nz);

    void writeImageFloat3D(
        const std::string &iname,
        const bool block,
        const int nx,
        const int ny,
        const int nz,
        void *bptr);

    void runKernel(
        const std::string &kname,
        const int nWorkgroups,
        const int workgroupSize = -1);

    void runKernelSelected(const std::string &kname);
    void runKernelOptimum(const std::string &kname);

    void runKernelOptimum(
        const std::string &kname,
        const int nTotalWorkItems,
        const int nPerGroupAID,
        const int nPerWorkitemAID);

    void runKernel2D(
        const std::string &kname,
        const int globalx,
        const int globaly,
        const int localx,
        const int localy);

    void acquireGLObject(const std::string &oname);
    void releaseGLObject(const std::string &oname);

    void allocateOpenCLBuffer(
        const std::string  &bname,
        size_t        bufferSize,
        void         *bufferPtr = NULL);

    void allocateOpenCLBuffer(
        const std::string &tname,
        const CLFlags      flags,
        size_t        bufferSize,
        void         *bufferPtr = NULL);

    void allocateOpenCLTexture2D(
        const std::string  &bname,
        const CLFlags       flags,
        unsigned int  glTexId);

    void allocateOpenCLImage3D(
        const std::string &iname,
        const int         nx,
        const int         ny,
        const int         nz,
        const CLFlags     flags,
        void             *bufferPtr = NULL);

    void deallocateOpenCLObject(const std::string & bname);

    IKernel & getKernel(const std::string & kname);

private:
    void checkSupport();

    struct Buffer;
    struct Device;
    struct Kernel;
    struct KernelContainer;
    struct BufferContainer;
    struct BuildConfiguration;
    struct OpenCLSupport;

    struct Data;
    std::unique_ptr<Data> _data;

    // References

    bool        & _initialized;
    DeviceType  & _deviceType;
    std::string & _kernelPath;

    size_t & _selectedDeviceId;
    std::vector<Device> & _devicesCPU;
    std::vector<Device> & _devicesGPU;

    KernelContainer & _kernels;
    BufferContainer & _buffers;

    KernelTree & _kernelTree;
    BuildConfiguration & _buildConfiguration;
    OpenCLSupport & _support;
};
}
