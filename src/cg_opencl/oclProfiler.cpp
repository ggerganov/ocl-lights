/*! \file oclProfiler.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#include "cg_opencl/oclProfiler.h"

#include "cg_logger.h"
#include "cg_timer.h"

#include <cmath>

namespace OCL {

Profiler::Profiler() {
    _printHeader = false;
    _printFooter = false;
}

// Profilers Int
void Profiler::setParams(const int id, const char *title, const int freq, const int skip) {
    PrivateProfiler *p = new PrivateProfiler();
    if (_profilersInt[id]) delete _profilersInt[id];
    _profilersInt[id] = p;

    p->_title = title;
    p->_freq = freq;
    p->_skip = skip;
}

void Profiler::start(const int id) {
    PrivateProfiler *p = _profilersInt[id];
    if (!p) return;
    if (p->_isRunning) {
        CG_FATAL(0, "Profiler %d ('%s') is already started.\n", id, p->_title.c_str());
        return;
    }

    p->_isRunning = true;
    p->_timer->start();
}

void Profiler::stop(const int id) {
    PrivateProfiler *p = _profilersInt[id];
    if (!p) return;
    if (!p->_isRunning) {
        CG_FATAL(0, "Profiler %d ('%s') is not started.\n", id, p->_title.c_str());
        return;
    }

    float t = p->_timer->time();
    p->_isRunning = false;
    p->_NN++;
    if (p->_NN > p->_skip) {
        p->_n++;
        if (t < p->_min) p->_min = t;
        if (t > p->_max) p->_max = t;
        p->_sum += t;
        p->_sum2 += t*t;
    }
}

void Profiler::print(const int id, const bool force) {
    PrivateProfiler *p = _profilersInt[id];
    if (!p) return;

    if ((!force && (p->_n % p->_freq != 0)) || (p->_n == 0)) return;

    if (_printHeader) {
        _printHeader = false;
        _printFooter = true;
        CG_INFO(0, "[T] Timing information: %41s %5s %9s %9s %9s +/- %9s %9s\n",
                "", "N", "Min", "Max", "Avg", "DT", "Tot");
    }

    float avg   = (p->_n == 0) ? 0.0 : p->_sum / p->_n;
    float delta = (p->_n < 2)  ? 0.0 : sqrt(p->_sum2/p->_n - avg*avg);
    CG_INFO(0, "[T] Profiler %44d for '%s': %5d %9.4f %9.4f %9.5f +/- %9.4f %9.4f\n",
            id, p->_title.c_str(), p->_n, p->_min, p->_max, avg, delta, p->_sum);
}

// Profilers Str
void Profiler::setParams(const std::string &id, const char *title, const int freq, const int skip) {
    PrivateProfiler *p = new PrivateProfiler();
    if (_profilersStr[id]) delete _profilersStr[id];
    _profilersStr[id] = p;

    p->_title = title;
    p->_freq = freq;
    p->_skip = skip;
}

void Profiler::start(const std::string &id) {
    PrivateProfiler *p = _profilersStr[id];
    if (!p) return;
    if (p->_isRunning) {
        CG_FATAL(0, "Profiler '%s' ('%s') is already started.\n", id.c_str(), p->_title.c_str());
        return;
    }

    p->_isRunning = true;
    p->_timer->start();
}

void Profiler::stop(const std::string &id) {
    PrivateProfiler *p = _profilersStr[id];
    if (!p) return;
    if (!p->_isRunning) {
        CG_FATAL(0, "Profiler '%s' ('%s') is not started.\n", id.c_str(), p->_title.c_str());
        return;
    }

    float t = p->_timer->time();
    p->_isRunning = false;
    p->_NN++;

    if (p->_NN > p->_skip) {
        p->_n++;
        if (t < p->_min) p->_min = t;
        if (t > p->_max) p->_max = t;
        p->_sum += t;
        p->_sum2 += t*t;
    }
}

void Profiler::print(const std::string &id, const bool force) {
    PrivateProfiler *p = _profilersStr[id];
    if (!p) return;

    if ((!force && (p->_n % p->_freq != 0)) || (p->_n == 0)) return;

    if (_printHeader) {
        _printHeader = false;
        _printFooter = true;
        CG_INFO(0, "[T] Timing information: %41s %5s %9s %9s %9s +/- %9s %9s\n",
                "", "N", "Min", "Max", "Avg", "DT", "Tot");
    }

    float avg   = (p->_n == 0) ? 0.0 : p->_sum / p->_n;
    float delta = (p->_n < 2)  ? 0.0 : sqrt(p->_sum2/p->_n - avg*avg);
    CG_INFO(0, "[T] Profiler %44s for '%s': %5d %9.4f %9.4f %9.5f +/- %9.4f %9.4f\n",
            id.c_str(), p->_title.c_str(), p->_n, p->_min, p->_max, avg, delta, p->_sum);
}

void Profiler::printAll(const bool force) {
    _printHeader = true;
    _printFooter = false;

    for (std::map<int, PrivateProfiler *>::iterator it = _profilersInt.begin();
         it != _profilersInt.end(); ++it) {
        print(it->first, force);
    }

    for (std::map<std::string, PrivateProfiler *>::iterator it = _profilersStr.begin();
         it != _profilersStr.end(); ++it) {
        print(it->first, force);
    }

    if (_printFooter) {
        CG_INFO(0, "[T]\n");
    }
}

void Profiler::resetAll() {
    for (std::map<int, PrivateProfiler *>::iterator it = _profilersInt.begin();
         it != _profilersInt.end(); ++it) {
        PrivateProfiler *p = it->second;
        if (p) p->reset();
    }

    for (std::map<std::string, PrivateProfiler *>::iterator it = _profilersStr.begin();
         it != _profilersStr.end(); ++it) {
        PrivateProfiler *p = it->second;
        if (p) p->reset();
    }
}


Profiler::PrivateProfiler::PrivateProfiler() {
    reset();

    _title = "noname";

    _timer = new CG::Timer();
}

void Profiler::PrivateProfiler::reset() {
    _isRunning = false;

    _n = 0;
    _NN = 0;
    _freq = 1;
    _skip = 0;

    _min = 1e6;
    _max = 0.0;
    _sum = 0.0;
    _sum2 = 0.0;
}

Profiler::PrivateProfiler::~PrivateProfiler() {
    if (_timer) delete _timer;
}

}
