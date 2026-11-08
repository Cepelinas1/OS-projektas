#include "RM.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
using namespace std;

void interruptHandler(uint8_t PI, uint8_t SI, uint16_t);

RAM ram;
Keyboard keyboard;
Screen screen;
HardDrive hardDrive;
ChannelDevice channelDevice(ram, hardDrive, keyboard, screen);
CPU cpu(ram, channelDevice, interruptHandler);

bool halted = false;
int inputCount = 0;

void setPageTableEntry(uint16_t virtualAddress, uint16_t physicalAddress){
    ram.setByte(cpu.PLR + virtualAddress, physicalAddress >> 8);
    ram.setByte(cpu.PLR + virtualAddress + 1, physicalAddress & 0xFF);
}

void writeInstruction(uint16_t address, uint8_t instruction[4]){
    for(int i = 0; i < 4; i++){
        ram.setByte(address + i, instruction[i]);
    }
}

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

        // Convert the four ASCII digits into a binary integer.
        uint32_t value = parseNumber(address);
        ram.setWord(address, value);

        // Consume the remaining newline.
        getchar();

        inputCount++;
    }
    else if(SI == 5){ // Output
        uint32_t value = ram.getWord(address);
        string result = to_string(value);

        // Convert the integer into ASCII characters.
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
     // Page table location.
    cpu.PLR = 256;

    // Generate random physical addresses.
    vector<uint16_t> locations;

    for(uint16_t i = 2048; i < 11000; i += 16){
        locations.push_back(i);
    }

    mt19937 generator(random_device{}());
    shuffle(locations.begin(), locations.end(), generator);

    // Virtual instruction addresses.
    uint16_t instructionAddresses[] = {
        0x00, 0x04, 0x08, 0x0C, 0x10,
        0x14, 0x18, 0x1C, 0x20, 0x24
    };

    // Virtual data addresses.
    uint16_t dataAddresses[] = {
        0x40, 0x44, 0x48
    };

    // Assign random physical locations to instructions.
    for(int i = 0; i < 10; i++){
        setPageTableEntry(instructionAddresses[i], locations[i]);
    }

    // Assign random physical locations to data.
    for(int i = 0; i < 3; i++){
        setPageTableEntry(dataAddresses[i], locations[i + 10]);
    }

    // Program:
    // IN40
    // IN44
    // LD40
    // SWAP
    // LD44
    // SWAP
    // ADDR
    // ST48
    // OU48
    // HALT

    uint8_t program[10][4] = {
        {'I', 'N', '4', '0'},
        {'I', 'N', '4', '4'},
        {'L', 'D', '4', '0'},
        {'S', 'W', 'A', 'P'},
        {'L', 'D', '4', '4'},
        {'S', 'W', 'A', 'P'},
        {'A', 'D', 'D', 'R'},
        {'S', 'T', '4', '8'},
        {'O', 'U', '4', '8'},
        {'H', 'L', 'T', '0'}
    };

    for(int i = 0; i < 10; i++){
        writeInstruction(locations[i], program[i]);
    }

    // Initialize CPU.
    cpu.IC = 0;
    cpu.R1 = 200;
    cpu.TI = 200;
    cpu.MODE = CPU::USER;

    cout << "PLR: " << cpu.PLR << endl;

    cout << "\nInstruction mappings:" << endl;
    for(int i = 0; i < 10; i++){
        cout << hex << instructionAddresses[i]
             << " -> " << locations[i] << endl;
    }

    cout << "\nData mappings:" << endl;
    for(int i = 0; i < 3; i++){
        cout << hex << dataAddresses[i]
             << " -> " << locations[i + 10] << endl;
    }

    cout << dec << "\nStarting program...\n\n";

    while(!halted){
        cpu.run();
    }

    cout << "\nProgram halted." << endl;

    cout << "R1: " << cpu.R1 << endl;
    cout << "R2: " << cpu.R2 << endl;
}