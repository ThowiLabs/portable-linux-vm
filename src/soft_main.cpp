#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <atomic>
#include "soft_elf.hpp"

namespace fs=std::filesystem;
using softvm::ElfInfo;
static uint8_t* guest_base=nullptr;
static size_t guest_size=0;
static DWORD guest_tid=0;
static constexpr size_t max_binary=32*1024*1024;

static std::vector<uint8_t> load_bytes(const fs::path& file) {
    if(!fs::is_regular_file(file))throw std::runtime_error("No existe ELF");
    auto n=fs::file_size(file);
    if(n==0||n>max_binary)throw std::runtime_error("Archivo demasiado grande (maximo 32 MiB)");
    std::vector<uint8_t> bytes(static_cast<size_t>(n));
    std::ifstream input(file,std::ios::binary);
    if(!input.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))
       throw std::runtime_error("Archivo ELF incompleto");
    return bytes;
}
static bool guest_range(uint64_t address,uint64_t length) {
    auto low=reinterpret_cast<uint64_t>(guest_base);
    return guest_base && address>=low && length<=guest_size
           && address-low<=guest_size-length;
}
static bool guest_writable(uint64_t address,uint64_t length) {
    if(!guest_range(address,length))return false;
    if(length==0)return true;
    const uint64_t last=address+length;
    uint64_t at=address;
    while(at<last) {
        MEMORY_BASIC_INFORMATION mbi{};
        if(!VirtualQuery(reinterpret_cast<void*>(at),&mbi,sizeof(mbi)))return false;
        if(mbi.State!=MEM_COMMIT || (mbi.Protect!=PAGE_READWRITE && mbi.Protect!=PAGE_WRITECOPY))
            return false;
        const auto end=reinterpret_cast<uint64_t>(mbi.BaseAddress)+mbi.RegionSize;
        if(end<=at)return false;
        at=std::min(last,end);
    }
    return true;
}
static bool guest_exec(uint64_t rip) {
    return guest_range(rip,2) && *reinterpret_cast<uint8_t*>(rip)==0xcc
        && *reinterpret_cast<uint8_t*>(rip+1)==0x90;
}
static LONG CALLBACK intercept(PEXCEPTION_POINTERS pointers) {
    if(GetCurrentThreadId()!=guest_tid) return EXCEPTION_CONTINUE_SEARCH;
    if(pointers->ExceptionRecord->ExceptionCode!=EXCEPTION_BREAKPOINT)
       return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT& c=*pointers->ContextRecord;
    uint64_t start=c.Rip;
    if(start && guest_exec(start-1))start--;
    else if(!guest_exec(start))return EXCEPTION_CONTINUE_SEARCH;
    // 0f 05 (Linux syscall) fue parcheado a CC 90 (INT3 NOP).
    // Solo unos pocos syscalls de Linux están disponibles todavía.
    switch(c.Rax) {
        case 0: { // read(stdin, buf, count)
            if(c.Rdi!=0){c.Rax=static_cast<uint64_t>(-9LL);break;}
            if(c.Rdx>1024*1024 || !guest_writable(c.Rsi,c.Rdx)) {
                c.Rax=static_cast<uint64_t>(-14LL);break;
            }
            DWORD n=0;
            if(!ReadFile(GetStdHandle(STD_INPUT_HANDLE),reinterpret_cast<void*>(c.Rsi),
                  static_cast<DWORD>(c.Rdx),&n,nullptr))c.Rax=static_cast<uint64_t>(-5LL);
            else c.Rax=n;
            break;
        }
        case 1: { // write(fd, buf, count)
            auto fd=c.Rdi;
            if(fd!=1 && fd!=2) {c.Rax=static_cast<uint64_t>(-9LL);break;}
            if(c.Rdx>1024*1024 || !guest_range(c.Rsi,c.Rdx)) {
                c.Rax=static_cast<uint64_t>(-14LL);break;
            }
            auto handle=GetStdHandle(fd==1?STD_OUTPUT_HANDLE:STD_ERROR_HANDLE);
            DWORD written=0;
            if(!WriteFile(handle,reinterpret_cast<void*>(c.Rsi),DWORD(c.Rdx),&written,nullptr))
                c.Rax=static_cast<uint64_t>(-5LL);
            else c.Rax=written;
            break;
        }
        case 39: c.Rax=1;break; // getpid (ID Linux virtual fijo: prototipo)
        case 60:
        case 231: { // exit / exit_group
            DWORD status=DWORD(c.Rdi)&0xff;
            ExitThread(status);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        default: c.Rax=static_cast<uint64_t>(-38LL);break; // ENOSYS
    }
    c.Rcx=start+2; // syscall real destruye RCX y R11
    c.R11=c.EFlags;
    c.Rip=start+2;
    return EXCEPTION_CONTINUE_EXECUTION;
}
struct Memory {
    uint8_t* ptr=nullptr;
    size_t n=0;
    ~Memory(){if(ptr)VirtualFree(ptr,0,MEM_RELEASE);}
};
static int run(const std::vector<uint8_t>& bytes,const ElfInfo& elf) {
    Memory mem;
    mem.n=static_cast<size_t>(elf.high-elf.low);
    mem.ptr=reinterpret_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(elf.low),
                       mem.n,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!mem.ptr || reinterpret_cast<uint64_t>(mem.ptr)!=elf.low)
        throw std::runtime_error("Windows no permite mapear el ELF en su direccion fija (ET_EXEC)");
    guest_base=mem.ptr;guest_size=mem.n;
    for(const auto& seg:elf.segments) {
        std::memcpy(reinterpret_cast<void*>(seg.vaddr),bytes.data()+seg.offset,seg.file_size);
        // lo restante está inicializado a cero por VirtualAlloc (BSS)
    }
    size_t patched=0;
    for(const auto& seg:elf.segments) {
        if(!(seg.flags&1))continue;
        auto code=reinterpret_cast<uint8_t*>(seg.vaddr);
        for(uint64_t i=0;i+1<seg.file_size;++i) {
            if(code[i]==0x0f && code[i+1]==0x05) {
                code[i]=0xcc;code[i+1]=0x90;++patched;++i;
            }
        }
    }
    if(!patched)throw std::runtime_error("ELF no contiene llamadas Linux (syscall)");
    for(const auto& seg:elf.segments) {
        DWORD old=0;
        auto start=seg.vaddr&~uint64_t(4095);
        auto end=(seg.vaddr+seg.mem_size+4095)&~uint64_t(4095);
        DWORD protect=(seg.flags&1)?PAGE_EXECUTE_READ:((seg.flags&2)?PAGE_READWRITE:PAGE_READONLY);
        if(!VirtualProtect(reinterpret_cast<void*>(start),SIZE_T(end-start),protect,&old))
            throw std::runtime_error("VirtualProtect falló al establecer permisos");
    }
    FlushInstructionCache(GetCurrentProcess(),mem.ptr,mem.n);
    guest_tid=0;
    auto veh=AddVectoredExceptionHandler(1,intercept);
    if(!veh)throw std::runtime_error("No se pudo activar manejador de syscall");
    DWORD tid=0;
    HANDLE thread=CreateThread(nullptr,1024*1024,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(elf.entry),nullptr,CREATE_SUSPENDED,&tid);
    if(!thread){RemoveVectoredExceptionHandler(veh);throw std::runtime_error("No se pudo crear hilo Linux");}
    guest_tid=tid;
    ResumeThread(thread);
    // El programa Linux aún no está aislado: sólo ejecutar binarios propios confiables.
    DWORD wait=WaitForSingleObject(thread,5000);
    if(wait!=WAIT_OBJECT_0) {
        std::cerr<<"Tiempo agotado o fallo de espera; deteniendo prototipo Linux.\n";
        TerminateThread(thread,124);
    }
    DWORD code=125;
    GetExitCodeThread(thread,&code);
    CloseHandle(thread);
    RemoveVectoredExceptionHandler(veh);
    guest_tid=0;guest_base=nullptr;guest_size=0;
    return static_cast<int>(code);
}
static void help() {
    std::cout <<
     "PortableBox 0.1 — ejecutor Linux ELF x86-64 (sin QEMU/Hyper-V/WSL)\n"
     "  portablebox.exe inspect ejemplo-linux\n"
     "  portablebox.exe run ejemplo-linux\n\n"
     "Solo ELF64 ET_EXEC estaticos, sin libc dinamica, con syscalls read/write/exit/getpid.\n"
     "No ejecuta todavia Docker, Debian ni comandos Linux generales.\n"
     "ATENCION: no es un sandbox. Ejecuta solo binarios ELF de confianza.\n";
}
int wmain(int argc,wchar_t** argv) {
    try {
        SetConsoleOutputCP(CP_UTF8);
        if(argc==1||wcscmp(argv[1],L"--help")==0||wcscmp(argv[1],L"help")==0){help();return 0;}
        if(argc!=3)throw std::runtime_error("Uso: portablebox.exe inspect|run archivo.elf");
        auto bytes=load_bytes(fs::path(argv[2]));
        auto info=softvm::inspect(bytes);
        if(wcscmp(argv[1],L"inspect")==0) {
            std::cout<<"ELF64 Linux x86-64 ET_EXEC, entry=0x"<<std::hex<<info.entry
                     <<", mapa [0x"<<info.low<<",0x"<<info.high<<")"<<std::dec
                     <<", segmentos="<<info.segments.size()<<"\n";return 0;
        }
        if(wcscmp(argv[1],L"run")!=0)throw std::runtime_error("Comando desconocido");
        return run(bytes,info);
    }catch(const std::exception& e) {
        std::cerr<<"ERROR: "<<e.what()<<"\n";return 2;
    }
}
