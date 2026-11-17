// FileIO.cpp - Windows file I/O with CreateFile(), ReadFile(), WriteFile(), CloseHandle()
#include "Modules.h"
#include <algorithm>

namespace fileio {

bool readWholeFile(const std::string& path, std::string& out)
{
    out.clear();
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { logSysErr("CreateFile(" + path + ")"); return false; }
    static char buf[1 << 16];
    for (;;) {
        DWORD n = 0;
        if (!ReadFile(h, buf, sizeof buf, &n, nullptr)) { logSysErr("ReadFile(" + path + ")"); CloseHandle(h); return false; }
        if (n == 0) break;
        out.append(buf, n);
    }
    if (!CloseHandle(h)) { logSysErr("CloseHandle(" + path + ")"); return false; }
    return true;
}

bool writeWholeFile(const std::string& path, const std::string& data)
{
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { logSysErr("CreateFile(" + path + ")"); return false; }
    size_t off = 0;
    while (off < data.size()) {
        DWORD want = (DWORD)std::min<size_t>(data.size() - off, 1u << 24), got = 0;
        if (!WriteFile(h, data.data() + off, want, &got, nullptr)) { logSysErr("WriteFile(" + path + ")"); CloseHandle(h); return false; }
        off += got;
    }
    if (!CloseHandle(h)) { logSysErr("CloseHandle(" + path + ")"); return false; }
    return true;
}

void run(PerfTable& pt, int, bool verbose)
{
    Timer t; static char buf[1 << 16]; DWORD n = 0, w = 0;
    t.start();
    HANDLE h = CreateFileA(g_inputPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    double msOpen = t.stopMs();
    if (h == INVALID_HANDLE_VALUE) { logSysErr("CreateFile(" + g_inputPath + ")"); return; }
    t.start(); BOOL ok = ReadFile(h, buf, sizeof buf, &n, nullptr); double msRead = t.stopMs();
    if (!ok) { logSysErr("ReadFile"); CloseHandle(h); return; }
    t.start(); ok = CloseHandle(h); double msClose = t.stopMs();
    if (!ok) { logSysErr("CloseHandle"); return; }

    std::string outPath = g_outDir + "fileio_copy.txt";
    HANDLE o = CreateFileA(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);   // not timed
    if (o == INVALID_HANDLE_VALUE) { logSysErr("CreateFile(" + outPath + ")"); return; }
    t.start(); ok = WriteFile(o, buf, n, &w, nullptr); double msWrite = t.stopMs();
    if (!ok || w != n) { logSysErr("WriteFile"); CloseHandle(o); return; }
    if (!CloseHandle(o)) { logSysErr("CloseHandle(output)"); return; }

    pt.add("File I/O", "CreateFile (open)", msOpen); pt.add("File I/O", "ReadFile", msRead);
    pt.add("File I/O", "WriteFile", msWrite);        pt.add("File I/O", "CloseHandle", msClose);
    if (verbose) {
        logMsg(Lvl::OK, "CreateFile(" + g_inputPath + ") ok, ReadFile read " + std::to_string(n) + " bytes");
        logMsg(Lvl::OK, "WriteFile wrote " + std::to_string(w) + " bytes to " + outPath);
        logMsg(Lvl::OK, "CloseHandle succeeded on both handles");
    }
}

} // namespace fileio
