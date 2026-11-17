// ProcessManagement.cpp - Windows CreateProcess(), WaitForSingleObject()
// plus the integrated multi-process Kalman filter (file mapping + pipe + file I/O + VirtualAlloc).
#include "Modules.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace procmgmt {

static std::string selfPath()
{
    char p[MAX_PATH * 2];
    DWORD n = GetModuleFileNameA(nullptr, p, sizeof p);
    return std::string(p, n);
}

// CreateProcess of this same executable with extra arguments (inherits inheritable handles).
bool spawnSelf(const std::string& args, PROCESS_INFORMATION& pi)
{
    std::string cmd = "\"" + selfPath() + "\" " + args;
    std::vector<char> buf(cmd.begin(), cmd.end()); buf.push_back('\0');
    STARTUPINFOA si; ZeroMemory(&si, sizeof si); si.cb = sizeof si;
    ZeroMemory(&pi, sizeof pi);
    if (!CreateProcessA(nullptr, buf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        logSysErr("CreateProcess(" + args + ")"); return false;
    }
    return true;
}

bool waitFor(PROCESS_INFORMATION& pi, DWORD& code)
{
    bool ok = true; code = 1;
    if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0) { logSysErr("WaitForSingleObject"); ok = false; }
    else if (!GetExitCodeProcess(pi.hProcess, &code)) { logSysErr("GetExitCodeProcess"); ok = false; }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return ok;
}

// ---------- benchmark / demo of CreateProcess + WaitForSingleObject ----------
void run(PerfTable& pt, int, bool verbose)
{
    Timer total, t;
    char cmd[] = "cmd.exe /c echo hello from child program (CreateProcess)";
    STARTUPINFOA si; ZeroMemory(&si, sizeof si); si.cb = sizeof si;
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof pi);
    std::fflush(stdout);
    total.start();
    t.start(); BOOL ok = CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi); double msCreate = t.stopMs();
    if (!ok) { logSysErr("CreateProcess(cmd.exe)"); return; }
    t.start(); DWORD w = WaitForSingleObject(pi.hProcess, INFINITE); double msWait = t.stopMs();
    double msTotal = total.stopMs();
    DWORD code = 1;
    if (w != WAIT_OBJECT_0) { logSysErr("WaitForSingleObject"); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return; }
    if (!GetExitCodeProcess(pi.hProcess, &code)) logSysErr("GetExitCodeProcess");
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    if (code != 0) { logMsg(Lvl::ERR, "child exit code " + std::to_string(code)); return; }
    pt.add("Process", "CreateProcess", msCreate);
    pt.add("Process", "WaitForSingleObject (blocked until child exit)", msWait);
    pt.add("Process", "CreateProcess+Wait (total)", msTotal);
    if (verbose) logMsg(Lvl::OK, "parent pid " + std::to_string(GetCurrentProcessId()) + " created child pid " + std::to_string(pi.dwProcessId) +
                        "; child ran cmd.exe; WaitForSingleObject returned, exit code 0");
}

// ---------- worker: kalman_windows --worker <mapH> <pipeH> <rows> <cols> <first> <last> <idx> ----------
int workerMain(int argc, char** argv)
{
    if (argc < 9) { std::fprintf(stderr, "worker: bad arguments\n"); return 2; }
    HANDLE map = (HANDLE)(uintptr_t)std::strtoull(argv[2], nullptr, 10);
    HANDLE pipeW = (HANDLE)(uintptr_t)std::strtoull(argv[3], nullptr, 10);
    int rows = std::atoi(argv[4]), cols = std::atoi(argv[5]), first = std::atoi(argv[6]), last = std::atoi(argv[7]), idx = std::atoi(argv[8]);
    size_t n = (size_t)rows * cols;
    double* arr = (double*)ipcmgmt::mapView(map, (2 * n + 64) * sizeof(double));
    if (!arr) return 3;
    double* slots = arr + 2 * n;                         // timing slots live at the end of the mapping
    KalmanParams kp; Timer t; t.start();
    for (int ch = first; ch < last; ++ch) kalmanFilterChannel(arr, arr + n, rows, cols, ch, kp);
    double ms = t.stopMs();
    slots[idx] = ms;
    char msg[160];
    int len = std::snprintf(msg, sizeof msg, "worker %d (pid %lu): channels %d..%d filtered in %.4f ms\n", idx, GetCurrentProcessId(), first + 1, last, ms);
    DWORD wrote = 0;
    int rc = (WriteFile(pipeW, msg, (DWORD)len, &wrote, nullptr) && (int)wrote == len) ? 0 : 5;
    CloseHandle(pipeW);
    UnmapViewOfFile(arr); CloseHandle(map);
    return rc;
}

// ---------- integrated application ----------
int kalmanParallel(int workers)
{
    Timer t, wall; wall.start();
    std::string text;
    t.start(); if (!fileio::readWholeFile(g_inputPath, text)) return 1; double msRead = t.stopMs();
    std::vector<double> data; int rows = 0, cols = 0; std::string err;
    t.start(); if (!parseCsv(text, data, rows, cols, err)) { logMsg(Lvl::ERR, "parse: " + err); return 1; } double msParse = t.stopMs();
    logMsg(Lvl::OK, "read " + std::to_string(text.size()) + " bytes via CreateFile/ReadFile/CloseHandle; " + std::to_string(rows) + " rows x " + std::to_string(cols) + " sensor channels");

    size_t n = (size_t)rows * cols, bytes = (2 * n + 64) * sizeof(double);
    void* view = nullptr;
    t.start();
    HANDLE map = ipcmgmt::createMapping(bytes, &view);
    if (!map) return 1;
    double* arr = (double*)view;
    std::memcpy(arr, data.data(), n * sizeof(double));
    double msMap = t.stopMs();
    logMsg(Lvl::OK, "CreateFileMapping + MapViewOfFile: shared [input array | output array | timing slots], " + std::to_string(bytes) + " bytes");

    int W = std::max(1, std::min(std::min(workers, cols), 64));   // 64 timing slots
    SECURITY_ATTRIBUTES sa; sa.nLength = sizeof sa; sa.lpSecurityDescriptor = nullptr; sa.bInheritHandle = TRUE;
    HANDLE pr = nullptr, pw = nullptr;
    if (!CreatePipe(&pr, &pw, &sa, 0)) { logSysErr("CreatePipe"); ipcmgmt::closeMapping(map, view); return 1; }

    std::vector<PROCESS_INFORMATION> procs;
    t.start();
    for (int i = 0; i < W; ++i) {
        int first = i * cols / W, last = (i + 1) * cols / W;
        std::string a = "--worker " + std::to_string((unsigned long long)(uintptr_t)map) + " " + std::to_string((unsigned long long)(uintptr_t)pw) + " " +
                        std::to_string(rows) + " " + std::to_string(cols) + " " + std::to_string(first) + " " + std::to_string(last) + " " + std::to_string(i);
        PROCESS_INFORMATION pi;
        if (!spawnSelf(a, pi)) break;
        procs.push_back(pi);
    }
    CloseHandle(pw);                                     // parent's write end; children keep theirs
    std::string msgs; char buf[256]; DWORD got = 0;
    for (;;) {
        if (!ReadFile(pr, buf, sizeof buf, &got, nullptr)) {
            if (GetLastError() != ERROR_BROKEN_PIPE) logSysErr("ReadFile(pipe)");   // BROKEN_PIPE = all writers finished
            break;
        }
        if (got == 0) break;
        msgs.append(buf, got);
    }
    CloseHandle(pr);
    bool ok = ((int)procs.size() == W);
    for (auto& pi : procs) {
        DWORD code = 1;
        if (!waitFor(pi, code) || code != 0) { logMsg(Lvl::ERR, "worker failed, exit code " + std::to_string(code)); ok = false; }
    }
    double msPar = t.stopMs();
    std::fputs(msgs.c_str(), stdout);
    int rc = 1;
    if (ok) {
        logMsg(Lvl::OK, std::to_string(W) + " worker process(es) finished (CreateProcess/WaitForSingleObject); results received via pipe + shared memory");
        double* ref = (double*)memmgmt::allocPrivate(n * sizeof(double));       // VirtualAlloc
        if (ref) {
            KalmanParams kp; t.start();
            for (int ch = 0; ch < cols; ++ch) kalmanFilterChannel(arr, ref, rows, cols, ch, kp);
            double msSeq = t.stopMs();
            double maxDiff = 0; for (size_t i = 0; i < n; ++i) maxDiff = std::max(maxDiff, std::fabs(ref[i] - arr[n + i]));
            logMsg(maxDiff == 0.0 ? Lvl::OK : Lvl::ERR, "parallel output vs sequential output: max |difference| = " + std::to_string(maxDiff));
            memmgmt::freePrivate(ref);                                          // VirtualFree

            std::string outPath = g_outDir + "kalman_output.csv";
            perms::setUsersFullControl(outPath);       // lets a re-run overwrite the file (fails harmlessly if it does not exist yet)
            t.start();
            bool wrote = fileio::writeWholeFile(outPath, formatCsv(arr + n, rows, cols, "f"));
            double msWrite = t.stopMs();
            if (wrote) {
                logMsg(Lvl::OK, "filtered output written with CreateFile/WriteFile/CloseHandle: " + outPath);
                if (perms::setUsersReadOnly(outPath)) logMsg(Lvl::OK, "SetFileSecurity: output file is now read-only for Users");
                double wmax = 0; for (int i = 0; i < W; ++i) wmax = std::max(wmax, arr[2 * n + i]);
                char line[600];
                std::snprintf(line, sizeof line,
                    "\nTIMING SUMMARY (ms)  rows=%d cols=%d workers=%d\n"
                    "  read file (CreateFile/ReadFile/CloseHandle) : %.4f\n  parse CSV                   : %.4f\n"
                    "  file mapping + copy input   : %.4f\n"
                    "  parallel phase (create..wait): %.4f   (slowest worker compute: %.4f)\n"
                    "  sequential compute (1 proc) : %.4f\n  write output file           : %.4f\n"
                    "  sequential/parallel ratio   : %.3f   (>1 means parallel was faster; <1 means slower)\n  total wall time             : %.4f\n",
                    rows, cols, W, msRead, msParse, msMap, msPar, wmax, msSeq, msWrite, msSeq / msPar, wall.stopMs());
                std::fputs(line, stdout); std::fflush(stdout);
                rc = (maxDiff == 0.0) ? 0 : 1;
            }
        }
    }
    ipcmgmt::closeMapping(map, view);
    saveLog(g_outDir + "kalman_run.log");
    return rc;
}

} // namespace procmgmt
