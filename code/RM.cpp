#include <cstdint>
using namespace std;

class RAM{
    private:
    uint8_t memory[11264]; // Supervisor 1024 + user 10240, maybe needs to be separated

    public:

    uint8_t getByte(uint16_t address){
    }

    void setByte(uint16_t address, uint8_t byte){
    }

    uint32_t getWord(uint16_t address){
    }

    void setWord(uint16_t address, uint32_t word){
    }
};

class ChannelDevice{
};

class HardDrive{
};

class Keyboard{
};

class Screen{
};

class CPU{
    private:
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
    RAM &ram; // Should be replaced by the channel device

    public:
    CPU(RAM &ram) : ram(ram){
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
    void testInterrupts(){}

    // Executing instructions
    void executeInstruction(uint8_t instruction[4]){
        switch((instruction[0] << 8) | instruction[1]){ // 2 bytes of each instruction are unique
            case ('L' << 8) | 'D': { // LDxy
                uint16_t address = (instruction[2] >= 'A' ? instruction[2] - 'A' + 10 : instruction[2] - '0') * 16 
                                    + (instruction[3] >= 'A' ? instruction[3] - 'A' + 10 : instruction[3] - '0');
                R1 = ram.getWord(getPagingAddress(address));
                break;
            }
            case ('S' << 8) | 'W' : { // SWAP
                uint32_t temp = R1;
                R1 = R2;
                R2 = temp;
                break;
            }
            case ('S' << 8) | 'T' : { // STxy
                uint16_t address = (instruction[2] >= 'A' ? instruction[2] - 'A' + 10 : instruction[2] - '0') * 16 
                                    + (instruction[3] >= 'A' ? instruction[3] - 'A' + 10 : instruction[3] - '0');
                ram.setWord(getPagingAddress(address), R1);
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
                break;
            }
            case ('D' << 8) | 'I': { // DIVR
                clearFlags();

                if(R2 == 0){
                    PI = DIVISION_BY_0;
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
                break;
            }
            case ('C' << 8) | 'O': // COMP
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
                break;
            case ('J' << 8) | 'P': // JPxy
                break;
            case ('J' << 8) | '1': // J1xy
                break;
            case ('J' << 8) | '0': // J0xy
                break;
            case ('J' << 8) | 'A': // JAxy
                break;
            case ('J' << 8) | 'B': // JBxy
                break;
            case ('J' << 8) | 'C': // JCxy
                break;
            case ('J' << 8) | 'O': // JOxy
                break;
            case ('P' << 8) | 'D': // PDxy
                break;
            case ('G' << 8) | 'D': // GDxy
                break;
            case ('F' << 8) | 'M': // FMOV
                break;
            case ('F' << 8) | 'O': // FOxy
                break;
            case ('F' << 8) | 'C': // FCLR
                break;
            case ('F' << 8) | 'D': // FDLR
                break;
            case ('I' << 8) | 'N': // INxy
                break;
            case ('O' << 8) | 'U': // OUxy
                break;
            case ('H' << 8) | 'L': // HALT
                break;
            default:
                PI = BAD_INSTRUCTION;
                break;
        }

        --TI;
        testInterrupts();
        IC += 4;
    }
};