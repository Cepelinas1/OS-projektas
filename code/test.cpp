#include "RM.hpp"
#include "VM.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
using namespace std;

void interruptHandler(uint8_t PI, uint8_t SI, uint16_t address);

RAM ram;
Keyboard keyboard;
Screen screen;
HardDrive hardDrive;
ChannelDevice channelDevice(ram, hardDrive, keyboard, screen);
CPU cpu(ram, channelDevice, interruptHandler);

bool halted = false;
int inputCount = 0;

uint32_t parseNumber(uint16_t address){
    uint32_t value = 0;
    for(int i = 0; i < 4; i++){
        uint8_t digit = ram.getByte(address + i);

        if(digit < '0' || digit > '9'){
            return 0;
        }
        value = value * 10 + digit - '0';
    }
    return value;
}

void interruptHandler(uint8_t PI, uint8_t SI, uint16_t address){
    uint16_t savedIC = cpu.IC;
    uint8_t savedMode = cpu.MODE;

    uint8_t instructionClearInterrupts[4] = {'R', 'S', 'T', 'I'};
    cpu.executeInstruction(instructionClearInterrupts);

    if(SI == 4){ // Input
        if(inputCount == 0){
            cout << "Enter first number (4 digits): " << flush;
        }
        else{
            cout << "Enter second number (4 digits): " << flush;
        }

        channelDevice.ST = ChannelDevice::INPUT;
        channelDevice.DT = ChannelDevice::USERMEM;
        channelDevice.DB = address;
        channelDevice.SN = 4;

        uint8_t instruction[4] = {'X', 'C', 'H', 'G'};
        cpu.executeInstruction(instruction);

        uint32_t value = parseNumber(address);
        ram.setWord(address, value);

        getchar();

        inputCount++;
    }
    else if(SI == 5){ // Output
        uint32_t value = ram.getWord(address);
        string result = to_string(value);

        for(size_t i = 0; i < result.size(); i++){
            ram.setByte(address + i, result[i]);
        }

        cout << "Sum: ";

        channelDevice.ST = ChannelDevice::USERMEM;
        channelDevice.DT = ChannelDevice::OUTPUT;
        channelDevice.SB = address;
        channelDevice.SN = result.size();

        uint8_t instruction[4] = {'X', 'C', 'H', 'G'};
        cpu.executeInstruction(instruction);

        cout << endl;
    }
    else if(SI == 3){ // Halt
        halted = true;
    }

    cpu.IC = savedIC;
    cpu.MODE = savedMode;
}

int main(){
    uint16_t PLR = 256;

    vector<uint16_t> locations;

    for(uint16_t i = 2048; i < 11000; i += 64){
        locations.push_back(i / 64);
    }

    mt19937 generator(random_device{}());
    shuffle(locations.begin(), locations.end(), generator);

    // Assign physical blocks to the VM.
    for(int i = 0; i < 16; i++){
        ram.setWord(PLR + i * 4, locations[i]);
    }

    vector<uint32_t> program = {
        0x494E3430, // IN40
        0x494E3434, // IN44
        0x4C443430, // LD40
        0x53574150, // SWAP
        0x4C443434, // LD44
        0x53574150, // SWAP
        0x41444452, // ADDR
        0x53543438, // ST48
        0x4F553438, // OU48
        0x48414C54  // HALT
    };

    VirtualMachine vm(cpu, ram, PLR);
    vm.loadProgram(program);

    cpu.IC = 0;
    cpu.R1 = 200;
    cpu.TI = 200;
    cpu.MODE = CPU::USER;

    cout << "PLR: " << cpu.PLR << endl;

    cout << "\nPhysical block mappings:" << endl;
    for(int i = 0; i < 16; i++){
        cout << "Virtual block " << i
             << " -> Physical block " << locations[i] << endl;
    }

    cout << dec << "\nStarting program...\n\n";

    while(!halted){
        cpu.run();
    }

    cout << "\nProgram halted." << endl;

    cout << "R1: " << cpu.R1 << endl;
    cout << "R2: " << cpu.R2 << endl;
}