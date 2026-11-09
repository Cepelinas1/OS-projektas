#include "RM.hpp"
#include "VM.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <iomanip>
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

void interruptHandler(uint8_t PI, uint8_t SI, uint16_t address){
    uint16_t savedIC = cpu.IC;
    uint8_t savedMode = cpu.MODE;

    cout<< "GOT INTERRUPT :^O --> " << static_cast<int>(PI) << " " << static_cast<int>(SI) << "\n";

    uint32_t savedR1 = cpu.R1;

    cpu.R1 = 10;
    uint8_t resetInstruction[4] = {'R', 'S', 'T', 'I'};
    cpu.executeInstruction(resetInstruction);

    cpu.R1 = savedR1;

    if(PI != 0){
        cout << "\nPI: ";
        switch(PI){
            case CPU::BAD_ADDRESS:
                cout << "Invalid memory address";
                halted = true;
                break;
            case CPU::BAD_INSTRUCTION:
                cout << "Invalid instruction";
                halted = true;
                break;
            case CPU::BAD_ASSIGNMENT:
                cout << "Invalid assignment";
                halted = true;
                break;
            case CPU::DIVISION_BY_0:
                cout << "Division by zero";
                halted = true;
                break;
        }
        cout << endl;
    }
    else if(SI == CPU::GD){ // GDxy
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
    else if(SI == CPU::PD){ // PDxy
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
    else if(SI == CPU::IN){ // INxy

        cout << "WRITE INPUT (" << static_cast<long>(cpu.R1) << " symbols): ";

        channelDevice.ST = ChannelDevice::INPUT;
        channelDevice.DT = ChannelDevice::USERMEM;
        channelDevice.DB = cpu.getPagingAddress(address);
        channelDevice.SN = cpu.R1;

        uint8_t instruction[4] = {'X', 'C', 'H', 'G'};
        cpu.executeInstruction(instruction);
    }
    else if(SI == CPU::OU){ // OUxy

        cout << "OUTPUT: ";

        channelDevice.ST = ChannelDevice::USERMEM;
        channelDevice.DT = ChannelDevice::OUTPUT;
        channelDevice.SB = cpu.getPagingAddress(address);
        channelDevice.SN = cpu.R1;

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
    else if(cpu.TI == 0){
        cpu.TI = 10;
    }

    cpu.IC = savedIC;
    cpu.MODE = CPU::USER;
}

int main(){
    channelDevice.setCPU(cpu);

    // Setting up the paging table
    uint16_t PLR = 0;

    vector<uint16_t> blockTable;

    for(uint16_t i = 1024; i < 11264; i += 64){
        blockTable.push_back(i / 64);
    }

    mt19937 generator(random_device{}());
    shuffle(blockTable.begin(), blockTable.end(), generator);

    // Adding the paging table to memory
    for(int i = 0; i < 16; ++i){
        ram.setWord(PLR + i * 4, blockTable[i]);
    }

    // Load the program to memory
    uint32_t name1 = 0x494E504F; // INPO
    uint32_t name2 = 0x55544558; // UTEX

    uint32_t fileID = hardDrive.openFile(name1, name2);

    if(fileID == UINT32_MAX){
        cout << "Program file not found." << endl;
        return 1;
    }

    // Read the file.
    vector<uint32_t> program;
    
    uint32_t size = hardDrive.getFileSize(fileID);

    cout << "Loaded program:";
    for(uint32_t offset = 0; offset < size; offset += 4){
        uint32_t word = 0;
        for(int i = 0; i < 4; ++i){
            word = (word << 8) | hardDrive.readFileByte(fileID, offset + i);
        }
        cout << hex << word << " ";
        program.push_back(word);
    }

    // Set up VM and CPU
    VirtualMachine vm(cpu, ram, PLR);
    vm.loadProgram(program);

    cpu.TI = 10;
    cpu.MODE = CPU::USER;

    cout << "\nPaging:\n";
    for(int i = 0; i < 16; i++){
        cout << dec << "Virtual memory block " << i << " is User memory block " << blockTable[i] << "\n";
    }

    cout << "STARTING PROGRAM :^)\n";
    cout << "'a' to advance, 'm' to print memory, 'r' to print all memory, 'c' to print channel device registers, 'd' to print hard drive information\n";

    char input;
    while(!halted && (input = getchar()) != EOF){
        char c;
        while((c = getchar()) != '\n' && c != EOF);
        if(input == 'a'){
            cout << "Current CPU register values:\n";
            cout << hex << " R1:" << static_cast<long>(cpu.R1) << " R2:" << static_cast<long>(cpu.R2) << " IC:" << static_cast<long>(cpu.IC) << " SF:" << static_cast<long>(cpu.SF) << " MODE:" 
                 << cpu.MODE << " PLR:" << static_cast<long>(cpu.PLR) << " PI:" << static_cast<long>(cpu.PI) << " SI:" << static_cast<long>(cpu.SI) 
                 << " TI:" << static_cast<long>(cpu.TI) << "\n";

            cout << " Executing instruction: ";
            uint32_t instructionWord = ram.getWord(cpu.getPagingAddress(cpu.IC));
            uint8_t instruction[4] = {(instructionWord >> 24) & 0xFF, (instructionWord >> 16) & 0xFF, (instructionWord >> 8) & 0xFF, instructionWord & 0xFF};
            for(int i = 0; i < 4; ++i){
                cout << static_cast<char>(instruction[i]);
            }
            cout<<"\n";
            
            vm.run();
        }
        else if(input == 'm'){
            cout << hex << setfill('0');
            for(uint32_t address = 0; address < 16 * 64; address += 4){
                if(address % 64 == 0){
                    cout << "\n" << setw(4) << address / 64 << ": ";
                }
                cout << setw(8) << ram.getWord(cpu.getPagingAddress(address)) << " ";
            }
            cout << dec << setfill(' ') << "\n";
        }
        else if(input == 'r'){
            cout << hex << setfill('0');
            for(uint32_t address = 0; address < 176 * 64; address += 4){
                if(address % 64 == 0){
                    cout << "\n" << setw(4) << address / 64 << ": ";
                }
                cout << setw(8) << ram.getWord(address) << " ";
            }
            cout << dec << setfill(' ') << "\n";
        }
        else if(input == 'c'){
            cout << "Current channel device register values:\n";
            cout << hex << " SB:" << static_cast<long>(channelDevice.SB) << " DB:" << static_cast<long>(channelDevice.DB) << " OF:" << static_cast<long>(channelDevice.OF) 
                 << " SN:" << static_cast<long>(channelDevice.SN) << " ID:" << static_cast<long>(channelDevice.ID) << " ST:" << static_cast<long>(channelDevice.ST) 
                 << " DT:" << static_cast<long>(channelDevice.DT) << "\n";
        }
        else if(input == 'd'){
            cout << "Current hard drive status:\n";
            cout << hex << " SIZE:" << static_cast<long>(hardDrive.diskSize) << " NEXT FREE ADDRESS:" << static_cast<long>(hardDrive.nextFreeAddress)
                 << " FILE COUNT:" << static_cast<long>(hardDrive.fileCount) << "\n";
            cout << "Files: \n";
            for(int i = 0; i < hardDrive.fileCount; ++i){
                cout << " NAME:" << static_cast<long>(hardDrive.files[i].name1) << static_cast<long>(hardDrive.files[i].name2) << " START:" << static_cast<long>(hardDrive.files[i].startAddress) << "\n";
            }
        }
    }

    cout << "\nPROGRAM HALTED >:^(";
}

