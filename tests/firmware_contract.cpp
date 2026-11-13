// SPDX-License-Identifier: GPL-2.0-only
#include "../kernel/firmware_contract.h"
#include <array>
#include <iostream>
int main()
{
    // Firmware attributes and native board/project/keyboard bytes must match.
    std::array<unsigned char, SLAYER_OEM_SIZE> original{};
    original.fill(0xa5);
    original[0]=0x05;original[1]=0x1d;original[2]=0x5c;original[3]=0x60;
    original[6]=26;original[7]=16;original[SLAYER_MUX_OFFSET]=0;
    if(!slayer_oem_valid(original.data(),original.size(),7))return 1;
    for(unsigned long size=0;size<200;++size)
        if(size!=original.size()&&slayer_oem_valid(original.data(),size,7))return 2;
    for(unsigned int attrs=0;attrs<256;++attrs)
        if(attrs!=7&&slayer_oem_valid(original.data(),original.size(),attrs))return 3;
    for(auto index:{0,1,2,3,6,7})for(unsigned int bit=0;bit<8;++bit){
        auto data=original;data[index]^=1u<<bit;
        if(slayer_oem_valid(data.data(),data.size(),7))return 4;
    }
    for(unsigned int mode=0;mode<256;++mode){
        auto data=original;data[SLAYER_MUX_OFFSET]=mode;
        if(slayer_oem_valid(data.data(),data.size(),7)!=(mode<=2))return 5;
        for(unsigned int i=0;i<data.size();++i)
            if(i!=SLAYER_MUX_OFFSET&&data[i]!=original[i])return 6;
    }
    // Every unrelated bit must survive normal, Turbo, Boost, Fn and USB writes.
    unsigned int checked=0;
    for(unsigned int old=0;old<256;++old)
        for(auto mask:{0x10u,0x40u,0xb0u})for(unsigned int value=0;value<256;++value){
            auto next=slayer_mask_bits(old,mask,value);
            if((next&~mask)!=(old&~mask)||(next&mask)!=(value&mask))return 7;
            auto restored=slayer_mask_bits(next,mask,old);
            if(restored!=old)return 8;
            ++checked;
        }
    std::cout<<checked<<" masked-control cases; native EFI identity/size/attribute gates passed.\n";
}
