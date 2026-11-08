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

bool openFiles[16] = {};
uint32_t filePositions[16] = {};


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

    cout<< "got interrupt" << static_cast<int>(PI) << " " << static_cast<int>(SI) << "\n";

    uint32_t savedR1 = cpu.R1;

    cpu.R1 = 10;
    uint8_t resetInstruction[4] = {'R', 'S', 'T', 'I'};
    cpu.executeInstruction(resetInstruction);

    cpu.R1 = savedR1;

    if(PI != 0){
        cout << "\nProgram interrupt: ";

        switch(PI){
            case CPU::BAD_ADDRESS:
                cout << "Invalid memory address";
                break;
            case CPU::BAD_INSTRUCTION:
                cout << "Invalid instruction";
                break;
            case CPU::BAD_ASSIGNMENT:
                cout << "Invalid assignment";
                break;
            case CPU::DIVISION_BY_0:
                cout << "Division by zero";
                break;
            default:
                cout << "Unknown program interrupt";
                break;
        }

        cout << endl;
        halted = true;
    }
    else if(SI == CPU::GD){ // Read from file
        uint32_t id = cpu.R1;
        uint32_t length = cpu.R2;

        if(id >= 16 || !openFiles[id]){
            cout << "File is not open." << endl;
            cpu.R1 = UINT32_MAX;
        }
        else{
            channelDevice.ST = ChannelDevice::DRIVE;
            channelDevice.DT = ChannelDevice::USERMEM;
            channelDevice.ID = id;
            channelDevice.OF = filePositions[id];
            channelDevice.DB = address;
            channelDevice.SN = length;

            uint8_t instruction[4] = {'X', 'C', 'H', 'G'};
            cpu.executeInstruction(instruction);

            filePositions[id] += length;
        }
    }
    else if(SI == CPU::PD){ // Write to file
        uint32_t id = cpu.R1;
        uint32_t length = cpu.R2; cout << hex << static_cast<long>(id) << " " << static_cast<long>(length) << "\n";

        if(id >= 16 || !openFiles[id]){
            cout << "File is not open." << endl;
            cpu.R1 = UINT32_MAX;
        }
        else{
            channelDevice.ST = ChannelDevice::USERMEM;
            channelDevice.DT = ChannelDevice::DRIVE;
            channelDevice.ID = id;
            channelDevice.OF = filePositions[id];
            channelDevice.SB = address;
            channelDevice.SN = length;

            uint8_t instruction[4] = {'X', 'C', 'H', 'G'};
            cpu.executeInstruction(instruction);

            filePositions[id] += length;
        }
    }
    else if(SI == CPU::HALT){
        halted = true;
    }
    else if(SI == CPU::IN){ // Input
        cout << (inputCount == 0 ? "Enter first number (4 digits): "
                                 : "Enter second number (4 digits): ") << flush;

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
    else if(SI == CPU::OU){ // Output
        uint32_t value = ram.getWord(address);
        string result = to_string(value);

        for(size_t i = 0; i < result.size(); i++){
            ram.setByte(address + i, result[i]);
        }

        cout << "Output: ";

        channelDevice.ST = ChannelDevice::USERMEM;
        channelDevice.DT = ChannelDevice::OUTPUT;
        channelDevice.SB = address;
        channelDevice.SN = result.size();

        uint8_t instruction[4] = {'X', 'C', 'H', 'G'};
        cpu.executeInstruction(instruction);

        cout << endl;
    }
    else if(SI == CPU::OPEN_FILE){ // Open file
        uint32_t name1 = ram.getWord(cpu.getPagingAddress(address));
        uint32_t name2 = ram.getWord(cpu.getPagingAddress(address + 4));cout << hex << static_cast<long>(name1) << " " << static_cast<long>(name2) << "\n";

        uint32_t id = hardDrive.openFile(name1, name2);

        if(id == UINT32_MAX || id >= 16){
            cout << "File not found." << endl;
            cpu.R1 = UINT32_MAX;
        }
        else{
            openFiles[id] = true;
            filePositions[id] = 0;
            cpu.R1 = id;
        }
    }
    else if(SI == CPU::CLOSE_FILE){ // Close file
        uint32_t id = cpu.R1;

        if(id < 16 && openFiles[id]){
            openFiles[id] = false;
            filePositions[id] = 0;
        }
        else{
            cout << "File is not open." << endl;
        }
    }
    else if(SI == CPU::DELETE_FILE){ // Delete file
        uint32_t id = cpu.R1;

        if(id < 16 && hardDrive.deleteFile(id)){
            openFiles[id] = false;
            filePositions[id] = 0;
        }
        else{
            cout << "Could not delete file." << endl;
        }
    }
    else if(SI == CPU::CHANGE_FILE_POINTER){ // Change file position
        uint32_t id = cpu.R1;

        if(id < 16 && openFiles[id]){
            filePositions[id] = cpu.R2;
        }
        else{
            cout << "File is not open." << endl;
        }
    }

    cpu.IC = savedIC;
    cpu.MODE = savedMode;
}

int main(){
    channelDevice.setCPU(cpu);
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

    // Open the executable file.
    uint32_t name1 = 0x46544553; // FTES
    uint32_t name2 = 0x54534558; // TSEX

    int fileID = hardDrive.openFile(name1, name2);

    if(fileID < 0){
        cout << "Program file not found." << endl;
        return 1;
    }

    // Read the file contents into 4-byte words.
    vector<uint32_t> program;
    
    uint32_t size = hardDrive.getFileSize(fileID);

    for(uint32_t offset = 0; offset < size; offset += 4){
        uint32_t word = 0;

        for(int i = 0; i < 4; i++){
            word = (word << 8) | hardDrive.readFileByte(fileID, offset + i);
        }

        cout << hex << word << " ";
        program.push_back(word);
    }

    VirtualMachine vm(cpu, ram, PLR);
    vm.loadProgram(program);

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
        vm.run();
    }

    cout << "\nProgram halted." << endl;

    cout << "R1: " << cpu.R1 << endl;
    cout << "R2: " << cpu.R2 << endl;
}

