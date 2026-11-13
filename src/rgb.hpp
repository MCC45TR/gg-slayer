// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <array>
namespace slayer {
struct PhysicalKey {std::string id,label;int row,column,width,index;bool calibrated=false;};
std::vector<PhysicalKey> physicalKeys(const Io&);
void layout(const Io&,const std::vector<std::string>&);
void configuration(const Io&,const std::vector<std::string>&);
std::map<std::string,std::string> configurationValues(const Io&);
void rgbProfile(const Io&,const std::vector<std::string>&);
void validateProfileName(const std::string&);
std::string readProtected(const Io&,const std::string&,std::size_t);
void saveProtected(const Io&,const std::string&,const std::string&);
void saveText(const Io&,const std::string&,const std::string&);
std::map<int,std::string> loadKeyColors(const Io&,const std::string&);
void applyColors(const Io&,const std::map<int,std::string>&,int brightness=-1);
void colorProfile(const Io&,const std::vector<std::string>&);
void hardwareEffect(const Io&,const std::vector<std::string>&);
std::string hardwareDevice(const Io&);
void locate(const Io&,int);
}
