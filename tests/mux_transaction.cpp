// SPDX-License-Identifier: GPL-2.0-only
#include "../kernel/mux_transaction.h"
#include <array>
#include <iostream>
#include <set>
struct Firmware {
    std::array<unsigned char,SLAYER_OEM_SIZE> bytes{};
    int reads=0,writes=0,failBefore=0,failAfter=0,changeOther=0;
    std::set<int> failReads;
    Firmware(){bytes.fill(0xa5);bytes[0]=5;bytes[1]=0x1d;bytes[2]=0x5c;bytes[3]=0x60;bytes[6]=26;bytes[7]=16;bytes[SLAYER_MUX_OFFSET]=0;}
    static int read(void* ptr,unsigned char* data){
        auto& f=*static_cast<Firmware*>(ptr);
        if(f.failReads.contains(++f.reads))return -EIO;
        memcpy(data,f.bytes.data(),f.bytes.size());
        return slayer_oem_valid(data,SLAYER_OEM_SIZE,7)?0:-EPROTO;
    }
    static int write(void* ptr,unsigned char* data){
        auto& f=*static_cast<Firmware*>(ptr);
        if(++f.writes==f.failBefore)return -EIO;
        memcpy(f.bytes.data(),data,f.bytes.size());
        if(f.writes==f.changeOther)f.bytes[20]^=1;
        return f.writes==f.failAfter?-EIO:0;
    }
    int set(unsigned int value){const slayer_mux_ops ops{read,write};return slayer_mux_update(value,&ops,this);}
};
int main(){
    for(unsigned int mode=0;mode<3;++mode){Firmware f;const auto old=f.bytes;if(f.set(mode))return 1;for(unsigned i=0;i<f.bytes.size();++i)if(f.bytes[i]!=(i==SLAYER_MUX_OFFSET?mode:old[i]))return 2;if(f.writes!=(mode?1:0))return 3;}
    {Firmware f;if(f.set(3)!=-EINVAL||f.reads||f.writes)return 4;}
    {Firmware f;f.failReads={1};if(f.set(2)!=-EIO||f.writes)return 5;}
    {Firmware f;const auto old=f.bytes;f.failBefore=1;if(f.set(2)!=-EIO||f.bytes!=old||f.writes!=1)return 6;}
    {Firmware f;const auto old=f.bytes;f.failAfter=1;if(f.set(2)!=-EIO||f.bytes!=old||f.writes!=2)return 7;}
    {Firmware f;const auto old=f.bytes;f.failReads={2};if(f.set(2)!=-EIO||f.bytes!=old||f.writes!=2)return 8;}
    {Firmware f;auto old=f.bytes;old[20]^=1;f.changeOther=1;if(f.set(2)!=-EIO||f.bytes!=old||f.writes!=2)return 9;}
    {Firmware f;f.failReads={2};f.failBefore=2;if(f.set(2)!=-EUCLEAN||f.bytes[SLAYER_MUX_OFFSET]!=2)return 10;}
    {Firmware f;auto old=f.bytes;f.failReads={2};f.failAfter=2;if(f.set(2)!=-EIO||f.bytes!=old)return 11;}
    {Firmware f;f.failReads={2,3};if(f.set(2)!=-EUCLEAN||f.writes!=1)return 12;}
    std::cout<<"MUX commit/readback/rollback fault cases passed; no EFI device accessed.\n";
}
