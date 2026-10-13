#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace nativevm {
namespace fs=std::filesystem;
inline std::vector<uint8_t> read_file(const fs::path& name, size_t limit) {
    if(!fs::is_regular_file(name))throw std::runtime_error("Archivo no encontrado");
    auto length=fs::file_size(name);
    if(!length || length>limit)throw std::runtime_error("Archivo vacio o mayor que limite");
    std::ifstream file(name,std::ios::binary);
    if(!file)throw std::runtime_error("No se pudo abrir archivo");
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    if(!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Lectura de archivo incompleta");
    return bytes;
}
inline uint16_t le16(const std::vector<uint8_t>& data, size_t off) {
    if(off+2>data.size())throw std::runtime_error("Archivo truncado");
    return uint16_t(data[off]) | (uint16_t(data[off+1])<<8);
}
inline uint32_t le32(const std::vector<uint8_t>& data, size_t off) {
    if(off+4>data.size())throw std::runtime_error("Archivo truncado");
    return uint32_t(data[off]) | (uint32_t(data[off+1])<<8) | (uint32_t(data[off+2])<<16) | (uint32_t(data[off+3])<<24);
}
inline void put32(uint8_t* data,uint32_t v) {
    for(int i=0;i<4;++i)data[i]=uint8_t(v>>(i*8));
}
inline void put64(uint8_t* data,uint64_t v) {
    for(int i=0;i<8;++i)data[i]=uint8_t(v>>(i*8));
}
struct KernelInfo {
    uint16_t protocol;
    uint32_t protected_offset;
    uint32_t protected_size;
    uint32_t initrd_addr_max;
};
inline KernelInfo inspect_kernel(const std::vector<uint8_t>& kernel) {
    if(kernel.size()<4096 || le32(kernel,0x202)!=0x53726448u)
        throw std::runtime_error("No es un bzImage x86 Linux (falta HdrS)");
    auto protocol=le16(kernel,0x206);
    if(protocol<0x0206)throw std::runtime_error("Kernel demasiado antiguo: requiere boot protocol >= 2.06");
    if(!(kernel[0x211]&1u))throw std::runtime_error("bzImage sin LOAD_HIGH, no compatible con esta version");
    uint32_t setup=kernel[0x1f1];
    if(!setup)setup=4;
    uint32_t offset=(setup+1)*512;
    if(offset>=kernel.size())throw std::runtime_error("Kernel truncado sin codigo protegido");
    uint32_t max=protocol>=0x0203?le32(kernel,0x22c):0x37ffffffu;
    if(!max)max=0x37ffffffu;
    return {protocol,offset,static_cast<uint32_t>(kernel.size()-offset),max};
}
inline void check_region(uint64_t ram,uint64_t addr,uint64_t size) {
    if(addr>ram || size>ram-addr)throw std::runtime_error("Imagen excede la RAM virtual");
}
inline void setup_boot_params(uint8_t* ram,size_t ram_size,
    const std::vector<uint8_t>& kernel,const KernelInfo& info,
    const std::vector<uint8_t>& initramfs,const std::string& cmd) {
    constexpr size_t bp=0x90000, entry=0x100000, line=0x20000, table=0x2d0;
    check_region(ram_size,bp,4096);
    check_region(ram_size,entry,info.protected_size);
    if(cmd.size()>2047)throw std::runtime_error("Linea de arranque demasiado larga");
    if(cmd.find('\0')!=std::string::npos)throw std::runtime_error("Linea de arranque contiene NUL");
    uint64_t initrd=0;
    if(!initramfs.empty()) {
        // Dejar 16 MiB libres alrededor del kernel y alinear initramfs a 4 KiB.
        initrd=((uint64_t(entry)+info.protected_size+16*1024*1024+4095)/4096)*4096;
        if(initrd+initramfs.size()>info.initrd_addr_max)throw std::runtime_error("initramfs fuera del maximo del kernel");
        check_region(ram_size,initrd,initramfs.size());
    }
    std::memset(ram+bp,0,4096);
    size_t hdr_size=0x202+kernel[0x201];
    if(hdr_size>kernel.size() || hdr_size>4096)
        throw std::runtime_error("Cabecera setup demasiado grande");
    std::memcpy(ram+bp+0x1f1,kernel.data()+0x1f1,hdr_size-0x1f1);
    ram[bp+0x210]=0xff; // type_of_loader: desconocido
    ram[bp+0x211]|=0x80; // CAN_USE_HEAP
    ram[bp+0x224]=0xff;ram[bp+0x225]=0x8f; // heap_end_ptr
    put32(ram+bp+0x228,static_cast<uint32_t>(line));
    if(!initramfs.empty()) {
        put32(ram+bp+0x218,static_cast<uint32_t>(initrd));
        put32(ram+bp+0x21c,static_cast<uint32_t>(initramfs.size()));
        std::memcpy(ram+initrd,initramfs.data(),initramfs.size());
    }
    // E820 entries: RAM bajo 640 KiB y memoria de 1 MiB hasta ram_size.
    ram[bp+0x1e8]=2;
    put64(ram+bp+table,0); put64(ram+bp+table+8,0x9fc00); put32(ram+bp+table+16,1);
    put64(ram+bp+table+20,0x100000);
    put64(ram+bp+table+28,static_cast<uint64_t>(ram_size)-0x100000);
    put32(ram+bp+table+36,1);
    std::memcpy(ram+line,cmd.c_str(),cmd.size()+1);
    std::memcpy(ram+entry,kernel.data()+info.protected_offset,info.protected_size);
}
}
