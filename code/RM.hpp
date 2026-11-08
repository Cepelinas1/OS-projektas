#include <cstdint>
#include <iostream>
#include <cstdio>
using namespace std;

class RAM{
    private:
    uint8_t memory[11264]; // Supervisor 1024 + user 10240, maybe needs to be separated

    public:

    uint8_t getByte(uint16_t address){
        return memory[address];
    }

    uint8_t* getBytePtr(uint16_t address){
        return &memory[address];
    }

    void setByte(uint16_t address, uint8_t byte){
        memory[address] = byte;
    }

    uint32_t getWord(uint16_t address){
        return (static_cast<uint32_t>(memory[address]) << 24) |
               (static_cast<uint32_t>(memory[address + 1]) << 16) |
               (static_cast<uint32_t>(memory[address + 2]) << 8) |
                static_cast<uint32_t>(memory[address + 3]);
    }

    void setWord(uint16_t address, uint32_t word){
        memory[address] = (word >> 24) & 0xFF;
        memory[address + 1] = (word >> 16) & 0xFF;
        memory[address + 2] = (word >> 8) & 0xFF;
        memory[address + 3] = word & 0xFF;
    }
};

class HardDrive{
    private:
    FILE* disk;

    uint32_t diskSize = 1024*1024;
    uint32_t nextFreeAddress = 268;
    uint32_t fileCount = 0;

    struct FileInfo{
        uint32_t name1;
        uint32_t name2;
        uint32_t startAddress;
        uint32_t size;
    };

    FileInfo files[16];

    public:
    HardDrive()
    {
        disk = fopen("disk.bin", "r+b");

        if(disk == nullptr) // jei nera failo
        {
            disk = fopen("disk.bin", "w+b"); // kuria faila

            writeWord(0, diskSize);
            writeWord(4, nextFreeAddress);
            writeWord(8, fileCount);
        }
        else // jei egzistuoja failas
        {
            diskSize = readWord(0);
            nextFreeAddress = readWord(4);
            fileCount = readWord(8);

            for (uint32_t i = 0; i< fileCount; ++i)
            {
                uint32_t address = 12 + i * 16;
                files[i].name1 = readWord(address);
                files[i].name2 = readWord(address + 4);
                files[i].startAddress = readWord(address + 8);
                files[i].size = readWord(address + 12);
            }
        }
    }
    ~HardDrive()
    {
        fclose(disk);
    }
    // ziuret kad neuzraso ant kitu
    uint8_t readByte(uint32_t address)
    {
        fseek(disk, address, SEEK_SET);
        return static_cast<uint8_t>(fgetc(disk));
    }

    void writeByte(uint32_t address, uint8_t value)
    {
        fseek(disk, address, SEEK_SET);
        fputc(value, disk);
    }

    uint32_t readWord(uint32_t address)
    {
        uint32_t value = 0;
        for (int i= 0; i<4; ++i)
        {
            value = (value << 8) | readByte(address + i);
        }
        return value;
    }

    void writeWord(uint32_t address, uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
        {
            writeByte(address + i, (value >> (24 - i * 8)) & 0xFF);
        }
    }

    uint32_t createFile(uint32_t name1, uint32_t name2, uint32_t size)
    {
        uint32_t id = fileCount;
        uint32_t start = nextFreeAddress;

        writeWord(start, name1);
        writeWord(start + 4, name2);
        writeWord(start + 8, size);
        writeWord(start + 12, 0);
        writeWord(start + 16, size);

        for (uint32_t i = 0; i < size; ++i){
            writeByte(start + 20 + i, 0);
        }

        writeWord(start + 20 + size, 0x24454E44); // $END (papildoma zyme)

        files[id] = {name1, name2, start, size};
        uint32_t table = 12 + id * 16;
        writeWord(table, name1);
        writeWord(table + 4, name2);
        writeWord(table + 8, start);
        writeWord(table + 12, size);

        fileCount++;
        nextFreeAddress += 20 + size + 4;
        writeWord(4, nextFreeAddress);
        writeWord(8, fileCount);
        fflush(disk);
        return id;
    }

    uint32_t openFile(uint32_t name1, uint32_t name2)
    {
        for(uint32_t i=0; i < fileCount; ++i)
        {
            if(files[i].name1 == name1 && files[i].name2 == name2)
                return i;
        }
        return UINT32_MAX;
    }

    uint8_t readFileByte(uint32_t id, uint32_t offset)
    {
        return readByte(files[id].startAddress + 20 + offset);
    }

    void writeFileByte(uint32_t id, uint32_t offset, uint8_t value)
    {
        writeByte(files[id].startAddress + 20 + offset, value);
    }

    uint32_t programStart(uint32_t id)
    {
        return readWord(files[id].startAddress + 20);
    }
};

class Keyboard{
    public:
    uint8_t readByte(){
        uint8_t result;
        scanf("%1c", &result);
        return result;
    }
};

class Screen{
    public:
    void print(uint8_t *message, uint32_t length){
        for(int i = 0; i < length; ++i){
            cout << static_cast<char>(message[i]);
        }
    }
};

class ChannelDevice{
    public:
    // Registers
    uint32_t SB = 0;
    uint32_t DB = 0;
    uint32_t OF = 0;
    uint32_t SN = 0;
    uint32_t ID = 0;
    uint8_t ST = 0;
    uint8_t DT = 0;

    // ST/DT values
    enum Devices {USERMEM = 1, SUPERVISORMEM = 2, DRIVE = 3, INPUT = 4, OUTPUT = 4};

    // Pointers to devices
    RAM &ram;
    HardDrive &drive;
    Keyboard &input;
    Screen &output;

    ChannelDevice(RAM &ram, HardDrive &drive, Keyboard &input, Screen &output) 
                    : ram(ram), drive(drive), input(input), output(output){}

    void run(){
        if(ST == USERMEM && DT == USERMEM){
            // NOT USED
        }
        else if(ST == USERMEM && DT == SUPERVISORMEM){
            // NOT USED
        }
        else if(ST == USERMEM && DT == DRIVE){
            for(int i = 0; i < SN; ++i){
                drive.writeByte(DB + i, input.readByte());
            }
        }
        else if(ST == USERMEM && DT == OUTPUT){
            output.print(ram.getBytePtr(SB), SN);
        }
        else if(ST == SUPERVISORMEM && DT == USERMEM){
            for(int i = 0; i < SN; ++i){
                drive.writeByte(DB + i, input.readByte());
            }
        }
        else if(ST == SUPERVISORMEM && DT == SUPERVISORMEM){
            // NOT USED
        }
        else if(ST == SUPERVISORMEM && DT == DRIVE){
            // NOT USED
        }
        else if(ST == SUPERVISORMEM && DT == OUTPUT){
            output.print(ram.getBytePtr(SB), SN);
        }
        else if(ST == DRIVE && DT == USERMEM){
            for(int i = 0; i < SN; ++i){
                ram.setByte(DB + i, drive.readByte(SB + i));
            }
        }
        else if(ST == DRIVE && DT == SUPERVISORMEM){
            for(int i = 0; i < SN; ++i){
                ram.setByte(DB + i, drive.readByte(SB + i));
            }
        }
        else if(ST == DRIVE && DT == DRIVE){
            // NOT USED
        }
        else if(ST == DRIVE && DT == OUTPUT){
            // NOT USED
        }
        else if(ST == INPUT && DT == USERMEM){
            for(int i = 0; i < SN; ++i){
                ram.setByte(DB + i, input.readByte());
            }
        }
        else if(ST == INPUT && DT == SUPERVISORMEM){
            for(int i = 0; i < SN; ++i){
                ram.setByte(DB + i, input.readByte());
            }
        }
        else if(ST == INPUT && DT == DRIVE){
            // NOT USED
        }
        else if(ST == INPUT && DT == OUTPUT){
            // NOT USED
        }
    }
};

class CPU{
    public:
    // CPU registers
    uint16_t IC = 0;
    uint32_t R1 = 0;
    uint32_t R2 = 0;
    uint8_t SF = 0;
    uint8_t MODE = 0;
    uint16_t PLR = 0;
    uint8_t PI = 0;
    uint8_t SI = 0;
    uint8_t TI = 0;

    // SF FLAG NUMBERS
    enum Flags {CARRY, ZERO, SIGN, OVERFLOW};
    // CPU modes
    enum Modes {USER = 'N', SUPERVISOR = 'S'};
    // Interupts
    enum PInts {BAD_ADDRESS = 1, BAD_INSTRUCTION = 2,
                BAD_ASSIGNMENT = 3, DIVISION_BY_0 = 4};
    enum SInts {GD = 1, PD = 2, HALT = 3, IN = 4, OU = 5,
                OPEN_FILE = 6, CLOSE_FILE = 7, DELETE_FILE = 8,
                CHANGE_FILE_POINTER = 9};

    // Pointers to other components
    RAM &ram;
    ChannelDevice &channelDevice;

    public:
    // Pointer to an interrupt handler
    void (*interruptHandler)(uint8_t, uint8_t, uint16_t);

    CPU(RAM &ram, ChannelDevice &channelDevice, void (*interruptHandler)(uint8_t, uint8_t, uint16_t)) 
            : ram(ram), channelDevice(channelDevice), interruptHandler(interruptHandler){
        MODE = SUPERVISOR;
    }

    // FOR SETTING FLAGS IN SF
    void setFlag(uint8_t flag){
        SF |= (1 << flag);
    }

    void clearFlag(uint8_t flag){
        SF &= ~(1 << flag);
    }

    void clearFlags(){
        clearFlag(CARRY);
        clearFlag(ZERO);
        clearFlag(SIGN);
        clearFlag(OVERFLOW);
    }

    bool getFlag(uint8_t flag){
        return SF & (1 << flag);
    }

    // Calculating real addresses
    uint16_t getPagingAddress(uint16_t virtualAddress){
        uint16_t address = (ram.getByte(PLR + virtualAddress) << 8) | ram.getByte(PLR + virtualAddress + 1);
        return address;
    }

    // Searching for interrupts
    void testInterrupts(){
        if((PI + SI) > 0 || TI == 0){
            MODE = SUPERVISOR;
            uint16_t address = 0;
            if(SI == 4 || SI == 5){
                uint16_t instructionAddress = getPagingAddress(IC - 4);
                address = (ram.getByte(instructionAddress + 2) >= 'A' ? ram.getByte(instructionAddress + 2) - 'A' + 10 : ram.getByte(instructionAddress + 2) - '0') * 16
                                    + (ram.getByte(instructionAddress + 3) >= 'A' ? ram.getByte(instructionAddress + 3) - 'A' + 10 : ram.getByte(instructionAddress + 3) - '0');
                address = getPagingAddress(address);
            }
            interruptHandler(PI, SI, address);
        }
    }

    // Executing instructions
    void executeInstruction(uint8_t instruction[4]){
        switch((instruction[0] << 8) | instruction[1]){ // 2 bytes of each instruction are unique
            case ('L' << 8) | 'D': { // LDxy
                uint16_t address = (instruction[2] >= 'A' ? instruction[2] - 'A' + 10 : instruction[2] - '0') * 16 
                                    + (instruction[3] >= 'A' ? instruction[3] - 'A' + 10 : instruction[3] - '0');
                R1 = ram.getWord(getPagingAddress(address));
                IC += 4;
                break;
            }
            case ('S' << 8) | 'W' : { // SWAP
                uint32_t temp = R1;
                R1 = R2;
                R2 = temp;
                IC += 4;
                break;
            }
            case ('S' << 8) | 'T' : { // STxy
                uint16_t address = (instruction[2] >= 'A' ? instruction[2] - 'A' + 10 : instruction[2] - '0') * 16 
                                    + (instruction[3] >= 'A' ? instruction[3] - 'A' + 10 : instruction[3] - '0');
                ram.setWord(getPagingAddress(address), R1);
                IC += 4;
                break;
            }
            case ('A' << 8) | 'D' : { // ADDR
                clearFlags();

                uint32_t oldR1 = R1;
                R1 += R2;

                if(R1 < oldR1){
                    setFlag(CARRY);
                }
                if(R1 == 0){
                    setFlag(ZERO);
                }
                if(R1 & 0x80000000){
                    setFlag(SIGN);
                }
                if(~(oldR1 ^ R2) & (oldR1 ^ R1) & 0x80000000){
                    setFlag(OVERFLOW);
                }
                IC += 4;
                break;
            }
            case ('S' << 8) | 'U' : { // SUBR
                clearFlags();

                uint32_t oldR1 = R1;
                R1 -= R2;

                if(oldR1 < R2){
                    setFlag(CARRY);
                }
                if(R1 == 0){
                    setFlag(ZERO);
                }
                if(R1 & 0x80000000){
                    setFlag(SIGN);
                }
                if((oldR1 ^ R2) & (oldR1 ^ R1) & 0x80000000){
                    setFlag(OVERFLOW);
                }
                break;
            }
            case ('M' << 8) | 'U' : { // MULR
                clearFlags();
                
                int32_t a = (int32_t)R1;
                int32_t b = (int32_t)R2;

                if(a != 0 && b != 0){
                    if((a == -1 && b == INT32_MIN) || (b == -1 && a == INT32_MIN) || (a > 0 && b > 0 && a > INT32_MAX / b) ||
                        (a > 0 && b < 0 && b < INT32_MIN / a) || (a < 0 && b > 0 && a < INT32_MIN / b) || (a < 0 && b < 0 && a < INT32_MAX / b)){
                        setFlag(OVERFLOW);
                    }
                }

                R1 *= R2;

                if(R1 == 0){
                    setFlag(ZERO);
                }
                if(R1 & 0x80000000){
                    setFlag(SIGN);
                }
                IC += 4;
                break;
            }
            case ('D' << 8) | 'I': { // DIVR
                clearFlags();

                if(R2 == 0){
                    PI = DIVISION_BY_0;
                    IC += 4;
                    break;
                }
                else if((int32_t)R1 == INT32_MIN && (int32_t)R2 == -1){
                    setFlag(OVERFLOW);
                    break;
                }

                int32_t result = (int32_t)R1 / (int32_t)R2;
                R1 = (uint32_t)result;

                if(R1 == 0){
                    setFlag(ZERO);
                }
                if(R1 & 0x80000000){
                    setFlag(SIGN);
                }
                IC += 4;
                break;
            }
            case ('C' << 8) | 'O': { // COMP
                clearFlags();

                uint32_t temp = R1 -= R2;

                if(R1 < R2){
                    setFlag(CARRY);
                }
                if(temp == 0){
                    setFlag(ZERO);
                }
                if(temp & 0x80000000){
                    setFlag(SIGN);
                }
                if((R1 ^ R2) & (R1 ^ temp) & 0x80000000){
                    setFlag(OVERFLOW);
                }
                IC += 4;
                break;
            }
            case ('J' << 8) | 'P': { // JPxy
                uint16_t address = (instruction[2] >= 'A' ? instruction[2] - 'A' + 10 : instruction[2] - '0') * 16 
                                    + (instruction[3] >= 'A' ? instruction[3] - 'A' + 10 : instruction[3] - '0');
                IC = ram.getWord(getPagingAddress(address));
                break;
            }
            case ('J' << 8) | '1': { // J1xy
                if(getFlag(ZERO)){
                    uint16_t address = (instruction[2] >= 'A' ? instruction[2] - 'A' + 10 : instruction[2] - '0') * 16 
                                    + (instruction[3] >= 'A' ? instruction[3] - 'A' + 10 : instruction[3] - '0');
                    IC = ram.getWord(getPagingAddress(address));
                }
                else{
                    IC += 4;
                }
                break;
            }
            case ('J' << 8) | '0': { // J0xy
                if(!getFlag(ZERO)){
                    uint16_t address = (instruction[2] >= 'A' ? instruction[2] - 'A' + 10 : instruction[2] - '0') * 16 
                                    + (instruction[3] >= 'A' ? instruction[3] - 'A' + 10 : instruction[3] - '0');
                    IC = ram.getWord(getPagingAddress(address));
                }
                else{
                    IC += 4;
                }
                break;
            }
            case ('J' << 8) | 'A': { // JAxy
                if(!getFlag(ZERO) && !getFlag(SIGN)){
                    uint16_t address = (instruction[2] >= 'A' ? instruction[2] - 'A' + 10 : instruction[2] - '0') * 16 
                                    + (instruction[3] >= 'A' ? instruction[3] - 'A' + 10 : instruction[3] - '0');
                    IC = ram.getWord(getPagingAddress(address));
                }
                else{
                    IC += 4;
                }
                break;
            }
            case ('J' << 8) | 'B': { // JBxy
                if(getFlag(SIGN)){
                    uint16_t address = (instruction[2] >= 'A' ? instruction[2] - 'A' + 10 : instruction[2] - '0') * 16 
                                    + (instruction[3] >= 'A' ? instruction[3] - 'A' + 10 : instruction[3] - '0');
                    IC = ram.getWord(getPagingAddress(address));
                }
                else{
                    IC += 4;
                }
                break;
            }
            case ('J' << 8) | 'C': { // JCxy
                if(getFlag(CARRY)){
                    uint16_t address = (instruction[2] >= 'A' ? instruction[2] - 'A' + 10 : instruction[2] - '0') * 16 
                                    + (instruction[3] >= 'A' ? instruction[3] - 'A' + 10 : instruction[3] - '0');
                    IC = ram.getWord(getPagingAddress(address));
                }
                else{
                    IC += 4;
                }
                break;
            }
            case ('J' << 8) | 'O': { // JOxy
                if(getFlag(OVERFLOW)){
                    uint16_t address = (instruction[2] >= 'A' ? instruction[2] - 'A' + 10 : instruction[2] - '0') * 16 
                                    + (instruction[3] >= 'A' ? instruction[3] - 'A' + 10 : instruction[3] - '0');
                    IC = ram.getWord(getPagingAddress(address));
                }
                else{
                    IC += 4;
                }
                break;
            }
            case ('P' << 8) | 'D': // PDxy
                SI = 2;
                IC += 4;
                break;
            case ('G' << 8) | 'D': // GDxy
                SI = 1;
                IC += 4;
                break;
            case ('F' << 8) | 'M': // FMOV
                SI = 9;
                IC += 4;
                break;
            case ('F' << 8) | 'O': // FOxy
                SI = 6;
                IC += 4;
                break;
            case ('F' << 8) | 'C': // FCLR
                SI = 7;
                IC += 4;
                break;
            case ('F' << 8) | 'D': // FDLR
                SI = 8;
                IC += 4;
                break;
            case ('I' << 8) | 'N': // INxy
                SI = 4;
                IC += 4;
                break;
            case ('O' << 8) | 'U': // OUxy
                SI = 5;
                IC += 4;
                break;
            case ('H' << 8) | 'A': // HALT
                SI = 3;
                IC += 4;
                break;
            // Extra supervisor mode instructions
            case ('X' << 8) | 'C': { // XCHG - using the channel device
                if(MODE != SUPERVISOR){
                    PI = BAD_INSTRUCTION;
                }
                else{
                    channelDevice.run();
                }
                IC += 4;
                break;
            }
            // Undocumented ones
            case ('R' << 8) | 'S': { // RSTI - reset interrupt registers
                if(MODE != SUPERVISOR){
                    PI = BAD_INSTRUCTION;
                }
                else{
                    PI = 0;
                    SI = 0;
                    TI = R1;
                }
                IC += 4;
                break;
            }
            case ('U' << 8) | 'S': { // USRM - set to user mode
                if(MODE != SUPERVISOR){
                    PI = BAD_INSTRUCTION;
                }
                else{
                    MODE = USER;
                }
                IC += 4;
                break;
            }
            default:
                PI = BAD_INSTRUCTION;
                IC += 4;
                break;
        }

        --TI;
        testInterrupts();
    }

    void run(){
        uint32_t instructionWord = ram.getWord(getPagingAddress(IC));
        uint8_t instruction[4] = {(instructionWord >> 24) & 0xFF, (instructionWord >> 16) & 0xFF, (instructionWord >> 8) & 0xFF, instructionWord & 0xFF};
        executeInstruction(instruction);
    }
};