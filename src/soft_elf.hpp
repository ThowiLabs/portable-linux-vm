#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <string>
namespace softvm {
struct Segment {
    uint64_t vaddr=0,offset=0,file_size=0,mem_size=0;
    uint32_t flags=0;
};
struct ElfInfo {
    uint64_t entry=0,low=0,high=0;
    std::vector<Segment> segments;
};
inline uint16_t u16(const std::vector<uint8_t>& x,size_t i) {
    if(i>x.size() || x.size()-i<2)throw std::runtime_error("ELF truncado");
    return uint16_t(x[i])|(uint16_t(x[i+1])<<8);
}
inline uint32_t u32(const std::vector<uint8_t>& x,size_t i) {
    if(i>x.size() || x.size()-i<4)throw std::runtime_error("ELF truncado");
    uint32_t out=0;for(int j=0;j<4;++j)out|=uint32_t(x[i+j])<<(8*j);
    return out;
}
inline uint64_t u64(const std::vector<uint8_t>& x,size_t i) {
    if(i>x.size() || x.size()-i<8)throw std::runtime_error("ELF truncado");
    uint64_t out=0;for(int j=0;j<8;++j)out|=uint64_t(x[i+j])<<(8*j);
    return out;
}
inline ElfInfo inspect(const std::vector<uint8_t>& bytes) {
    if(bytes.size()<64 || bytes[0]!=0x7f || bytes[1]!='E' || bytes[2]!='L' || bytes[3]!='F')
        throw std::runtime_error("No es ELF");
    if(bytes[4]!=2 || bytes[5]!=1 || bytes[6]!=1)throw std::runtime_error("Solo ELF64 little-endian");
    if(u16(bytes,16)!=2 || u16(bytes,18)!=62)throw std::runtime_error("Solo ELF64 ET_EXEC x86-64");
    if(u32(bytes,20)!=1)throw std::runtime_error("Version ELF invalida");
    const auto phoff=u64(bytes,32); const auto phsize=u16(bytes,54); const auto phcount=u16(bytes,56);
    if(phsize<56 || phcount<1 || phcount>32)throw std::runtime_error("Tabla de segmentos invalida");
    if(phoff>bytes.size() || uint64_t(phcount)*phsize>bytes.size()-phoff)
        throw std::runtime_error("Encabezados fuera de archivo");
    ElfInfo elf;elf.entry=u64(bytes,24);elf.low=UINT64_MAX;
    for(unsigned n=0;n<phcount;++n) {
        const auto p=phoff+uint64_t(n)*phsize;
        const auto type=u32(bytes,static_cast<size_t>(p));
        if(type==3 || type==7)throw std::runtime_error("ELF dinamico/TLS no soportado");
        if(type!=1)continue;
        Segment seg{u64(bytes,p+16),u64(bytes,p+8),u64(bytes,p+32),u64(bytes,p+40),u32(bytes,p+4)};
        if(!seg.mem_size || seg.mem_size<seg.file_size)throw std::runtime_error("Segmento ELF invalido");
        if(seg.offset>bytes.size() || seg.file_size>bytes.size()-seg.offset)
            throw std::runtime_error("Segmento ELF truncado");
        if(seg.vaddr<0x10000 || seg.vaddr>0x7fff00000000ULL || seg.mem_size>256*1024*1024ULL
          || seg.mem_size>UINT64_MAX-seg.vaddr)throw std::runtime_error("Segmento ELF fuera de rango");
        if((seg.vaddr & 4095)!=(seg.offset & 4095))
            throw std::runtime_error("Alineacion ELF invalida");
        if((seg.flags&3)==3)throw std::runtime_error("Pagina ELF writable-executable no permitida");
        elf.low=std::min(elf.low,seg.vaddr&~uint64_t(4095));
        elf.high=std::max(elf.high,(seg.vaddr+seg.mem_size+4095)&~uint64_t(4095));
        elf.segments.push_back(seg);
    }
    if(elf.segments.empty() || elf.high-elf.low>256*1024*1024ULL)
        throw std::runtime_error("ELF sin segmentos o demasiado grande");
    bool executable=false;
    for(const auto& seg:elf.segments)
        if((seg.flags&1) && elf.entry>=seg.vaddr && elf.entry-seg.vaddr<seg.file_size)executable=true;
    if(!executable)throw std::runtime_error("Entry ELF fuera de segmento ejecutable");
    for(size_t a=0;a<elf.segments.size();++a)
      for(size_t b=a+1;b<elf.segments.size();++b) {
        const auto& x=elf.segments[a];const auto& y=elf.segments[b];
        if(x.vaddr<y.vaddr+y.mem_size && y.vaddr<x.vaddr+x.mem_size)
            throw std::runtime_error("Segmentos ELF superpuestos");
      }
    return elf;
}
}
