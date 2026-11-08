#pragma once
//#include "RM.hpp"
#include <cstdint>
#include <vector>

using namespace std;

class VirtualMachine {
    private:
    CPU &cpu;
    RAM &ram;

    uint16_t PLR;
    bool halted = false;

    uint16_t BLOCK_COUNT = 16;
    uint16_t WORDS_PER_BLOCK = 16;
    uint16_t WORD_SIZE = 4;
    uint16_t MEMORY_SIZE = BLOCK_COUNT * WORDS_PER_BLOCK;

    uint16_t translateAddress(uint16_t virtualAddress){
        uint16_t block = virtualAddress / (WORDS_PER_BLOCK * WORD_SIZE);
        uint16_t offset = virtualAddress % (WORDS_PER_BLOCK * WORD_SIZE);

        uint32_t physicalBlock = ram.getWord(PLR + block * 4);

        return physicalBlock * WORDS_PER_BLOCK * WORD_SIZE + offset;
    }

    public:
    VirtualMachine(CPU &cpu, RAM &ram, uint16_t PLR) : cpu(cpu), ram(ram), PLR(PLR) {}

    void loadProgram(const vector<uint32_t> &instructions){
        uint16_t virtualAddress = 0;

        for(const uint32_t &instruction : instructions){
            uint16_t physicalAddress = translateAddress(virtualAddress);

            ram.setWord(physicalAddress, instruction);

            virtualAddress += WORD_SIZE;
        }

        cpu.PLR = PLR;
        cpu.IC = 0;

        halted = false;
    }

    void run(){
        cpu.PLR = PLR;
        halted = false;

        while(!halted){
            cpu.run();
        }
    }

    void stop(){
        halted = true;
    }
};