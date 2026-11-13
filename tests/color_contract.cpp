// SPDX-License-Identifier: MIT
#include "../kernel/ite8291/color_contract.h"
int main(){
    for(unsigned int gain=1;gain<256;++gain){
        unsigned char previous=0;
        for(unsigned int channel=0;channel<256;++channel){
            const auto current=slayer_color_channel(channel,gain);
            if(current<previous||current>gain)return 1;
            if(gain==255&&current!=channel)return 2;
            previous=current;
        }
        if(slayer_color_channel(255,gain)!=gain)return 3;
    }
    // User measured neutral white at R160/G255/B255. Preserve black,
    // attenuate red only, and retain the intended green/blue proportions.
    if(slayer_color_channel(113,160)!=71)return 4;
    if(slayer_color_channel(116,255)!=116)return 5;
    if(slayer_color_channel(58,255)!=58)return 6;
    if(slayer_color_channel(0,160)!=0)return 7;
}
