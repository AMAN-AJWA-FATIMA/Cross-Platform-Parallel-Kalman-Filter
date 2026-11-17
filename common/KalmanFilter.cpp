// KalmanFilter.cpp - identical algorithm on Linux and Windows (no OS calls here).
#include "KalmanFilter.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

void kalmanFilterChannel(const double* in, double* out, int rows, int cols,
                         int ch, const KalmanParams& kp)
{
    double x = in[ch];          // initial state = first measurement
    double p = kp.p0;           // initial variance
    out[ch] = x;
    for (int t = 1; t < rows; ++t) {
        double z = in[(size_t)t * cols + ch];
        // PREDICT (constant-state model: x_pred = x)
        double pPred = p + kp.q;                 // P- = P + Q
        // UPDATE
        double k = pPred / (pPred + kp.r);       // K = P- / (P- + R)
        x = x + k * (z - x);                     // x = x + K (z - x)
        p = (1.0 - k) * pPred;                   // P = (1 - K) P-
        out[(size_t)t * cols + ch] = x;
    }
}

bool parseCsv(const std::string& text, std::vector<double>& data,
              int& rows, int& cols, std::string& err)
{
    data.clear(); rows = 0; cols = 0;
    const char* p = text.c_str();
    const char* end = p + text.size();
    const char* nl = (const char*)std::memchr(p, '\n', end - p);
    if (!nl) { err = "CSV has no data rows"; return false; }
    cols = 1;
    for (const char* q = p; q < nl; ++q) if (*q == ',') ++cols;
    p = nl + 1;
    while (p < end) {
        if (*p == '\n' || *p == '\r') { ++p; continue; }
        for (int c = 0; c < cols; ++c) {
            char* e = nullptr;
            double v = std::strtod(p, &e);
            if (e == p) { err = "bad number in row " + std::to_string(rows + 1); return false; }
            data.push_back(v);
            p = e;
            if (c < cols - 1) {
                if (p < end && *p == ',') ++p;
                else { err = "too few columns in row " + std::to_string(rows + 1); return false; }
            }
        }
        while (p < end && *p != '\n') ++p;
        ++rows;
    }
    if (rows == 0) { err = "CSV has no data rows"; return false; }
    return true;
}

std::string formatCsv(const double* data, int rows, int cols, const char* prefix)
{
    std::string s;
    s.reserve((size_t)rows * cols * 10 + 64);
    char buf[48];
    for (int c = 0; c < cols; ++c) {
        std::snprintf(buf, sizeof buf, "%s%d%s", prefix, c + 1, c < cols - 1 ? "," : "\n");
        s += buf;
    }
    for (int t = 0; t < rows; ++t)
        for (int c = 0; c < cols; ++c) {
            std::snprintf(buf, sizeof buf, "%.4f%s", data[(size_t)t * cols + c], c < cols - 1 ? "," : "\n");
            s += buf;
        }
    return s;
}
