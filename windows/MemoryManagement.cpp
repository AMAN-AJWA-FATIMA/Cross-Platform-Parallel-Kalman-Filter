// MemoryManagement.cpp - Windows VirtualAlloc() / VirtualFree()
// VirtualAlloc gives PRIVATE virtual memory (allocation). Memory that several processes share
// is created with CreateFileMapping/MapViewOfFile (see IPC.cpp).
#include "Modules.h"

namespace memmgmt {

void* allocPrivate(size_t bytes)
{
    void* p = VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p) logSysErr("VirtualAlloc");
    return p;
}

void freePrivate(void* p)
{
    if (p && !VirtualFree(p, 0, MEM_RELEASE)) logSysErr("VirtualFree");
}

void run(PerfTable& pt, int, bool verbose)
{
    Timer t; const size_t bytes = 1 << 20;
    t.start(); void* p = VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); double msAlloc = t.stopMs();
    if (!p) { logSysErr("VirtualAlloc"); return; }
    int* a = (int*)p; size_t n = bytes / sizeof(int);
    for (size_t i = 0; i < n; ++i) a[i] = (int)(i * 3);
    bool good = true;
    for (size_t i = 0; i < n; ++i) if (a[i] != (int)(i * 3)) { good = false; break; }
    t.start(); BOOL ok = VirtualFree(p, 0, MEM_RELEASE); double msFree = t.stopMs();
    if (!ok) { logSysErr("VirtualFree"); return; }
    if (!good) { logMsg(Lvl::ERR, "memory read-back mismatch"); return; }
    pt.add("Memory", "VirtualAlloc (1MiB)", msAlloc);
    pt.add("Memory", "VirtualFree (1MiB)", msFree);
    if (verbose) logMsg(Lvl::OK, "VirtualAlloc 1 MiB: wrote and verified " + std::to_string(n) + " ints, then VirtualFree OK");
}

} // namespace memmgmt
