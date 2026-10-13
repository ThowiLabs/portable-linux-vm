#pragma once
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace plvm {
namespace fs = std::filesystem;
struct VM {
    int ram = 2048;
    int cpus = 2;
};
inline bool valid_name(const std::wstring& name) {
    if(name.empty() || name.size() > 48 || !std::iswalnum(name[0]) || name[0] > 127) return false;
    for(auto c : name) if(c > 127 || !((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'_'||c==L'-')) return false;
    std::wstring upper = name;
    std::transform(upper.begin(),upper.end(),upper.begin(),::towupper);
    for(auto bad : {L"CON",L"PRN",L"AUX",L"NUL",L"COM1",L"COM2",L"LPT1",L"LPT2"})
        if(upper == bad) return false;
    return true;
}
inline int number(const std::wstring& input, int low, int high, const char* label) {
    if(input.empty()) throw std::runtime_error(std::string(label)+": numero vacio");
    for(wchar_t c : input) if(c < L'0' || c > L'9') throw std::runtime_error(std::string(label)+": solo digitos");
    unsigned long n;
    try { n=std::stoul(input); } catch(...) { throw std::runtime_error(std::string(label)+": fuera de rango"); }
    if(n < static_cast<unsigned long>(low) || n > static_cast<unsigned long>(high))
        throw std::runtime_error(std::string(label)+": fuera de rango");
    return static_cast<int>(n);
}
inline std::wstring disk_size(const std::wstring& input) {
    if(input.size()<2 || (input.back()!=L'G' && input.back()!=L'M')) throw std::runtime_error("Tamano: usa N[G|M]");
    auto n=number(input.substr(0,input.size()-1), 1, input.back()==L'G'?1024:1048576, "Tamano");
    if((input.back()==L'G' && n<1) || (input.back()==L'M' && n<512)) throw std::runtime_error("Disco minimo 512M");
    return std::to_wstring(n)+input.back();
}
inline void save_vm(const fs::path& file, VM v) {
    auto pending=file; pending+=L".tmp";
    {std::ofstream out(pending, std::ios::binary|std::ios::trunc);
    if(!out) throw std::runtime_error("No se puede escribir configuracion");
    out << "version=1\nram=" << v.ram << "\ncpus=" << v.cpus << "\n";
    if(!out.good()) throw std::runtime_error("Error de escritura configuracion");}
    fs::rename(pending,file);
}
inline VM read_vm(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if(!in) throw std::runtime_error("No existe vm.cfg");
    std::string line;
    VM v{};
    bool version=false, ram=false, cpus=false;
    while(std::getline(in,line)) {
        auto equal=line.find('=');
        if(equal==std::string::npos) throw std::runtime_error("Archivo vm.cfg invalido");
        auto key=line.substr(0,equal),val=line.substr(equal+1);
        if(key=="version" && val=="1" && !version) version=true;
        else if(key=="ram" && !ram) {v.ram=number(std::wstring(val.begin(),val.end()),256,65536,"RAM");ram=true;}
        else if(key=="cpus" && !cpus) {v.cpus=number(std::wstring(val.begin(),val.end()),1,32,"CPU");cpus=true;}
        else throw std::runtime_error("Clave duplicada/desconocida de vm.cfg");
    }
    if(!version || !ram || !cpus) throw std::runtime_error("vm.cfg incompleto");
    return v;
}
inline std::wstring quote_arg(const std::wstring& text) {
    std::wstring s=L"\""; int back=0;
    for(wchar_t c : text) {
        if(c==L'\\') {++back; continue;}
        if(c==L'"') {s.append(back*2+1,L'\\');s+=L'"';back=0;continue;}
        s.append(back,L'\\');back=0;s+=c;
    }
    s.append(back*2,L'\\');s+=L'"';
    return s;
}
inline std::wstring drive_arg(const fs::path& disk) {
    auto s=disk.wstring();std::wstring escaped;
    for(wchar_t c:s){escaped+=c;if(c==L',')escaped+=L',';}
    return L"file="+escaped+L",if=virtio,format=qcow2";
}
inline bool is_qcow2(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);
    char header[4]{};
    if(!in.read(header,4)) return false;
    return static_cast<unsigned char>(header[0])==0x51 && static_cast<unsigned char>(header[1])==0x46
        && static_cast<unsigned char>(header[2])==0x49 && static_cast<unsigned char>(header[3])==0xfb;
}
inline bool has_backing_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    unsigned char header[20]{};
    if(!in.read(reinterpret_cast<char*>(header),sizeof(header)))
        throw std::runtime_error("Cabecera QCOW2 incompleta");
    uint64_t offset=0;
    for(int i=8;i<16;++i)offset=(offset<<8)|header[i];
    uint32_t length=0;
    for(int i=16;i<20;++i)length=(length<<8)|header[i];
    return offset!=0 || length!=0;
}
inline bool is_iso(const fs::path& path) {
    auto ext=path.extension().wstring();
    std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
    return ext==L".iso" && fs::is_regular_file(path);
}
}
