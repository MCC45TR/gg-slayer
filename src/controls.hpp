// SPDX-License-Identifier: MIT
#pragma once
#include "rgb.hpp"
namespace slayer {
std::string hardwareProfilePath(const Io&);
std::vector<std::string> cpuPolicies(const Io&);
void cpuControl(const Io&,const std::vector<std::string>&);
void gpuLimits(const Io&,const std::vector<std::string>&);
void fanCurve(const Io&,const std::vector<std::string>&);
void platformSetting(const Io&,const std::vector<std::string>&);
std::string cameraPath(const Io&);
void semanticWrite(const Io&, const std::string&, const std::string&, const std::string&, const std::string&);
bool acOnline(const Io&);
}
