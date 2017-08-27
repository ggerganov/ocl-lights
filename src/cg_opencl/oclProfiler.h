/*! \file oclProfiler.h
 *  \brief Timing profiling tool.
 *  \author Georgi Gerganov
 */

#pragma once

#include <string>
#include <map>

#define OCL_PROFILING

#ifdef OCL_PROFILING
    #define _OCL_P_ OCL::Profiler::getInstance()
    #define OCL_PROFILING_SET_PARAMETERS(ID, TITLE, FREQ, SKIP) _OCL_P_.setParams(ID, TITLE, FREQ, SKIP);
    #define OCL_PROFILING_START(ID, COND) if (COND) _OCL_P_.start(ID);
    #define OCL_PROFILING_STOP(ID, COND) if (COND) _OCL_P_.stop(ID);
    #define OCL_PROFILING_PRINT(ID, FORCE) _OCL_P_.print(ID, FORCE);
    #define OCL_PROFILING_PRINT_ALL(FORCE) _OCL_P_.printAll(FORCE);
    #define OCL_PROFILING_PRINT_ALL_COND(COND) if (COND) _OCL_P_.printAll(true);
    #define OCL_PROFILING_RESET_ALL() _OCL_P_.resetAll();
#else
    #define OCL_PROFILING_SET_PARAMETERS(ID, TITLE, FREQ, SKIP)
    #define OCL_PROFILING_START(ID, COND)
    #define OCL_PROFILING_STOP(ID, COND)
    #define OCL_PROFILING_PRINT(ID, FORCE)
    #define OCL_PROFILING_PRINT_ALL(FORCE)
    #define OCL_PROFILING_PRINT_ALL_COND(COND)
    #define OCL_PROFILING_RESET_ALL()
#endif

namespace CG {
class Timer;
}

namespace OCL {
class Profiler {
public:
    Profiler();

    static Profiler & getInstance() {
        static Profiler instance;
        return instance;
    }

    void setParams(const int id, const char *title, const int freq, const int skip);
    void start    (const int id);
    void stop     (const int id);
    void print    (const int id, const bool force = false);

    void setParams(const std::string &id, const char *title, const int freq, const int skip);
    void start    (const std::string &id);
    void stop     (const std::string &id);
    void print    (const std::string &id, const bool force = false);

    void printAll (const bool force = false);

    void resetAll();

private:

    class PrivateProfiler {
    public:
        PrivateProfiler();
        ~PrivateProfiler();

        void reset();

        bool _isRunning;

        int _n;
        int _NN;
        int _freq;
        int _skip;

        float _min;
        float _max;
        float _sum;
        float _sum2;

        std::string _title;

        CG::Timer *_timer;
    };

    bool _printHeader;
    bool _printFooter;

    std::map<int, PrivateProfiler *>         _profilersInt;
    std::map<std::string, PrivateProfiler *> _profilersStr;

};

}
