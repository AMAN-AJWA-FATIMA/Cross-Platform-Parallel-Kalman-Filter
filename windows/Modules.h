// Modules.h - declarations shared by all Windows modules (included by every windows/*.cpp file)
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include "../common/KalmanFilter.h"

extern std::string g_inputPath;   // defined in main.cpp (default input\sensor_data.csv)
extern std::string g_outDir;      // defined in main.cpp (default output\windows\)

// ---- Timer (std::chrono::steady_clock, result in milliseconds) ----
class Timer {
    std::chrono::steady_clock::time_point t0_;
public:
    void start() { t0_ = std::chrono::steady_clock::now(); }
    double stopMs() const {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0_).count();
    }
};

// ---- Logging (Performance.cpp) ----
enum class Lvl { INFO, OK, ERR };
void logMsg(Lvl lvl, const std::string& msg);
void logSysErr(const std::string& call);          // logs GetLastError() + Windows message text
bool saveLog(const std::string& path);
void logResourceUsage();                          // CPU time + peak memory of THIS process

// ---- Performance table (Performance.cpp) ----
struct PerfRow { std::string category, op; std::vector<double> ms; };
struct PerfTable {
    std::vector<PerfRow> rows;
    void add(const std::string& category, const std::string& op, double ms);
    void print() const;
    std::string toCsv() const;
};

// ---- A. File I/O: CreateFile/ReadFile/WriteFile/CloseHandle (FileIO.cpp) ----
namespace fileio {
    bool readWholeFile(const std::string& path, std::string& out);
    bool writeWholeFile(const std::string& path, const std::string& data);
    void run(PerfTable& pt, int run, bool verbose);
}
// ---- B. Processes: CreateProcess/WaitForSingleObject + parallel Kalman (ProcessManagement.cpp) ----
namespace procmgmt {
    void run(PerfTable& pt, int run, bool verbose);
    bool spawnSelf(const std::string& args, PROCESS_INFORMATION& pi);   // CreateProcess of this same .exe
    bool waitFor(PROCESS_INFORMATION& pi, DWORD& exitCode);             // WaitForSingleObject + close handles
    int  kalmanParallel(int workers);
    int  workerMain(int argc, char** argv);
}
// ---- C. Memory: VirtualAlloc/VirtualFree (MemoryManagement.cpp) ----
namespace memmgmt {
    void  run(PerfTable& pt, int run, bool verbose);
    void* allocPrivate(size_t bytes);
    void  freePrivate(void* p);
}
// ---- D. IPC: CreatePipe/CreateFileMapping/MapViewOfFile (IPC.cpp) ----
namespace ipcmgmt {
    void   run(PerfTable& pt, int run, bool verbose);
    HANDLE createMapping(size_t bytes, void** view);     // inheritable, backed by the paging file
    void*  mapView(HANDLE h, size_t bytes);
    void   closeMapping(HANDLE h, void* view);
    int    pipeChildMain(HANDLE readEnd, bool verbose);
    int    shmChildMain(HANDLE mapping, bool verbose);
}
// ---- E. Permissions: SetFileSecurity (Permissions.cpp) ----
namespace perms {
    void run(PerfTable& pt, int run, bool verbose);
    bool setUsersReadOnly(const std::string& path);
    bool setUsersFullControl(const std::string& path);
}
