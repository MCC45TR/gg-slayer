// SPDX-License-Identifier: MIT
#include "../src/idle_curve.hpp"
#include "../kernel/ite8291/brightness_contract.h"
#include <iostream>
int main(){
    for(unsigned logical=0;logical<=50;++logical){
        unsigned previous=logical,writes=0;
        for(int ms=0;ms<=5000;ms+=50){
            const auto scale=slayer::idleScaleAt(1000,1,ms);
            const auto actual=slayer_effective_brightness(logical,unsigned(scale));
            if(actual>logical||actual>previous||(!logical&&actual)||(logical&&!actual))return 1;
            if(actual!=previous)++writes;
            previous=actual;
        }
        if(previous!=(logical?1u:0u)||writes>49)return 2;
        for(int ms=0;ms<=5000;ms+=50){
            const auto actual=slayer_effective_brightness(logical,unsigned(slayer::idleScaleAt(1,1000,ms)));
            if(actual<previous||actual>logical)return 3;
            previous=actual;
        }
        if(previous!=logical)return 4;
    }
    const int interrupted=slayer::idleScaleAt(1000,1,1750);
    if(slayer::idleScaleAt(interrupted,1000,0)!=interrupted||slayer::idleScaleAt(interrupted,1000,5000)!=1000)return 5;
    if(slayer::idleScaleAt(1000,1,-1)!=1000||slayer::idleScaleAt(1000,1,99999)!=1)return 6;
    std::cout<<"All 51 logical levels: zero preserved, nonzero minimum, monotonic five-second fades, bounded USB writes, continuous reversal.\n";
}
