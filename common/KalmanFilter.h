// KalmanFilter.h - platform-independent Kalman filter + CSV helpers (shared by Linux and Windows)
#pragma once
#include <string>
#include <vector>

struct KalmanParams {
    double q  = 0.01;  // process noise variance Q
    double r  = 1.00;  // measurement noise variance R
    double p0 = 1.00;  // initial estimate variance P0
};

// Filters ONE sensor channel `ch` of a rows x cols row-major array.
// in[t*cols+ch] = measurement z_t, out[t*cols+ch] = filtered estimate x_t.
void kalmanFilterChannel(const double* in, double* out, int rows, int cols,
                         int ch, const KalmanParams& kp);

// Parses "header line + numeric rows" CSV text into a row-major array.
bool parseCsv(const std::string& text, std::vector<double>& data,
              int& rows, int& cols, std::string& err);

// Formats a rows x cols array as CSV text (header: prefix1,prefix2,...).
std::string formatCsv(const double* data, int rows, int cols, const char* prefix);
