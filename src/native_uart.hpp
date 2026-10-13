#pragma once
#include <cstdint>
#include <deque>
#include <string>

// UART 16550 básico en COM1 (0x3F8). La implementación NO contiene PIC/IRQ.
// Suficiente para salida por sondeo y diagnóstico de la primera fase.
namespace nativevm {
class Serial16550 {
public:
    uint8_t dll=1, dlm=0, ier=0, lcr=0, mcr=0, scr=0;
    std::deque<uint8_t> rx;
    std::string transmitted;
    static constexpr uint16_t base=0x3f8;

    bool handles(uint16_t port) const { return port>=base && port<base+8; }
    void receive(uint8_t value) {
        if(rx.size()<16)rx.push_back(value);
    }
    uint8_t read(uint16_t port) {
        const unsigned reg=port-base;
        const bool dlab=(lcr&0x80)!=0;
        switch(reg) {
            case 0:
                if(dlab)return dll;
                if(rx.empty())return 0;
                {auto value=rx.front();rx.pop_front();return value;}
            case 1:return dlab?dlm:ier;
            case 2:return 0x01; // Interrupt Identification Register: ninguno pendiente
            case 3:return lcr;
            case 4:return mcr;
            case 5:return static_cast<uint8_t>(0x60|(rx.empty()?0:1)); // THR/TEMT + data ready
            case 6:return 0xb0; // CTS, DSR, DCD
            case 7:return scr;
            default:return 0xff;
        }
    }
    void write(uint16_t port,uint8_t value) {
        const unsigned reg=port-base;
        const bool dlab=(lcr&0x80)!=0;
        switch(reg) {
            case 0:
                if(dlab)dll=value;
                else if(transmitted.size()<1024*1024)transmitted.push_back(char(value));
                break;
            case 1:if(dlab)dlm=value;else ier=value&0x0f;break;
            case 2:if(value&0x02)rx.clear();break; // limpiar FIFO RX
            case 3:lcr=value;break;
            case 4:mcr=value;break;
            case 7:scr=value;break;
            default:break;
        }
    }
};
}
