#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhvplatform.h>
#include <iostream>
#include <cstdint>

// Prueba independiente de WHPX: crea una particion y una CPU virtual.
// No implementa BIOS, dispositivos, discos, red ni arranque de un kernel.
int wmain() {
    auto dll=LoadLibraryExW(L"WinHvPlatform.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!dll) {std::wcerr<<L"WHPX no disponible en este equipo.\n";return 2;}
    using GetCap=HRESULT(WINAPI*)(WHV_CAPABILITY_CODE,VOID*,UINT32,UINT32*);
    using Create=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE*);
    using SetProp=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE,WHV_PARTITION_PROPERTY_CODE,const VOID*,UINT32);
    using Setup=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE);
    using CreateCpu=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE,UINT32,UINT32);
    using DeleteCpu=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE,UINT32);
    using Delete=HRESULT(WINAPI*)(WHV_PARTITION_HANDLE);
    auto fn=[&](const char *name)->FARPROC {return GetProcAddress(dll,name);};
    const auto cap=reinterpret_cast<GetCap>(fn("WHvGetCapability"));
    const auto create=reinterpret_cast<Create>(fn("WHvCreatePartition"));
    const auto prop=reinterpret_cast<SetProp>(fn("WHvSetPartitionProperty"));
    const auto setup=reinterpret_cast<Setup>(fn("WHvSetupPartition"));
    const auto cpu=reinterpret_cast<CreateCpu>(fn("WHvCreateVirtualProcessor"));
    const auto deleteCpu=reinterpret_cast<DeleteCpu>(fn("WHvDeleteVirtualProcessor"));
    const auto destroy=reinterpret_cast<Delete>(fn("WHvDeletePartition"));
    if(!cap||!create||!prop||!setup||!cpu||!deleteCpu||!destroy) {
        std::wcerr<<L"Version de WHPX incompleta.\n";FreeLibrary(dll);return 3;
    }
    UINT32 present=0,written=0;
    if(FAILED(cap(WHvCapabilityCodeHypervisorPresent,&present,sizeof(present),&written)) || !present) {
        std::wcerr<<L"El hipervisor no esta activo. Habilita Windows Hypervisor Platform.\n";
        FreeLibrary(dll);return 4;
    }
    WHV_PARTITION_HANDLE partition=nullptr;
    HRESULT hr=create(&partition);
    if(FAILED(hr)) {std::wcerr<<L"No fue posible crear la particion WHPX.\n";FreeLibrary(dll);return 5;}
    const UINT32 cores=1;
    hr=prop(partition,WHvPartitionPropertyCodeProcessorCount,&cores,sizeof(cores));
    if(SUCCEEDED(hr))hr=setup(partition);
    bool madeCpu=false;
    if(SUCCEEDED(hr)) {hr=cpu(partition,0,0);madeCpu=SUCCEEDED(hr);}
    if(madeCpu)deleteCpu(partition,0);
    destroy(partition);
    FreeLibrary(dll);
    if(FAILED(hr)) {std::wcerr<<L"Error al configurar CPU WHPX. HRESULT "<<std::hex<<static_cast<uint32_t>(hr)<<L"\n";return 6;}
    std::wcout<<L"OK: particion y procesador virtual creados y liberados mediante WHPX nativo.\n"
         <<L"Esta prueba NO inicia sistemas operativos. El gestor portable usa QEMU-WHPX.\n";
    return 0;
}
