// Permissions.cpp - Linux chmod(), chown(), umask()
#include "Modules.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>

namespace perms {

static bool modeOf(const std::string& path, unsigned& mode)   // stat() only VERIFIES the effect
{
    struct stat sb;
    if (stat(path.c_str(), &sb) < 0) { logSysErr("stat(" + path + ")"); return false; }
    mode = sb.st_mode & 0777; return true;
}
static std::string oct(unsigned m) { char b[16]; std::snprintf(b, sizeof b, "%04o", m); return b; }

bool applyOutputPermissions(const std::string& path)
{
    if (chmod(path.c_str(), 0640) < 0) { logSysErr("chmod(" + path + ")"); return false; }
    unsigned m; if (!modeOf(path, m)) return false;
    logMsg(m == 0640 ? Lvl::OK : Lvl::ERR, "chmod(" + path + ", 0640) -> mode now " + oct(m));
    return m == 0640;
}

void run(PerfTable& pt, int, bool verbose)
{
    Timer t; unsigned m;
    std::string f1 = g_outDir + "perm_umask.txt", f2 = g_outDir + "perm_test.txt";

    // umask: create with requested mode 0666, observe the mode actually granted
    t.start(); mode_t old = umask(077); double msUmask = t.stopMs();
    int fd = open(f1.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) { logSysErr("open(" + f1 + ")"); umask(old); return; }
    close(fd);
    if (!modeOf(f1, m)) { umask(old); return; }
    umask(old);                                    // restore the previous mask
    bool umaskOk = (m == 0600);
    if (verbose) logMsg(umaskOk ? Lvl::OK : Lvl::ERR, "umask(077): open(...,0666) produced mode " + oct(m) + " (expected 0600); old mask was " + oct(old));
    if (!umaskOk) return;

    // chmod
    fd = open(f2.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { logSysErr("open(" + f2 + ")"); return; }
    close(fd);
    t.start(); int rc = chmod(f2.c_str(), 0600); double msChmod = t.stopMs();
    if (rc < 0) { logSysErr("chmod"); return; }
    if (!modeOf(f2, m) || m != 0600) { logMsg(Lvl::ERR, "chmod did not produce 0600"); return; }
    if (verbose) logMsg(Lvl::OK, "chmod(" + f2 + ", 0600) -> mode now " + oct(m));

    // chown: to our own uid/gid (always allowed), then an attempt to become root-owned
    t.start(); rc = chown(f2.c_str(), getuid(), getgid()); double msChown = t.stopMs();
    if (rc < 0) { logSysErr("chown(self)"); return; }
    if (verbose) {
        logMsg(Lvl::OK, "chown(" + f2 + ", uid=" + std::to_string(getuid()) + ", gid=" + std::to_string(getgid()) + ") succeeded");
        if (chown(f2.c_str(), 0, 0) < 0) logMsg(Lvl::INFO, std::string("chown to root denied by the OS (normal without sudo): errno=") + std::to_string(errno));
        else logMsg(Lvl::OK, "chown to root succeeded (process has root privilege)");
    }
    pt.add("Permissions", "umask", msUmask);
    pt.add("Permissions", "chmod", msChmod);
    pt.add("Permissions", "chown (to self)", msChown);
}

} // namespace perms
