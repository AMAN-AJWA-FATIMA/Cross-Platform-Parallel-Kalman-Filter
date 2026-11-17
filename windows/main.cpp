// main.cpp - Windows entry point.  Run from the project root (Operating_System_CCA\).
//   kalman_windows.exe bench <fileio|process|memory|ipc|permissions|all>
//   kalman_windows.exe kalman [workers] [inputFile]
//   kalman_windows.exe gen <rows> <cols> <file>
#include "Modules.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

std::string g_inputPath = "input\\sensor_data.csv";
std::string g_outDir    = "output\\windows\\";

static const int RUNS = 5;

static void usage()
{
    std::puts("Usage:\n  kalman_windows bench <fileio|process|memory|ipc|permissions|all>\n"
              "  kalman_windows kalman [workers] [inputFile]\n  kalman_windows gen <rows> <cols> <file>");
}

static bool benchModule(const std::string& name)
{
    PerfTable pt;
    for (int r = 1; r <= RUNS; ++r) {
        bool v = (r == 1);
        if      (name == "fileio")      fileio::run(pt, r, v);
        else if (name == "process")     procmgmt::run(pt, r, v);
        else if (name == "memory")      memmgmt::run(pt, r, v);
        else if (name == "ipc")         ipcmgmt::run(pt, r, v);
        else if (name == "permissions") perms::run(pt, r, v);
        else return false;
    }
    pt.print();
    logResourceUsage();
    std::string csv = g_outDir + "performance_" + name + ".csv";
    if (fileio::writeWholeFile(csv, pt.toCsv())) logMsg(Lvl::OK, "saved " + csv);
    saveLog(g_outDir + name + ".log");
    return true;
}

static HANDLE handleArg(const char* s) { return (HANDLE)(uintptr_t)std::strtoull(s, nullptr, 10); }

int main(int argc, char** argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "--worker") == 0)   return procmgmt::workerMain(argc, argv);
    if (argc >= 4 && std::strcmp(argv[1], "--pipechild") == 0) return ipcmgmt::pipeChildMain(handleArg(argv[2]), argv[3][0] == '1');
    if (argc >= 4 && std::strcmp(argv[1], "--shmchild") == 0)  return ipcmgmt::shmChildMain(handleArg(argv[2]), argv[3][0] == '1');
    if (argc < 2) { usage(); return 1; }
    std::string mode = argv[1];

    if (mode == "bench" && argc >= 3) {
        std::string m = argv[2];
        if (m == "all") {
            for (const char* s : {"fileio", "process", "memory", "ipc", "permissions"}) benchModule(s);
            return 0;
        }
        if (!benchModule(m)) { usage(); return 1; }
        return 0;
    }
    if (mode == "kalman") {
        int w = argc >= 3 ? std::atoi(argv[2]) : 4;
        if (w < 1) w = 1;
        if (argc >= 4) g_inputPath = argv[3];
        int rc = procmgmt::kalmanParallel(w);
        logResourceUsage();   // parent process only; worker processes are separate processes
        return rc;
    }
    if (mode == "gen" && argc >= 5) {
        int rows = std::atoi(argv[2]), cols = std::atoi(argv[3]);
        if (rows < 2 || cols < 1) { usage(); return 1; }
        std::mt19937 rng(42); std::normal_distribution<double> noise(0.0, 1.0);
        std::string s; char b[32];
        for (int c = 0; c < cols; ++c) { std::snprintf(b, sizeof b, "s%d%s", c + 1, c < cols - 1 ? "," : "\n"); s += b; }
        for (int t = 0; t < rows; ++t)
            for (int c = 0; c < cols; ++c) { std::snprintf(b, sizeof b, "%.2f%s", 20.0 + 5.0 * c + noise(rng), c < cols - 1 ? "," : "\n"); s += b; }
        return fileio::writeWholeFile(argv[4], s) ? 0 : 1;
    }
    usage();
    return 1;
}
