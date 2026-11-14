// SPDX-License-Identifier: MIT
#include "../src/idle_curve.hpp"
#include "../kernel/ite8291/brightness_contract.h"
#include <iostream>
static bool close(double a,double b){return std::abs(a-b)<1.e-8;}
int main(){
    for(unsigned logical=0;logical<=50;++logical){
        const unsigned full=logical?std::max(logical,2u):0u;
        unsigned previous=full,writes=0;
        for(int ms=0;ms<=5000;ms+=50){
            const auto scale=slayer::idleScaleAt(1000,1,ms);
            const auto actual=slayer_effective_brightness(logical,unsigned(scale));
            if(actual>full||actual>previous||(!logical&&actual)||(logical&&actual<2))return 1;
            if(actual!=previous)++writes;
            previous=actual;
        }
        if(previous!=(logical?2u:0u)||writes>48)return 2;
        for(int ms=0;ms<=5000;ms+=50){
            const auto actual=slayer_effective_brightness(logical,unsigned(slayer::idleScaleAt(1,1000,ms)));
            if(actual<previous||actual>full)return 3;
            previous=actual;
        }
        if(previous!=full)return 4;
    }
    const int interrupted=slayer::idleScaleAt(1000,1,1750);
    if(slayer::idleScaleAt(interrupted,1000,0)!=interrupted||slayer::idleScaleAt(interrupted,1000,5000)!=1000)return 5;
    if(slayer::idleScaleAt(1000,1,-1)!=1000||slayer::idleScaleAt(1000,1,99999)!=1)return 6;
    for(const int initial:{1,1000}){
        const int destination=initial==1?1000:1;
        const slayer::IdleTransition original({double(initial),0.},destination);
        // Includes the reported press three seconds into dimming, plus both
        // directions and presses just before either endpoint is reached.
        for(int cut=0;cut<5000;cut+=25){
            const auto before=original.sample(cut);
            const slayer::IdleTransition reversed(before,initial);
            const auto after=reversed.sample(0);
            if(!close(before.scale,after.scale)||!close(before.velocity,after.velocity))return 7;
            auto previous=after;
            for(int ms=1;ms<=5000;++ms){
                const auto next=reversed.sample(ms);
                if(!std::isfinite(next.scale)||!std::isfinite(next.velocity)||next.scale<1.||next.scale>1000.)return 8;
                // The existing motion is braked within 120 ms; after that the
                // path must move monotonically toward the requested endpoint.
                if(ms>120&&((initial==1000&&next.scale<previous.scale-1.e-8)||(initial==1&&next.scale>previous.scale+1.e-8)))return 9;
                if(std::abs(next.scale-previous.scale)>.5)return 10;
                previous=next;
            }
            if(!close(previous.scale,double(initial))||previous.velocity!=0.)return 11;
            for(unsigned logical=0;logical<=50;++logical){
                unsigned physical=slayer_effective_brightness(logical,unsigned(after.rounded())),writes=0;
                for(int ms=50;ms<=5000;ms+=50){
                    const auto next=slayer_effective_brightness(logical,unsigned(reversed.sample(ms).rounded()));
                    if((!logical&&next)||(logical&&next<2)||next>(logical?std::max(logical,2u):0u))return 12;
                    if(next!=physical)++writes;
                    if(std::abs(int(next)-int(physical))>1)return 13;
                    physical=next;
                }
                if(writes>96)return 14; // At most one brake and one return.
            }
        }
    }
    slayer::IdleTransition rapid({1000.,0.},1);
    for(int i=0;i<1000;++i){
        const auto before=rapid.sample(17);
        rapid=slayer::IdleTransition(before,i%2?1:1000);
        const auto after=rapid.sample(0);
        if(!close(before.scale,after.scale)||!close(before.velocity,after.velocity))return 15;
    }
    if(rapid.sample(5000).rounded()!=rapid.target())return 16;
    std::cout<<"All 51 logical levels: explicit zero, visible 4% floor, five-second endpoints, 400 interruption points with continuous position/velocity, bounded USB steps and rapid retargeting.\n";
}
