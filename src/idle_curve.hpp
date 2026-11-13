// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
namespace slayer {
inline constexpr int idleFadeMilliseconds=5000;
inline int idleScaleAt(int from,int to,long long elapsed){
    const double t=std::clamp(double(elapsed)/idleFadeMilliseconds,0.,1.);
    return std::clamp(int(std::lround(from+(to-from)*t*t*(3.-2.*t))),1,1000);
}
}
