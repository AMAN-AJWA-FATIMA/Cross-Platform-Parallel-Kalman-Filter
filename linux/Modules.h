// Modules.h - declarations shared by all Linux modules (included by every linux/*.cpp file)
#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <cstddef>
#include "../common/KalmanFilter.h"

extern std::string g_inputPath;   // defined in main.cpp (default input/sensor_data.csv)
extern std::string g_outDir;      // defined in main.cpp (default output/linux/)

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
void logSysErr(const std::string& call);          // logs errno + strerror
bool saveLog(const std::string& path);            // writes the in-memory log with write()
void logResourceUsage();                          // CPU time + peak memory of THIS process (and its children)

// ---- Performance table (Performance.cpp) ----
struct PerfRow { std::string category, op; std::vector<double> ms; };
struct PerfTable {
    std::vector<PerfRow> rows;
    void add(const std::string& category, const std::string& op, double ms);
    void print() const;
    std::string toCsv() const;
};

// ---- A. File I/O: open/read/write/close (FileIO.cpp) ----
namespace fileio {
    bool readWholeFile(const std::string& path, std::string& out);
    bool writeWholeFile(const std::string& path, const std::string& data, unsigned mode = 0644);
    void run(PerfTable& pt, int run, bool verbose);
}
// ---- B. Processes: fork/exec/wait + parallel Kalman (ProcessManagement.cpp) ----
namespace procmgmt {
    void run(PerfTable& pt, int run, bool verbose);
    int  kalmanParallel(int workers);                 // integrated application
    int  workerMain(int argc, char** argv);           // child program started with execv
}
// ---- C. Memory: mmap/munmap (MemoryManagement.cpp) ----
namespace memmgmt {
    void  run(PerfTable& pt, int run, bool verbose);
    void* allocPrivate(size_t bytes);                 // anonymous private mmap
    void  freePrivate(void* p, size_t bytes);         // munmap
    void* mapSharedFile(const std::string& path, size_t bytes, bool create);
}
// ---- D. IPC: pipe/shmget/shmat (IPC.cpp) ----
namespace ipcmgmt {
    void  run(PerfTable& pt, int run, bool verbose);
    void* shmCreate(size_t bytes, int* idOut);        // shmget + shmat
    void* shmAttach(int id);                          // shmat
    void  shmDestroy(void* addr, int id);             // shmdt + shmctl(IPC_RMID)
    void  shmDetach(void* addr);
}
// ---- E. Permissions: chmod/chown/umask (Permissions.cpp) ----
namespace perms {
    void run(PerfTable& pt, int run, bool verbose);
    bool applyOutputPermissions(const std::string& path);
}
