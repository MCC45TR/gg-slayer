// SPDX-License-Identifier: GPL-2.0-only
#include "../kernel/profile_bits.h"
int main(){
    for(unsigned int old=0;old<256;++old)for(unsigned int profile=0;profile<8;++profile){
        unsigned char result=0xaa;
        int status=slayer_profile_bits(static_cast<unsigned char>(old),profile,&result);
        bool valid=profile<=2&&((old>>4)&3)<=2;
        if((status==0)!=valid)return 1;
        if(valid&&((result&0xcf)!=(old&0xcf)||((result>>4)&3)!=profile))return 2;
        if(!valid&&result!=0xaa)return 3;
    }
    return 0;
}
