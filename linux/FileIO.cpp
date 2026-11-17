// FileIO.cpp - Linux file I/O with open(), read(), write(), close()
#include "Modules.h"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

namespace fileio {

bool readWholeFile(const std::string& path, std::string& out)
{
    out.clear();
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) { logSysErr("open(" + path + ")"); return false; }
    char buf[1 << 16];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        if (n < 0) { if (errno == EINTR) continue; logSysErr("read(" + path + ")"); close(fd); return false; }
        if (n == 0) break;
        out.append(buf, (size_t)n);
    }
    if (close(fd) < 0) { logSysErr("close(" + path + ")"); return false; }
    return true;
}

bool writeWholeFile(const std::string& path, const std::string& data, unsigned mode)
{
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, (mode_t)mode);
    if (fd < 0) { logSysErr("open(" + path + ")"); return false; }
    size_t off = 0;
    while (off < data.size()) {
        ssize_t n = write(fd, data.data() + off, data.size() - off);
        if (n < 0) { if (errno == EINTR) continue; logSysErr("write(" + path + ")"); close(fd); return false; }
        off += (size_t)n;
    }
    if (close(fd) < 0) { logSysErr("close(" + path + ")"); return false; }
    return true;
}

// One benchmark run: each call is timed on its own.
void run(PerfTable& pt, int, bool verbose)
{
    Timer t; char buf[1 << 16];
    t.start(); int fd = open(g_inputPath.c_str(), O_RDONLY); double msOpen = t.stopMs();
    if (fd < 0) { logSysErr("open(" + g_inputPath + ")"); return; }
    t.start(); ssize_t n = read(fd, buf, sizeof buf); double msRead = t.stopMs();
    if (n < 0) { logSysErr("read"); close(fd); return; }
    t.start(); int rc = close(fd); double msClose = t.stopMs();
    if (rc < 0) { logSysErr("close"); return; }

    std::string outPath = g_outDir + "fileio_copy.txt";
    int wfd = open(outPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);   // not timed
    if (wfd < 0) { logSysErr("open(" + outPath + ")"); return; }
    t.start(); ssize_t w = write(wfd, buf, (size_t)n); double msWrite = t.stopMs();
    if (w != n) { if (w < 0) logSysErr("write"); else logMsg(Lvl::ERR, "short write"); close(wfd); return; }
    if (close(wfd) < 0) { logSysErr("close(output)"); return; }

    pt.add("File I/O", "open", msOpen);   pt.add("File I/O", "read", msRead);
    pt.add("File I/O", "write", msWrite); pt.add("File I/O", "close", msClose);
    if (verbose) {
        logMsg(Lvl::OK, "open(" + g_inputPath + ") returned fd, read " + std::to_string(n) + " bytes");
        logMsg(Lvl::OK, "write() wrote " + std::to_string(w) + " bytes to " + outPath);
        logMsg(Lvl::OK, "close() succeeded on both descriptors");
    }
}

} // namespace fileio
