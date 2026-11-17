// MemoryManagement.cpp - Linux mmap() / munmap()
#include "Modules.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace memmgmt {

void* allocPrivate(size_t bytes)
{
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { logSysErr("mmap(anonymous)"); return nullptr; }
    return p;
}

void freePrivate(void* p, size_t bytes)
{
    if (p && munmap(p, bytes) < 0) logSysErr("munmap");
}

// File-backed MAP_SHARED mapping: every process that maps the file sees the same bytes.
void* mapSharedFile(const std::string& path, size_t bytes, bool create)
{
    int fd = open(path.c_str(), create ? (O_RDWR | O_CREAT | O_TRUNC) : O_RDWR, 0600);
    if (fd < 0) { logSysErr("open(" + path + ")"); return nullptr; }
    if (create) {                       // give the file its size by writing zeros
        static const char zeros[4096] = {0};
        size_t left = bytes;
        while (left > 0) {
            size_t chunk = left < sizeof zeros ? left : sizeof zeros;
            ssize_t n = write(fd, zeros, chunk);
            if (n < 0) { if (errno == EINTR) continue; logSysErr("write(zero fill)"); close(fd); return nullptr; }
            left -= (size_t)n;
        }
    }
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { logSysErr("mmap(" + path + ")"); close(fd); return nullptr; }
    close(fd);                          // mapping stays valid after close
    return p;
}

void run(PerfTable& pt, int, bool verbose)
{
    Timer t;
    const size_t bytes = 1 << 20;       // 1 MiB
    // 1) anonymous mapping = memory allocation
    t.start(); void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); double msMap = t.stopMs();
    if (p == MAP_FAILED) { logSysErr("mmap(anonymous)"); return; }
    int* a = (int*)p; size_t n = bytes / sizeof(int);
    for (size_t i = 0; i < n; ++i) a[i] = (int)(i * 3);
    bool good = true;
    for (size_t i = 0; i < n; ++i) if (a[i] != (int)(i * 3)) { good = false; break; }
    t.start(); int rc = munmap(p, bytes); double msUnmap = t.stopMs();
    if (rc < 0) { logSysErr("munmap(anonymous)"); return; }
    if (!good) { logMsg(Lvl::ERR, "memory read-back mismatch"); return; }

    // 2) file mapping = file contents appear as memory
    std::string text;
    if (!fileio::readWholeFile(g_inputPath, text) || text.empty()) return;   // only used to learn the size
    int fd = open(g_inputPath.c_str(), O_RDONLY);
    if (fd < 0) { logSysErr("open"); return; }
    t.start(); void* f = mmap(nullptr, text.size(), PROT_READ, MAP_PRIVATE, fd, 0); double msFMap = t.stopMs();
    if (f == MAP_FAILED) { logSysErr("mmap(file)"); close(fd); return; }
    close(fd);
    bool same = std::memcmp(f, text.data(), text.size()) == 0;
    t.start(); rc = munmap(f, text.size()); double msFUnmap = t.stopMs();
    if (rc < 0) { logSysErr("munmap(file)"); return; }
    if (!same) { logMsg(Lvl::ERR, "mapped file differs from read() data"); return; }

    pt.add("Memory", "mmap (anonymous 1MiB)", msMap);   pt.add("Memory", "munmap (anonymous 1MiB)", msUnmap);
    pt.add("Memory", "mmap (file)", msFMap);            pt.add("Memory", "munmap (file)", msFUnmap);
    if (verbose) {
        logMsg(Lvl::OK, "mmap(anonymous) 1 MiB: wrote and verified " + std::to_string(n) + " ints, then munmap OK");
        logMsg(Lvl::OK, "mmap(file) of " + g_inputPath + ": bytes identical to read() data, munmap OK");
    }
}

} // namespace memmgmt
