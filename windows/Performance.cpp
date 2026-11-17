// Performance.cpp - logging and performance-table helpers (Windows)
#include "Modules.h"
#include <cstdio>
#include <algorithm>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

static std::string g_log;

void logMsg(Lvl lvl, const std::string& msg)
{
    const char* tag = lvl == Lvl::OK ? "[ OK ]" : lvl == Lvl::ERR ? "[FAIL]" : "[INFO]";
    std::string line = std::string(tag) + " " + msg + "\n";
    std::fputs(line.c_str(), lvl == Lvl::ERR ? stderr : stdout);
    std::fflush(stdout);
    g_log += line;
}

void logSysErr(const std::string& call)
{
    DWORD e = GetLastError();
    char* text = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, e, 0, (LPSTR)&text, 0, nullptr);
    std::string t = text ? text : "(no message)";
    if (text) LocalFree(text);
    while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' ')) t.pop_back();
    logMsg(Lvl::ERR, call + " failed: GetLastError=" + std::to_string(e) + " (" + t + ")");
}

bool saveLog(const std::string& path) { return fileio::writeWholeFile(path, g_log); }

void PerfTable::add(const std::string& category, const std::string& op, double ms)
{
    for (auto& r : rows) if (r.op == op) { r.ms.push_back(ms); return; }
    rows.push_back({category, op, {ms}});
}

static double average(const std::vector<double>& v)
{
    double s = 0; for (double x : v) s += x;
    return v.empty() ? 0.0 : s / (double)v.size();
}

void PerfTable::print() const
{
    std::printf("\n%-12s %-34s", "Category", "Operation");
    size_t n = 0; for (auto& r : rows) n = std::max(n, r.ms.size());
    for (size_t i = 0; i < n; ++i) std::printf(" %10s", ("Run" + std::to_string(i + 1)).c_str());
    std::printf(" %12s\n", "Average(ms)");
    for (auto& r : rows) {
        std::printf("%-12s %-34s", r.category.c_str(), r.op.c_str());
        for (double x : r.ms) std::printf(" %10.6f", x);
        std::printf(" %12.6f\n", average(r.ms));
    }
    std::fflush(stdout);
}

std::string PerfTable::toCsv() const
{
    size_t n = 0; for (auto& r : rows) n = std::max(n, r.ms.size());
    std::string s = "Category,Operation";
    for (size_t i = 0; i < n; ++i) s += ",Run" + std::to_string(i + 1) + "_ms";
    s += ",Average_ms\n";
    char buf[40];
    for (auto& r : rows) {
        s += r.category + "," + r.op;
        for (double x : r.ms) { std::snprintf(buf, sizeof buf, ",%.6f", x); s += buf; }
        std::snprintf(buf, sizeof buf, ",%.6f\n", average(r.ms)); s += buf;
    }
    return s;
}

void logResourceUsage()
{
    FILETIME c, e, k, u;
    if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) { logSysErr("GetProcessTimes"); return; }
    double ms_k = (double)(((ULONGLONG)k.dwHighDateTime << 32) | k.dwLowDateTime) / 10000.0;
    double ms_u = (double)(((ULONGLONG)u.dwHighDateTime << 32) | u.dwLowDateTime) / 10000.0;
    PROCESS_MEMORY_COUNTERS pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) { logSysErr("GetProcessMemoryInfo"); return; }
    char b[260];
    std::snprintf(b, sizeof b, "process-level resources (whole process, NOT one call): user CPU %.3f ms, kernel CPU %.3f ms, peak working set %.1f KB, peak pagefile %.1f KB",
                  ms_u, ms_k, pmc.PeakWorkingSetSize / 1024.0, pmc.PeakPagefileUsage / 1024.0);
    logMsg(Lvl::INFO, b);
}
