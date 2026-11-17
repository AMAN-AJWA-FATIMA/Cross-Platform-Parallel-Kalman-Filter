// Permissions.cpp - Windows SetFileSecurity()
// Builds a security descriptor (DACL) from an SDDL string and applies it to a file with SetFileSecurity().
//   BU = built-in Users, SY = SYSTEM, BA = Administrators; FA = full access, FR = generic read.
#include "Modules.h"
#include <sddl.h>
#pragma comment(lib, "advapi32.lib")

namespace perms {

static const char* SDDL_USERS_READONLY = "D:P(A;;FR;;;BU)(A;;FA;;;SY)(A;;FA;;;BA)";
static const char* SDDL_USERS_FULL     = "D:P(A;;FA;;;BU)(A;;FA;;;SY)(A;;FA;;;BA)";

// Applies the SDDL as the file's DACL.  `msOut` (optional) receives the time of SetFileSecurity alone.
static bool applySddl(const std::string& path, const char* sddl, double* msOut)
{
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl, SDDL_REVISION_1, &sd, nullptr)) {
        logSysErr("ConvertStringSecurityDescriptorToSecurityDescriptor"); return false;
    }
    Timer t; t.start();
    BOOL ok = SetFileSecurityA(path.c_str(), DACL_SECURITY_INFORMATION, sd);
    double ms = t.stopMs();
    if (!ok) logSysErr("SetFileSecurity(" + path + ")");
    LocalFree(sd);
    if (ok && msOut) *msOut = ms;
    return ok != FALSE;
}

// Reads the DACL back (GetFileSecurity) and returns it as an SDDL string for the log.
static std::string readDacl(const std::string& path)
{
    DWORD need = 0;
    GetFileSecurityA(path.c_str(), DACL_SECURITY_INFORMATION, nullptr, 0, &need);
    if (need == 0) { logSysErr("GetFileSecurity(size)"); return "?"; }
    std::vector<char> buf(need);
    if (!GetFileSecurityA(path.c_str(), DACL_SECURITY_INFORMATION, (PSECURITY_DESCRIPTOR)buf.data(), need, &need)) { logSysErr("GetFileSecurity"); return "?"; }
    char* s = nullptr;
    if (!ConvertSecurityDescriptorToStringSecurityDescriptorA((PSECURITY_DESCRIPTOR)buf.data(), SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &s, nullptr)) { logSysErr("ConvertSecurityDescriptorToString"); return "?"; }
    std::string r = s; LocalFree(s); return r;
}

// Can we open the file for writing right now?  (proves the effect of the DACL)
static bool canWrite(const std::string& path)
{
    HANDLE h = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h); return true;
}

bool setUsersReadOnly(const std::string& path)    { return applySddl(path, SDDL_USERS_READONLY, nullptr); }
bool setUsersFullControl(const std::string& path) { return applySddl(path, SDDL_USERS_FULL, nullptr); }

void run(PerfTable& pt, int, bool verbose)
{
    std::string f = g_outDir + "perm_test.txt";
    if (!fileio::writeWholeFile(f, "permission test file\n")) return;
    if (!applySddl(f, SDDL_USERS_FULL, nullptr)) return;            // known starting state (not recorded)

    double msRestrict = 0, msRestore = 0;
    if (!applySddl(f, SDDL_USERS_READONLY, &msRestrict)) return;
    bool writableAfterRestrict = canWrite(f);
    std::string dacl1 = verbose ? readDacl(f) : "";
    if (!applySddl(f, SDDL_USERS_FULL, &msRestore)) return;
    bool writableAfterRestore = canWrite(f);
    std::string dacl2 = verbose ? readDacl(f) : "";

    if (writableAfterRestrict || !writableAfterRestore) {
        logMsg(Lvl::ERR, "unexpected access result: writable after read-only DACL = " + std::string(writableAfterRestrict ? "yes" : "no") +
                         ", writable after restore = " + (writableAfterRestore ? "yes" : "no"));
        return;
    }
    pt.add("Permissions", "SetFileSecurity (read-only DACL)", msRestrict);
    pt.add("Permissions", "SetFileSecurity (restore DACL)", msRestore);
    if (verbose) {
        logMsg(Lvl::OK, "SetFileSecurity: read-only DACL applied; opening for write was DENIED as expected");
        logMsg(Lvl::INFO, "DACL now: " + dacl1);
        logMsg(Lvl::OK, "SetFileSecurity: full-control DACL restored; opening for write now SUCCEEDS");
        logMsg(Lvl::INFO, "DACL now: " + dacl2);
        logMsg(Lvl::INFO, "Windows has no umask()/chown() equivalent in the CCA table; ownership is not changed here.");
    }
}

} // namespace perms
