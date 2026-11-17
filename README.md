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
| Windows | Small | 0.0078 | 61.88 | 0.00013 |
| Windows | Large | 163.62 | 220.81 | 0.74× |

**Key finding:** Four-process execution improved large-dataset performance on Linux by approximately 23%, while Windows parallel execution was approximately 26% slower than its sequential baseline.

For small datasets, process creation and synchronization overhead outweighed the computational benefits of parallelism.

## 4. Project Structure

```text
Cross-Platform-Parallel-Kalman-Filter/
│
├── common/
│   ├── KalmanFilter.h
│   └── KalmanFilter.cpp
│
├── linux/
│   ├── main.cpp
│   ├── Modules.h
│   ├── FileIO.cpp
│   ├── ProcessManagement.cpp
│   ├── MemoryManagement.cpp
│   ├── IPC.cpp
│   ├── Permissions.cpp
│   └── Performance.cpp
│
├── windows/
│   ├── main.cpp
│   ├── Modules.h
│   ├── FileIO.cpp
│   ├── ProcessManagement.cpp
│   ├── MemoryManagement.cpp
│   ├── IPC.cpp
│   ├── Permissions.cpp
│   └── Performance.cpp
│
├── input/
│   └── sensor_data.csv
│
├── output/
│   ├── linux/
│   └── windows/
│
└── report/
    ├── performance_template.csv
    ├── final_report.md
    └── presentation_and_viva.md
```

## 5. Build Instructions

**Requirements**
- C++17 compatible compiler
- GCC/G++ on Linux
- MinGW-w64 or Microsoft Visual C++ on Windows
- Windows libraries: `advapi32`, `psapi`

Run commands from the project root.

### Linux

```bash
g++ -std=c++17 -O2 -Wall -Wextra \
-o kalman_linux common/KalmanFilter.cpp linux/*.cpp
```

### Windows (MinGW-w64)

```bash
g++ -std=c++17 -O2 -o kalman_windows.exe \
common\KalmanFilter.cpp windows\main.cpp \
windows\FileIO.cpp windows\ProcessManagement.cpp \
windows\MemoryManagement.cpp windows\IPC.cpp \
windows\Permissions.cpp windows\Performance.cpp \
-ladvapi32 -lpsapi
```

## 6. Execution

### Linux

```bash
# Run individual benchmarks
./kalman_linux bench fileio
./kalman_linux bench process
./kalman_linux bench memory
./kalman_linux bench ipc
./kalman_linux bench permissions

# Run all benchmarks
./kalman_linux bench all

# Sequential and parallel execution
./kalman_linux kalman 1
./kalman_linux kalman 4

# Generate a large dataset
./kalman_linux gen 1000000 8 input/big.csv

# Execute parallel filtering
./kalman_linux kalman 4 input/big.csv
```

### Windows

```bash
# Run individual benchmarks
kalman_windows bench fileio
kalman_windows bench process
kalman_windows bench memory
kalman_windows bench ipc
kalman_windows bench permissions

# Run all benchmarks
kalman_windows bench all

# Sequential and parallel execution
kalman_windows kalman 1
kalman_windows kalman 4

# Generate a large dataset
kalman_windows gen 1000000 8 input\big.csv

# Execute parallel filtering
kalman_windows kalman 4 input\big.csv
```

## 7. Technologies and Concepts

- **Language:** C++17
- **Operating Systems:** Linux and Windows
- **System Interfaces:** POSIX System Calls and Windows Win32 API
- **Process Management:** Forking, process creation, execution, synchronization
- **Memory Management:** Virtual memory and memory mapping
- **IPC:** Pipes and shared memory
- **File Systems:** File handling and access permissions
- **Algorithm:** Kalman Filtering
- **Benchmarking:** Execution time, CPU time, and memory measurements
- **Data Handling:** CSV input/output and performance logging

## 8. References

1. **Linux Man Pages Project.** Linux system call and library interface documentation.  
   https://man7.org/linux/man-pages/

2. **Microsoft Learn.** Windows API and Win32 application development documentation.  
   https://learn.microsoft.com/en-us/windows/win32/

3. **Microsoft Learn.** Creating and Opening Files.  
   https://learn.microsoft.com/en-us/windows/desktop/FileIO/creating-and-opening-files

4. **Microsoft Learn.** Creating Processes.  
   https://learn.microsoft.com/en-us/windows/win32/procthread/creating-processes

5. **Kalman, R. E. (1960).** A New Approach to Linear Filtering and Prediction Problems. *Journal of Basic Engineering*, 82(1), 35–45.  
   https://doi.org/10.1115/1.3662552

6. **Silberschatz, A., Galvin, P. B., & Gagne, G.** *Operating System Concepts*. Wiley.

---
