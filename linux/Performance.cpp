// Performance.cpp - logging and performance-table helpers (Linux)
#include "Modules.h"
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <sys/resource.h>

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
    int e = errno;
    logMsg(Lvl::ERR, call + " failed: errno=" + std::to_string(e) + " (" + std::strerror(e) + ")");
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
    struct rusage self, kids;
    if (getrusage(RUSAGE_SELF, &self) < 0) { logSysErr("getrusage(RUSAGE_SELF)"); return; }
    if (getrusage(RUSAGE_CHILDREN, &kids) < 0) { logSysErr("getrusage(RUSAGE_CHILDREN)"); return; }
    auto ms = [](const struct timeval& tv) { return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0; };
    char b[300];
    std::snprintf(b, sizeof b, "process-level resources (whole process, NOT one call): user CPU %.3f ms, system CPU %.3f ms, peak RSS %ld KB",
                  ms(self.ru_utime), ms(self.ru_stime), self.ru_maxrss);
    logMsg(Lvl::INFO, b);
    std::snprintf(b, sizeof b, "terminated children combined: user CPU %.3f ms, system CPU %.3f ms, largest child peak RSS %ld KB",
                  ms(kids.ru_utime), ms(kids.ru_stime), kids.ru_maxrss);
    logMsg(Lvl::INFO, b);
}
