#include "../src/native_uart.hpp"
#include <cassert>
#include <iostream>
using namespace nativevm;
int main(){
    Serial16550 uart;
    assert(uart.handles(0x3f8)&&uart.handles(0x3ff)&&!uart.handles(0x2f8));
    assert(uart.read(0x3fd)==0x60); // ready to transmit
    uart.write(0x3fb,0x80); // DLAB on
    uart.write(0x3f8,0x0c); // baud divisor 12: NOT a character
    uart.write(0x3f9,0x00);
    assert(uart.transmitted.empty());
    assert(uart.read(0x3f8)==12);
    uart.write(0x3fb,0x03); // 8 bits, DLAB off
    uart.write(0x3f8,'O');
    uart.write(0x3f8,'K');
    assert(uart.transmitted=="OK");
    uart.receive('z');
    assert((uart.read(0x3fd)&1)!=0);
    assert(uart.read(0x3f8)=='z');
    assert((uart.read(0x3fd)&1)==0);
    uart.receive('a');
    uart.write(0x3fa,2); // clear FIFO
    assert((uart.read(0x3fd)&1)==0);
    uart.write(0x3f9,15);
    assert(uart.read(0x3f9)==15);
    uart.write(0x3ff,0x5a);
    assert(uart.read(0x3ff)==0x5a);
    std::cout<<"OK: UART 16550 DLAB, TX, RX, line status, FIFO\n";
}
