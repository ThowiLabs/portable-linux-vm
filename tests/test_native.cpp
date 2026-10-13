#include "../src/native_core.hpp"
#include <cassert>
#include <iostream>
#include <vector>
using namespace nativevm;
static void set_le16(std::vector<uint8_t>& bytes,size_t o,uint16_t v) {
    bytes[o]=uint8_t(v);bytes[o+1]=uint8_t(v>>8);
}
static void set_le32(std::vector<uint8_t>& bytes,size_t o,uint32_t v) {
    for(int n=0;n<4;++n)bytes[o+n]=uint8_t(v>>(8*n));
}
int main(int argc,char** argv) {
    std::vector<uint8_t> fake(8192,0xa5);
    set_le32(fake,0x202,0x53726448);
    set_le16(fake,0x206,0x020f);
    fake[0x1f1]=4;
    fake[0x201]=0x6b;
    fake[0x211]=1;
    set_le32(fake,0x22c,0x37ffffff);
    const auto header=inspect_kernel(fake);
    assert(header.protocol==0x020f);
    assert(header.protected_offset==2560);
    assert(header.protected_size==5632);
    std::vector<uint8_t> ram(64*1024*1024);
    std::vector<uint8_t> initrd(128*1024,0x42);
    setup_boot_params(ram.data(),ram.size(),fake,header,initrd,"console=ttyS0");
    auto bp=ram.data()+0x90000;
    assert(le32(std::vector<uint8_t>(bp,bp+4096),0x202)==0x53726448);
    assert(bp[0x1e8]==2);
    assert(le32(std::vector<uint8_t>(bp,bp+4096),0x228)==0x20000);
    const uint32_t image=le32(std::vector<uint8_t>(bp,bp+4096),0x218);
    assert(image>0x100000+5632);
    assert(ram[image]==0x42);
    assert(ram[0x100000]==0xa5);
    auto bad=fake;bad[0x202]=0;
    try {inspect_kernel(bad);assert(false);}catch(const std::runtime_error&){}
    try {setup_boot_params(ram.data(),2*1024*1024,fake,header,initrd,"test");assert(false);}catch(const std::runtime_error&){}
    if(argc==2) {
        const auto alpine=read_file(argv[1],256*1024*1024);
        auto real=inspect_kernel(alpine);
        std::cout<<"Alpine real: protocol 0x"<<std::hex<<real.protocol<<std::dec
                 <<", protected bytes "<<real.protected_size<<"\n";
        assert(real.protected_size>1000000);
    }
    std::cout<<"OK: cabecera bzImage, regiones RAM, E820, initramfs, limites\n";
}
