#pragma once
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhvplatform.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "native_core.hpp"
#include "native_uart.hpp"

namespace nativevm {
inline void require_hr(HRESULT hr,const char* api) {
    if(FAILED(hr)) {
        char msg[150]{};
        std::snprintf(msg,sizeof(msg),"%s error HRESULT=0x%08lx",api,static_cast<unsigned long>(static_cast<uint32_t>(hr)));
        throw std::runtime_error(msg);
    }
}
struct Hypervisor {
    HMODULE dll=nullptr;
    WHV_PARTITION_HANDLE partition=nullptr;
    bool cpu=false;
    uint8_t* memory=nullptr;
    size_t memory_size=0;
    using Cap=HRESULT(WINAPI*)(WHV_CAPABILITY_CODE,VOID*,UINT32,UINT32*);
    using Create=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE*);
    using Config=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE,WHV_PARTITION_PROPERTY_CODE,const VOID*,UINT32);
    using Setup=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE);
    using Map=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE,VOID*,WHV_GUEST_PHYSICAL_ADDRESS,UINT64,WHV_MAP_GPA_RANGE_FLAGS);
    using Cpu=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE,UINT32,UINT32);
    using Run=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE,UINT32,VOID*,UINT32);
    using Cancel=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE,UINT32,UINT32);
    using RegisterSet=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE,UINT32,const WHV_REGISTER_NAME*,UINT32,const WHV_REGISTER_VALUE*);
    using RegisterGet=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE,UINT32,const WHV_REGISTER_NAME*,UINT32,WHV_REGISTER_VALUE*);
    using DeleteCpu=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE,UINT32);
    using Delete=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE);
    Cap capability;Create create;Config config;Setup setup;Map map;Cpu make_cpu;Run run;Cancel cancel;
    RegisterSet set;RegisterGet get;DeleteCpu delete_cpu;Delete destroy;
    template<class T> T proc(const char* name) {
        auto ptr=GetProcAddress(dll,name);
        if(!ptr)throw std::runtime_error(std::string("API WHPX ausente: ")+name);
        return reinterpret_cast<T>(ptr);
    }
    Hypervisor() {
        dll=LoadLibraryExW(L"WinHvPlatform.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!dll)throw std::runtime_error("WHPX no instalado. Habilita Windows Hypervisor Platform");
        try {
            capability=proc<Cap>("WHvGetCapability");create=proc<Create>("WHvCreatePartition");
            config=proc<Config>("WHvSetPartitionProperty");setup=proc<Setup>("WHvSetupPartition");
            map=proc<Map>("WHvMapGpaRange");make_cpu=proc<Cpu>("WHvCreateVirtualProcessor");
            run=proc<Run>("WHvRunVirtualProcessor");cancel=proc<Cancel>("WHvCancelRunVirtualProcessor");
            set=proc<RegisterSet>("WHvSetVirtualProcessorRegisters");
            get=proc<RegisterGet>("WHvGetVirtualProcessorRegisters");
            delete_cpu=proc<DeleteCpu>("WHvDeleteVirtualProcessor");destroy=proc<Delete>("WHvDeletePartition");
        }catch(...) {FreeLibrary(dll);dll=nullptr;throw;}
    }
    ~Hypervisor() {
        if(cpu)delete_cpu(partition,0);
        if(partition)destroy(partition);
        if(memory)VirtualFree(memory,0,MEM_RELEASE);
        if(dll)FreeLibrary(dll);
    }
    Hypervisor(const Hypervisor&)=delete;
    Hypervisor& operator=(const Hypervisor&)=delete;
    bool present() const {
        WINBOOL enabled=FALSE;UINT32 size=0;
        return SUCCEEDED(capability(WHvCapabilityCodeHypervisorPresent,&enabled,sizeof(enabled),&size))
            && enabled && size==sizeof(enabled);
    }
    void initialize(size_t bytes) {
        if(bytes<2*1024*1024 || bytes>1024ull*1024*1024 || (bytes%4096)!=0)
            throw std::runtime_error("RAM virtual fuera de rango (2..1024 MiB, alineada)");
        if(!present())throw std::runtime_error("El hipervisor no esta activo en este Windows");
        require_hr(create(&partition),"WHvCreatePartition");
        UINT32 cores=1;
        require_hr(config(partition,WHvPartitionPropertyCodeProcessorCount,&cores,sizeof(cores)),"WHvSetPartitionProperty");
        require_hr(setup(partition),"WHvSetupPartition");
        memory=reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if(!memory)throw std::runtime_error("VirtualAlloc sin memoria");
        memory_size=bytes;
        require_hr(map(partition,memory,0,bytes,static_cast<WHV_MAP_GPA_RANGE_FLAGS>(
            WHvMapGpaRangeFlagRead|WHvMapGpaRangeFlagWrite|WHvMapGpaRangeFlagExecute)),"WHvMapGpaRange");
        require_hr(make_cpu(partition,0,0),"WHvCreateVirtualProcessor");
        cpu=true;
    }
    void set_reg(WHV_REGISTER_NAME name,uint64_t value) {
        WHV_REGISTER_VALUE data{};data.Reg128.Low64=value;
        require_hr(set(partition,0,&name,1,&data),"WHvSetVirtualProcessorRegisters");
    }
    uint64_t get_reg(WHV_REGISTER_NAME name) {
        WHV_REGISTER_VALUE data{};
        require_hr(get(partition,0,&name,1,&data),"WHvGetVirtualProcessorRegisters");
        return data.Reg128.Low64;
    }
    void set_segment(WHV_REGISTER_NAME name,UINT16 sel,UINT16 flags) {
        WHV_X64_SEGMENT_REGISTER seg{};seg.Base=0;seg.Limit=0xfffff;seg.Selector=sel;seg.Attributes=flags;
        WHV_REGISTER_VALUE data{};static_assert(sizeof(seg)<=sizeof(data));
        std::memcpy(&data,&seg,sizeof(seg));
        require_hr(set(partition,0,&name,1,&data),"WHvSetVirtualProcessorRegisters(segment)");
    }
    void setup_cpu(uint64_t rip,uint64_t stack,uint64_t esi) {
        // 32-bit flat protected mode. GDT en memoria real de la VM.
        constexpr uint64_t desc[4]={0,0,0x00cf9b000000ffffULL,0x00cf93000000ffffULL};
        std::memcpy(memory+0x500,desc,sizeof(desc));
        WHV_X64_TABLE_REGISTER gdt{};gdt.Base=0x500;gdt.Limit=sizeof(desc)-1;
        WHV_REGISTER_VALUE gdtr{};std::memcpy(&gdtr,&gdt,sizeof(gdt));
        auto name=WHvX64RegisterGdtr;
        require_hr(set(partition,0,&name,1,&gdtr),"WHvSetVirtualProcessorRegisters(gdt)");
        set_segment(WHvX64RegisterCs,0x10,0xc09b);
        for(auto reg:{WHvX64RegisterDs,WHvX64RegisterEs,WHvX64RegisterFs,WHvX64RegisterGs,WHvX64RegisterSs})
            set_segment(reg,0x18,0xc093);
        set_reg(WHvX64RegisterCr0,0x11); // PE + ET, sin paginacion
        set_reg(WHvX64RegisterCr3,0);
        set_reg(WHvX64RegisterCr4,0);
        set_reg(WHvX64RegisterEfer,0);
        set_reg(WHvX64RegisterRip,rip);
        set_reg(WHvX64RegisterRsp,stack);
        set_reg(WHvX64RegisterRsi,esi);
        set_reg(WHvX64RegisterRbx,0);
        set_reg(WHvX64RegisterRbp,0);
        set_reg(WHvX64RegisterRdi,0);
        set_reg(WHvX64RegisterRflags,2); // IF deshabilitado
    }
};
inline bool io_port(Hypervisor& vm,const WHV_RUN_VP_EXIT_CONTEXT& e,
                    Serial16550& serial,std::string& console) {
    const auto& io=e.IoPortAccess;
    if(io.AccessInfo.AccessSize!=1 || io.AccessInfo.StringOp || io.AccessInfo.RepPrefix
        || e.VpContext.InstructionLength==0 || !serial.handles(io.PortNumber)) return false;
    if(io.AccessInfo.IsWrite) {
        const size_t old=serial.transmitted.size();
        serial.write(io.PortNumber,static_cast<uint8_t>(io.Rax));
        if(serial.transmitted.size()>old) {
            const char ch=serial.transmitted.back();
            if(console.size()<1024*1024)console.push_back(ch);
            std::cout.put(ch).flush();
        }
    } else {
        const uint8_t value=serial.read(io.PortNumber);
        const auto rax=vm.get_reg(WHvX64RegisterRax);
        vm.set_reg(WHvX64RegisterRax,(rax&~0xffull)|value);
    }
    vm.set_reg(WHvX64RegisterRip,e.VpContext.Rip+e.VpContext.InstructionLength);
    return true;
}
inline int execution_loop(Hypervisor& vm,int timeout_seconds,std::string& output,bool halt_is_success=false) {
    Serial16550 serial;
    std::atomic<bool> stopped{false};
    std::atomic<bool> expired{false};
    std::jthread watchdog([&]{
        const auto end=GetTickCount64()+uint64_t(timeout_seconds)*1000;
        while(!stopped.load()) {
            if(GetTickCount64()>=end) {
                expired=true;
                vm.cancel(vm.partition,0,0);
                return;
            }
            Sleep(25);
        }
    });
    try {
        for(unsigned long n=0;n<5000000;++n) {
            WHV_RUN_VP_EXIT_CONTEXT e{};
            const HRESULT result=vm.run(vm.partition,0,&e,sizeof(e));
            if(FAILED(result) && expired.load()) {
                std::cerr<<"\nTimeout: ejecución WHPX cancelada.\n";
                stopped=true;return 124;
            }
            require_hr(result,"WHvRunVirtualProcessor");
            if(e.ExitReason==WHvRunVpExitReasonX64Halt) {
                if(halt_is_success)std::cout<<"\nPrueba x86 detenida por HLT.\n";
                else std::cerr<<"\nInvitado hizo HLT: falta implementar interrupciones y temporizador virtual.\n";
                stopped=true;
                return halt_is_success?0:9;
            }
            if(e.ExitReason==WHvRunVpExitReasonX64IoPortAccess && io_port(vm,e,serial,output))continue;
            if(e.ExitReason==WHvRunVpExitReasonCanceled && expired.load()) {
                std::cerr<<"\nTiempo agotado: CPU cancelada por watchdog.\n";
                stopped=true;return 124;
            }
            std::cerr<<"\nSalida WHPX sin dispositivo implementado: "<<unsigned(e.ExitReason)
                <<" RIP=0x"<<std::hex<<e.VpContext.Rip
                <<(e.ExitReason==WHvRunVpExitReasonX64IoPortAccess?" puerto=0x":"")
                <<(e.ExitReason==WHvRunVpExitReasonX64IoPortAccess?e.IoPortAccess.PortNumber:0)
                <<std::dec<<"\n";
            stopped=true;return 6;
        }
    }catch(...) {stopped=true;throw;}
    stopped=true;
    return 7;
}
}
