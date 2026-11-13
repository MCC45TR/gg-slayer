// SPDX-License-Identifier: MIT
#include "../kernel/ite8291/color_model.h"
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
static std::vector<unsigned char> identity(){
    std::vector<unsigned char> m{'S','C','3','D',1,8,6,0};
    for(int i=0;i<8;++i)for(int copy=0;copy<2;++copy)for(int channel=0;channel<3;++channel)m.push_back((i&(1<<channel))?255:0);
    for(const auto& cell:std::array<std::array<unsigned char,4>,6>{{{0,1,3,7},{0,1,5,7},{0,2,3,7},{0,2,6,7},{0,4,5,7},{0,4,6,7}}})m.insert(m.end(),cell.begin(),cell.end());
    return m;
}
int main(int argc,char** argv){
    auto m=identity();
    if(argc==3){std::ifstream file(argv[2],std::ios::binary);m.assign(std::istreambuf_iterator<char>(file),{});}
    if(!slayer_color_model_valid(m.data(),m.size()))return 1;
    std::vector<int> levels;for(int i=0;i<256;i+=16)levels.push_back(i);levels.push_back(255);
    for(int red:levels)for(int green:levels)for(int blue:levels){
        unsigned char rgb[]{(unsigned char)red,(unsigned char)green,(unsigned char)blue},out[3];
        if(!slayer_color_model_apply(m.data(),rgb,out))return 2;
        if(argc==1){for(int c=0;c<3;++c)if(rgb[c]!=out[c])return 3;}
        else std::cout<<red<<','<<green<<','<<blue<<','<<(int)out[0]<<','<<(int)out[1]<<','<<(int)out[2]<<'\n';
    }
    if(argc!=1)return 0;
    for(unsigned i=0;i<m.size();++i)if(slayer_color_model_valid(m.data(),i))return 4;
    auto invalid=m;invalid[8+8*6]=32;if(slayer_color_model_valid(invalid.data(),invalid.size()))return 5;
    invalid=m;invalid[4]=2;if(slayer_color_model_valid(invalid.data(),invalid.size()))return 6;
    invalid=m;invalid[8+3]=1;if(slayer_color_model_valid(invalid.data(),invalid.size()))return 7;
    invalid=m;invalid[8+7*6+3]=0;if(slayer_color_model_valid(invalid.data(),invalid.size()))return 8;
}
