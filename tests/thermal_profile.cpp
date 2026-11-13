// SPDX-License-Identifier: GPL-2.0-only
#include "../kernel/thermal_profile.h"
#include <iostream>
int main(){
    unsigned cases=0;
    for(unsigned old=0;old<256;++old){
        const unsigned bits=old&0x90;
        const int expected=bits==0x80?0:bits==0?1:bits==0x10?2:-1;
        if(slayer_thermal_decode(old)!=expected)return 1;
        for(unsigned mode=0;mode<3;++mode){
            unsigned char next=0;
            if(slayer_thermal_encode(old,mode,&next))return 2;
            if(slayer_thermal_decode(next)!=static_cast<int>(mode))return 3;
            if((old&~0x90)!=(next&~0x90))return 4;
            // Fan Boost and every unrelated flag survive all transitions.
            if((old&0x40)!=(next&0x40))return 5;
            ++cases;
        }
    }
    unsigned char next=0;
    if(!slayer_thermal_encode(0,3,&next)||!slayer_thermal_encode(0,0,nullptr))return 6;
    if(slayer_thermal_decode(0x30)!=2||slayer_thermal_decode(0xa0)!=0||slayer_thermal_decode(0x20)!=1)return 7;
    std::cout<<cases<<" exact-model thermal transitions; Office aliases, unknown states and independent fan flags verified.\n";
}
