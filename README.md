# Cross-Platform Operating System System Calls & Kalman Filter (C++)

A cross-platform C++ systems programming project implementing and benchmarking **Linux POSIX system calls and Windows Win32 APIs**, integrated into a multi-process Kalman filter for sensor data processing.

The project investigates differences in file I/O, process management, memory management, inter-process communication (IPC), and file permissions across both operating systems.

## 1. Project Overview

The project implements five fundamental operating system categories using native platform-specific interfaces.

| Category | Linux (POSIX) | Windows (Win32 API) |
|---|---|---|
| File I/O | `open`, `read`, `write`, `close` | `CreateFile`, `ReadFile`, `WriteFile`, `CloseHandle` |
| Process Management | `fork`, `exec`, `wait` | `CreateProcess`, `WaitForSingleObject` |
| Memory Management | `mmap`, `munmap` | `VirtualAlloc`, `VirtualFree` |
| IPC | `pipe`, `shmget`, `shmat` | `CreatePipe`, `CreateFileMapping`, `MapViewOfFile` |
| Permissions | `chmod`, `chown`, `umask` | `SetFileSecurity` |

**Implementation scope:** 15 Linux system calls and 12 Windows APIs. Windows does not provide separate direct counterparts for Linux `exec`, `chown`, and `umask`.

## 2. Integrated Multi-Process Kalman Filter

A common Kalman filter implementation is shared between both operating systems, while process management, memory operations, file handling, and IPC remain platform-specific.

### Execution Pipeline

1. Read sensor measurements from CSV input using native file I/O.
2. Store measurements in shared memory.
3. Create worker processes using `fork`/`exec` on Linux or `CreateProcess` on Windows.
4. Distribute independent sensor filtering tasks among worker processes.
5. Exchange worker status through pipes and computation timings through shared memory.
6. Synchronize worker completion and collect results.
7. Write filtered measurements to CSV output.
8. Compare sequential and parallel results for correctness.

### Kalman Filter Configuration

| Parameter | Value |
|---|---:|
| Initial estimate ($x$) | First measurement |
| Initial uncertainty ($P$) | 1.0 |
| Process noise ($Q$) | 0.01 |
| Measurement noise ($R$) | 1.0 |

The filter applies the prediction and measurement update equations.

**Prediction:**

$$
P^- = P + Q
$$

**Kalman Gain:**

$$
K = \frac{P^-}{P^- + R}
$$

**State Update:**

$$
x = x + K(z-x)
$$

**Uncertainty Update:**

$$
P = (1-K)P^-
$$

Where:
- $x$ = estimated state
- $P$ = estimation uncertainty
- $P^-$ = predicted uncertainty
- $K$ = Kalman gain
- $z$ = sensor measurement
- $Q$ = process noise
- $R$ = measurement noise

### Correctness Validation

- Sequential and parallel outputs matched across all 12 reported runs on both operating systems.
- Maximum absolute difference: **0**.
- Linux and Windows output files (64 MB) produced identical MD5 hashes: `3fdaad49fbc4c80ade7ef9618abb282f`.
- Filtering reduced measurement noise by approximately 78–87%.

## 3. Performance Benchmarking

Each system call/API operation was executed five times, with individual execution times measured in milliseconds.

The benchmarking framework records:

- Individual execution times and averages.
- Process-level CPU time.
- Peak memory usage.
- Sequential versus parallel execution time.
- CSV performance records and execution logs.

### Experimental Environment

| Specification | Linux | Windows |
|---|---|---|
| Operating System | Ubuntu 24.04.5 LTS (64-bit) | Windows 10 |
| Processor | AMD Ryzen 5 7430U | AMD Ryzen 5 7430U |
| CPU | 6 cores, 12 threads | 6 cores, 12 threads |
| RAM | 8 GB | 8 GB |

### Linux vs Windows: Key Findings

Results from 14 paired operation comparisons:

| Observation | Result |
|---|---|
| Linux faster | 9 of 14 comparisons |
| Windows faster | 5 of 14 comparisons |
| File I/O | Linux faster in all four comparisons |
| Process management | Linux faster in all three comparisons |
| Memory management | Windows faster in both comparisons |
| Shared-memory operations | Windows faster |
| Permission modification | Linux faster |

The measurements demonstrate that operating system performance varies by operation rather than one platform consistently outperforming the other.

### Sequential vs Parallel Kalman Filter

Experiments were conducted using a small dataset (20 rows) and a large dataset (1,000,000 rows).

**Speedup ratio:**

$$
\text{Speedup} = \frac{T_{\text{sequential}}}{T_{\text{parallel}}}
$$

Where:
- $T_{\text{sequential}}$ = sequential execution time.
- $T_{\text{parallel}}$ = parallel execution time.

A ratio greater than 1 indicates faster parallel execution, while a ratio below 1 indicates faster sequential execution.

| OS | Dataset | Sequential Avg (ms) | Parallel Avg (ms) | Ratio |
|---|---|---:|---:|---:|
| Linux | Small | 0.0180 | 5.404 | 0.0033 |
| Linux | Large | 191.80 | 156.14 | **1.23×** |
| Windows | Small | 0.0078 | 61
