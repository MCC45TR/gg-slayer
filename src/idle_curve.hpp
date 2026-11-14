// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
namespace slayer {
inline constexpr int idleFadeMilliseconds=5000;
struct IdleSample {
    double scale=1000.;
    double velocity=0.; // Scale units per millisecond, before USB quantization.
    int rounded() const {return std::clamp(int(std::lround(scale)),1,1000);}
};

// One five-second trajectory. Retarget from the exact position and velocity,
// including between timer ticks. A brief bounded brake absorbs existing motion
// before the new ease; changing direction never jumps or suddenly stops it.
class IdleTransition {
    IdleSample start;
    int destination=1000;
    double brakeMilliseconds=0.,brakeEnd=1000.;
public:
    IdleTransition()=default;
    IdleTransition(IdleSample current,int target):start(current),destination(std::clamp(target,1,1000)),brakeEnd(current.scale){
        start.scale=std::clamp(start.scale,1.,1000.);
        brakeEnd=start.scale;
        if(std::abs(start.velocity)>1.e-12){
            const double room=start.velocity>0.?1000.-start.scale:start.scale-1.;
            // Integral of the braking velocity is v0 * duration / 2.
            // Shorten the brake near either endpoint to stay inside the range.
            brakeMilliseconds=std::min(120.,2.*room/std::abs(start.velocity));
            brakeEnd=std::clamp(start.scale+start.velocity*brakeMilliseconds*.5,1.,1000.);
        }
    }
    int target() const {return destination;}
    IdleSample sample(long long milliseconds) const {
        const double elapsed=std::clamp(double(milliseconds),0.,double(idleFadeMilliseconds));
        if(elapsed>=idleFadeMilliseconds)return {double(destination),0.};
        if(brakeMilliseconds>0.&&elapsed<brakeMilliseconds){
            const double t=elapsed/brakeMilliseconds;
            const double t2=t*t,t3=t2*t,t4=t3*t;
            return {std::clamp(start.scale+start.velocity*brakeMilliseconds*(t-t3+.5*t4),1.,1000.),
                start.velocity*(1.-3.*t2+2.*t3)};
        }
        const double duration=idleFadeMilliseconds-brakeMilliseconds;
        const double t=(elapsed-brakeMilliseconds)/duration;
        const double t2=t*t,t3=t2*t;
        const double ease=t3*(10.+t*(-15.+6.*t));
        const double slope=30.*t2*(t-1.)*(t-1.);
        return {std::clamp(brakeEnd+(destination-brakeEnd)*ease,1.,1000.),
            (destination-brakeEnd)*slope/duration};
    }
};
inline int idleScaleAt(int from,int to,long long elapsed){
    return IdleTransition({double(from),0.},to).sample(elapsed).rounded();
}
}
