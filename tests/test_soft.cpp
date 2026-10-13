#include "../src/soft_elf.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace softvm;
int main(int argc,char** argv) {
    assert(argc==2);
    std::ifstream file(argv[1],std::ios::binary);
    assert(file.good());
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    auto info=inspect(bytes);
    assert(info.entry==0x401000);
    assert(info.low==0x400000);
    assert(info.segments.size()>=2);
    auto broken=bytes;
    broken[0]=0;
    try {inspect(broken);assert(false);}catch(const std::runtime_error&){}
    broken=bytes;broken[18]=0;broken[19]=0;
    try {inspect(broken);assert(false);}catch(const std::runtime_error&){}
    broken=bytes;broken[4]=1;
    try {inspect(broken);assert(false);}catch(const std::runtime_error&){}
    broken=bytes;
    for(int i=0;i<8;++i)broken[32+i]=0xff;
    try {inspect(broken);assert(false);}catch(const std::runtime_error&){}
    broken=bytes;broken[24]=0;broken[25]=0;broken[26]=0;broken[27]=0;
    try {inspect(broken);assert(false);}catch(const std::runtime_error&){}
    std::cout<<"OK: parseo ELF64, limites, cabecera y punto de entrada\n";
}
