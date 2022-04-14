#pragma once
#include <VEngineConfig.h>
#include <numeric>
constexpr double Deg2Rad = 0.0174532924;
constexpr double Rad2Deg = 57.29578l;
constexpr float Float32MaxValue = std::numeric_limits<float>::max();
constexpr float Float32MinValue = std::numeric_limits<float>::min();
constexpr float Float32Episilon = std::numeric_limits<float>::epsilon();
constexpr double Float64MaxValue = std::numeric_limits<double>::max();
constexpr double Float64MinValue = std::numeric_limits<double>::min();
constexpr double Float64Episilon = std::numeric_limits<double>::epsilon();
constexpr double VENGINE_PI = 3.1415926536;