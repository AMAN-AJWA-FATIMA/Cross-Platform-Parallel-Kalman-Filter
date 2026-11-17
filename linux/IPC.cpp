// IPC.cpp - Linux pipe(), shmget(), shmat()
#include "Modules.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdio>

namespace ipcmgmt {

void* shmCreate(size_t bytes, int* idOut)
{
    int id = shmget(IPC_PRIVATE, bytes, IPC_CREAT | 0600);
    if (id < 0) { logSysErr("shmget"); return nullptr; }
    void* p = shmat(id, nullptr, 0);
    if (p == (void*)-1) { logSysErr("shmat"); shmctl(id, IPC_RMID, nullptr); return nullptr; }
    *idOut = id;
    return p;
}
void* shmAttach(int id)
{
    void* p = shmat(id, nullptr, 0);
    if (p == (void*)-1) { logSysErr("shmat"); return nullptr; }
    return p;
}
void shmDetach(void* addr) { if (addr && shmdt(addr) < 0) logSysErr("shmdt"); }
void shmDestroy(void* addr, int id)
{
    shmDetach(addr);
    if (shmctl(id, IPC_RMID, nullptr) < 0) logSysErr("shmctl(IPC_RMID)");
}

void run(PerfTable& pt, int, bool verbose)
{
    Timer t;
    // ---------- pipe: parent -> child ----------
    const char msg[] = "sensor-batch-ready";
    int fds[2];
    t.start(); int rc = pipe(fds); double msPipe = t.stopMs();
    if (rc < 0) { logSysErr("pipe"); return; }
    std::fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) { logSysErr("fork"); close(fds[0]); close(fds[1]); return; }
    if (pid == 0) {
        close(fds[1]);
        char buf[64] = {0};
        ssize_t n = read(fds[0], buf, sizeof buf - 1);
        if (verbose && n > 0) { std::printf("       child received via pipe: \"%s\"\n", buf); std::fflush(stdout); }
        close(fds[0]);
        _exit(n == (ssize_t)sizeof msg && std::strcmp(buf, msg) == 0 ? 0 : 1);
    }
    close(fds[0]);
    t.start(); ssize_t w = write(fds[1], msg, sizeof msg); double msPipeWrite = t.stopMs();
    if (w < 0) logSysErr("write(pipe)");
    close(fds[1]);
    int st = 0;
    if (wait(&st) < 0) { logSysErr("wait"); return; }
    bool pipeOk = w == (ssize_t)sizeof msg && WIFEXITED(st) && WEXITSTATUS(st) == 0;
    if (!pipeOk) { logMsg(Lvl::ERR, "pipe communication failed"); return; }

    // ---------- shared memory: parent writes, child reads ----------
    t.start(); int id = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600); double msGet = t.stopMs();
    if (id < 0) { logSysErr("shmget"); return; }
    t.start(); void* mem = shmat(id, nullptr, 0); double msAt = t.stopMs();
    if (mem == (void*)-1) { logSysErr("shmat"); shmctl(id, IPC_RMID, nullptr); return; }
    double* d = (double*)mem;
    for (int i = 0; i < 5; ++i) d[i] = 10.5 * (i + 1);
    std::fflush(stdout);
    pid = fork();
    if (pid < 0) { logSysErr("fork"); shmDestroy(mem, id); return; }
    if (pid == 0) {                       // the attached segment is inherited by the child
        bool good = true;
        for (int i = 0; i < 5; ++i) if (d[i] != 10.5 * (i + 1)) good = false;
        if (verbose) { std::printf("       child read from shared memory: %.1f %.1f %.1f %.1f %.1f\n", d[0], d[1], d[2], d[3], d[4]); std::fflush(stdout); }
        _exit(good ? 0 : 1);
    }
    if (wait(&st) < 0) logSysErr("wait");
    bool shmOk = WIFEXITED(st) && WEXITSTATUS(st) == 0;
    shmDestroy(mem, id);
    if (!shmOk) { logMsg(Lvl::ERR, "shared-memory communication failed"); return; }

    pt.add("IPC", "pipe", msPipe);  pt.add("IPC", "write (to pipe)", msPipeWrite);
    pt.add("IPC", "shmget", msGet); pt.add("IPC", "shmat", msAt);
    if (verbose) {
        logMsg(Lvl::OK, "pipe(): child process read the message the parent wrote");
        logMsg(Lvl::OK, "shmget()/shmat(): child process read the values the parent stored; segment removed");
    }
}

} // namespace ipcmgmt
