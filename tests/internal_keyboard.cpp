// SPDX-License-Identifier: MIT
#include "../kernel/internal_keyboard_contract.h"
#include <iostream>
int main(){
    if(!slayer_internal_keyboard(0x11,"isa0060/serio0/input0","serio0","atkbd"))return 1;
    // A matching display name, spoofed phys string or USB port is insufficient.
    if(slayer_internal_keyboard(3,"isa0060/serio0/input0","serio0","atkbd"))return 2;
    if(slayer_internal_keyboard(5,"isa0060/serio0/input0","serio0","atkbd"))return 3;
    if(slayer_internal_keyboard(0x11,"isa0060/serio0/input0","input99","uinput"))return 4;
    if(slayer_internal_keyboard(0x11,"isa0060/serio1/input0","serio1","atkbd"))return 5;
    if(slayer_internal_keyboard(0x11,"usb-0000:06:00.4-2.1/input0","serio0","atkbd"))return 6;
    if(slayer_internal_keyboard(0x11,nullptr,"serio0","atkbd"))return 7;
    std::cout<<"Only the physical i8042/serio0 AT keyboard matches; USB, Bluetooth, virtual devices and other ports are excluded.\n";
}
