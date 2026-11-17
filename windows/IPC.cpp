// IPC.cpp - Windows CreatePipe(), CreateFileMapping(), MapViewOfFile()
#include "Modules.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace ipcmgmt {

static SECURITY_ATTRIBUTES inheritable()
{
    SECURITY_ATTRIBUTES sa; sa.nLength = sizeof sa; sa.lpSecurityDescriptor = nullptr; sa.bInheritHandle = TRUE;
    return sa;
}

HANDLE createMapping(size_t bytes, void** view)
{
    SECURITY_ATTRIBUTES sa = inheritable();
    HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, (DWORD)((unsigned long long)bytes >> 32), (DWORD)(bytes & 0xFFFFFFFFu), nullptr);
    if (!h) { logSysErr("CreateFileMapping"); return nullptr; }
    void* v = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    if (!v) { logSysErr("MapViewOfFile"); CloseHandle(h); return nullptr; }
    *view = v;
    return h;
}


void* mapView(HANDLE h, size_t bytes)
{
    void* v = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    if (!v) logSysErr("MapViewOfFile");
    return v;
}

void closeMapping(HANDLE h, void* view)
{
    if (view && !UnmapViewOfFile(view)) logSysErr("UnmapViewOfFile");
    if (h && !CloseHandle(h)) logSysErr("CloseHandle(mapping)");
}

static const char kMsg[] = "sensor-batch-ready";

// child side of the pipe demo:  kalman_windows --pipechild <readHandle> <verbose>
int pipeChildMain(HANDLE r, bool verbose)
{
    char buf[64] = {0}; DWORD n = 0;
    if (!ReadFile(r, buf, sizeof buf - 1, &n, nullptr)) { logSysErr("child ReadFile(pipe)"); return 1; }
    if (verbose) std::printf("       child received via pipe: \"%s\"\n", buf);
    std::fflush(stdout);
    CloseHandle(r);
    return (n == sizeof kMsg && std::strcmp(buf, kMsg) == 0) ? 0 : 2;
}

// child side of the shared-memory demo:  kalman_windows --shmchild <mappingHandle> <verbose>
int shmChildMain(HANDLE m, bool verbose)
{
    double* d = (double*)mapView(m, 4096);
    if (!d) return 1;
    bool good = true;
    for (int i = 0; i < 5; ++i) if (d[i] != 10.5 * (i + 1)) good = false;
    if (verbose) std::printf("       child read from shared memory: %.1f %.1f %.1f %.1f %.1f\n", d[0], d[1], d[2], d[3], d[4]);
    std::fflush(stdout);
    UnmapViewOfFile(d); CloseHandle(m);
    return good ? 0 : 2;
}

static std::string hs(HANDLE h) { return std::to_string((unsigned long long)(uintptr_t)h); }

void run(PerfTable& pt, int, bool verbose)
{
    Timer t; DWORD code = 0;
    std::string vflag = verbose ? "1" : "0";

    // ---------- pipe: parent -> child process ----------
    SECURITY_ATTRIBUTES sa = inheritable();
    HANDLE r = nullptr, w = nullptr;
    t.start(); BOOL ok = CreatePipe(&r, &w, &sa, 0); double msPipe = t.stopMs();
    if (!ok) { logSysErr("CreatePipe"); return; }
    PROCESS_INFORMATION pi;
    if (!procmgmt::spawnSelf("--pipechild " + hs(r) + " " + vflag, pi)) { CloseHandle(r); CloseHandle(w); return; }
    DWORD wrote = 0;
    t.start(); ok = WriteFile(w, kMsg, sizeof kMsg, &wrote, nullptr); double msPipeWrite = t.stopMs();
    if (!ok) logSysErr("WriteFile(pipe)");
    CloseHandle(w); CloseHandle(r);
    bool waited = procmgmt::waitFor(pi, code);
    if (!ok || !waited || code != 0) { logMsg(Lvl::ERR, "pipe communication failed (child exit code " + std::to_string(code) + ")"); return; }

    // ---------- shared memory: parent writes, child process reads ----------
    SECURITY_ATTRIBUTES sa2 = inheritable();
    t.start(); HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, &sa2, PAGE_READWRITE, 0, 4096, nullptr); double msCreate = t.stopMs();
    if (!m) { logSysErr("CreateFileMapping"); return; }
    t.start(); void* view = MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 4096); double msMap = t.stopMs();
    if (!view) { logSysErr("MapViewOfFile"); CloseHandle(m); return; }
    double* d = (double*)view;
    for (int i = 0; i < 5; ++i) d[i] = 10.5 * (i + 1);
    if (!procmgmt::spawnSelf("--shmchild " + hs(m) + " " + vflag, pi)) { closeMapping(m, view); return; }
    waited = procmgmt::waitFor(pi, code);
    closeMapping(m, view);
    if (!waited || code != 0) { logMsg(Lvl::ERR, "shared-memory communication failed (child exit code " + std::to_string(code) + ")"); return; }

    pt.add("IPC", "CreatePipe", msPipe);               pt.add("IPC", "WriteFile (to pipe)", msPipeWrite);
    pt.add("IPC", "CreateFileMapping", msCreate);      pt.add("IPC", "MapViewOfFile", msMap);
    if (verbose) {
        logMsg(Lvl::OK, "CreatePipe(): child process read the message the parent wrote");
        logMsg(Lvl::OK, "CreateFileMapping()/MapViewOfFile(): child process read the values the parent stored; mapping closed");
    }
}

} // namespace ipcmgmt
