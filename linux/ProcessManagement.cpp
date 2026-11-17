// ProcessManagement.cpp - Linux fork(), execv() (exec family), wait()/waitpid()
// plus the integrated multi-process Kalman filter (mmap + pipe + shmget/shmat + file I/O).
#include "Modules.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace procmgmt {

// ---------- benchmark / demo of fork + exec + wait ----------
void run(PerfTable& pt, int, bool verbose)
{
    Timer total, t;
    std::fflush(stdout);
    total.start();
    t.start(); pid_t pid = fork(); double msFork = t.stopMs();   // child measures nothing; parent times the call
    if (pid < 0) { logSysErr("fork"); return; }
    if (pid == 0) {
        char* args[] = { (char*)"echo", (char*)"hello from child program (execv)", nullptr };
        execv("/bin/echo", args);
        std::perror("execv failed");                              // only reached on failure
        _exit(127);
    }
    int st = 0;
    t.start(); pid_t w = wait(&st); double msWait = t.stopMs();
    double msTotal = total.stopMs();
    if (w < 0) { logSysErr("wait"); return; }
    if (WIFEXITED(st) && WEXITSTATUS(st) == 127) { logMsg(Lvl::ERR, "child could not exec /bin/echo"); return; }
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { logMsg(Lvl::ERR, "child ended abnormally"); return; }
    pt.add("Process", "fork", msFork);
    pt.add("Process", "wait (blocked until child exit)", msWait);
    pt.add("Process", "fork+execv+wait (total)", msTotal);
    if (verbose) logMsg(Lvl::OK, "parent pid " + std::to_string(getpid()) + " created child pid " + std::to_string(pid) +
                        "; child exec'd /bin/echo; wait() returned exit status 0");
}

// ---------- worker program (re-launched by execv with --worker) ----------
// argv: prog --worker <arrayFile> <shmId> <rows> <cols> <firstCh> <lastChExclusive> <index> <pipeWriteFd>
int workerMain(int argc, char** argv)
{
    if (argc < 10) { std::fprintf(stderr, "worker: bad arguments\n"); return 2; }
    std::string path = argv[2];
    int shmId = std::atoi(argv[3]), rows = std::atoi(argv[4]), cols = std::atoi(argv[5]);
    int first = std::atoi(argv[6]), last = std::atoi(argv[7]), idx = std::atoi(argv[8]), pfd = std::atoi(argv[9]);
    size_t n = (size_t)rows * cols;

    double* arr = (double*)memmgmt::mapSharedFile(path, 2 * n * sizeof(double), false);   // mmap
    if (!arr) return 3;
    double* slots = (double*)ipcmgmt::shmAttach(shmId);                                   // shmat
    if (!slots) { memmgmt::freePrivate(arr, 2 * n * sizeof(double)); return 4; }

    KalmanParams kp; Timer t; t.start();
    for (int ch = first; ch < last; ++ch) kalmanFilterChannel(arr, arr + n, rows, cols, ch, kp);
    double ms = t.stopMs();
    slots[idx] = ms;

    char msg[160];
    int len = std::snprintf(msg, sizeof msg, "worker %d (pid %d): channels %d..%d filtered in %.4f ms\n", idx, (int)getpid(), first + 1, last, ms);
    int rc = (write(pfd, msg, (size_t)len) == len) ? 0 : 5;                               // pipe -> parent
    close(pfd);
    ipcmgmt::shmDetach(slots);
    memmgmt::freePrivate(arr, 2 * n * sizeof(double));                                    // munmap
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
    logMsg(Lvl::OK, "read " + std::to_string(text.size()) + " bytes via open/read/close; " + std::to_string(rows) + " rows x " + std::to_string(cols) + " sensor channels");

    size_t n = (size_t)rows * cols, bytes = 2 * n * sizeof(double);
    std::string arrPath = g_outDir + "kalman_arrays.bin";
    t.start();
    double* arr = (double*)memmgmt::mapSharedFile(arrPath, bytes, true);
    if (!arr) return 1;
    std::memcpy(arr, data.data(), n * sizeof(double));
    double msMap = t.stopMs();
    logMsg(Lvl::OK, "mmap(MAP_SHARED) of " + arrPath + ": [input array | output array], " + std::to_string(bytes) + " bytes");

    int W = std::max(1, std::min(workers, cols));
    int shmId = -1;
    double* slots = (double*)ipcmgmt::shmCreate((size_t)W * sizeof(double), &shmId);
    if (!slots) { memmgmt::freePrivate(arr, bytes); return 1; }
    int pfd[2];
    if (pipe(pfd) < 0) { logSysErr("pipe"); ipcmgmt::shmDestroy(slots, shmId); memmgmt::freePrivate(arr, bytes); return 1; }

    std::vector<pid_t> pids;
    t.start();
    for (int i = 0; i < W; ++i) {
        int first = i * cols / W, last = (i + 1) * cols / W;
        std::vector<std::string> a = { "kalman_linux", "--worker", arrPath, std::to_string(shmId), std::to_string(rows),
            std::to_string(cols), std::to_string(first), std::to_string(last), std::to_string(i), std::to_string(pfd[1]) };
        std::fflush(stdout);
        pid_t pid = fork();
        if (pid < 0) { logSysErr("fork"); break; }
        if (pid == 0) {
            close(pfd[0]);
            std::vector<char*> av; for (auto& s : a) av.push_back((char*)s.c_str()); av.push_back(nullptr);
            execv("/proc/self/exe", av.data());
            std::perror("execv(/proc/self/exe)");
            _exit(127);
        }
        pids.push_back(pid);
    }
    close(pfd[1]);
    std::string msgs; char buf[256]; ssize_t r;
    while ((r = read(pfd[0], buf, sizeof buf)) != 0) {
        if (r < 0) { if (errno == EINTR) continue; logSysErr("read(pipe)"); break; }
        msgs.append(buf, (size_t)r);
    }
    close(pfd[0]);
    bool ok = ((int)pids.size() == W);
    for (pid_t pid : pids) {
        int st = 0;
        if (waitpid(pid, &st, 0) < 0) { logSysErr("waitpid"); ok = false; }
        else if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { logMsg(Lvl::ERR, "worker pid " + std::to_string(pid) + " failed, status " + std::to_string(st)); ok = false; }
    }
    double msPar = t.stopMs();
    std::fputs(msgs.c_str(), stdout);
    int rc = 1;
    if (ok) {
        logMsg(Lvl::OK, std::to_string(W) + " worker process(es) finished (fork/execv/waitpid); results received via pipe + shared memory");
        // sequential reference on the same data, in this single process
        double* ref = (double*)memmgmt::allocPrivate(n * sizeof(double));
        if (ref) {
            KalmanParams kp; t.start();
            for (int ch = 0; ch < cols; ++ch) kalmanFilterChannel(arr, ref, rows, cols, ch, kp);
            double msSeq = t.stopMs();
            double maxDiff = 0; for (size_t i = 0; i < n; ++i) maxDiff = std::max(maxDiff, std::fabs(ref[i] - arr[n + i]));
            logMsg(maxDiff == 0.0 ? Lvl::OK : Lvl::ERR, "parallel output vs sequential output: max |difference| = " + std::to_string(maxDiff));
            memmgmt::freePrivate(ref, n * sizeof(double));

            std::string outPath = g_outDir + "kalman_output.csv";
            t.start();
            bool wrote = fileio::writeWholeFile(outPath, formatCsv(arr + n, rows, cols, "f"), 0644);
            double msWrite = t.stopMs();
            if (wrote) {
                logMsg(Lvl::OK, "filtered output written with open/write/close: " + outPath);
                perms::applyOutputPermissions(outPath);
                double wmax = 0; for (int i = 0; i < W; ++i) wmax = std::max(wmax, slots[i]);
                char line[600];
                std::snprintf(line, sizeof line,
                    "\nTIMING SUMMARY (ms)  rows=%d cols=%d workers=%d\n"
                    "  read file (open/read/close) : %.4f\n  parse CSV                   : %.4f\n"
                    "  mmap + copy input           : %.4f\n"
                    "  parallel phase (fork..wait) : %.4f   (slowest worker compute: %.4f)\n"
                    "  sequential compute (1 proc) : %.4f\n  write output file           : %.4f\n"
                    "  sequential/parallel ratio   : %.3f   (>1 means parallel was faster; <1 means slower)\n  total wall time             : %.4f\n",
                    rows, cols, W, msRead, msParse, msMap, msPar, wmax, msSeq, msWrite, msSeq / msPar, wall.stopMs());
                std::fputs(line, stdout); std::fflush(stdout);
                rc = (maxDiff == 0.0) ? 0 : 1;
            }
        }
    }
    memmgmt::freePrivate(arr, bytes);                       // munmap
    ipcmgmt::shmDestroy(slots, shmId);                      // shmdt + remove segment
    std::remove(arrPath.c_str());
    saveLog(g_outDir + "kalman_run.log");
    return rc;
}

} // namespace procmgmt
