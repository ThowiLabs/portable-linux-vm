#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs=std::filesystem;
using std::wstring;

static wstring quote(const wstring& s) {
    wstring out=L"\"";unsigned back=0;
    for(wchar_t ch:s) {
        if(ch==L'\\'){++back;continue;}
        if(ch==L'"'){out.append(back*2+1,L'\\');out+=L'"';back=0;continue;}
        out.append(back,L'\\');back=0;out+=ch;
    }
    out.append(back*2,L'\\');out+=L'"';return out;
}
static fs::path home() {
    std::vector<wchar_t> path(32768);
    auto size=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));
    if(!size||size==path.size())throw std::runtime_error("No se encuentra la carpeta del programa.");
    return fs::path(wstring(path.data(),size)).parent_path();
}
static int launch(const fs::path& tool,const std::vector<wstring>& args,const fs::path& cwd,DWORD max_wait=INFINITE) {
    if(!fs::is_regular_file(tool))throw std::runtime_error("Falta motor blink.exe en el paquete.");
    wstring cmd=quote(tool.wstring());
    for(const auto& a:args)cmd+=L" "+quote(a);
    STARTUPINFOW si{};si.cb=sizeof(si);
    PROCESS_INFORMATION pi{};
    auto directory=cwd.wstring();
    if(!CreateProcessW(tool.c_str(),cmd.data(),nullptr,nullptr,TRUE,0,nullptr,
        directory.c_str(),&si,&pi)) {
        auto err=GetLastError();
        throw std::runtime_error("No se pudo iniciar el motor Linux (Windows "+std::to_string(err)+").");
    }
    CloseHandle(pi.hThread);
    DWORD wait=WaitForSingleObject(pi.hProcess,max_wait);
    if(wait==WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess,124);
        CloseHandle(pi.hProcess);
        throw std::runtime_error("El motor no respondio dentro del tiempo esperado.");
    }
    DWORD code=2;
    if(wait==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&code);
    CloseHandle(pi.hProcess);
    return int(code);
}
static void help(){
    std::wcout << L"PortableBox 1.0.1 - Linux en modo usuario, sin Hyper-V, WSL ni QEMU\n"
      L"Uso:\n"
      L"  portablebox.exe images\n"
      L"  portablebox.exe doctor\n"
      L"  portablebox.exe run debian:12\n"
      L"  portablebox.exe run alpine:3.24\n"
      L"  portablebox.exe run debian:12 -- /bin/bash -c \"cat /etc/debian_version\"\n"
      L"\nNo es Docker Engine: sin aislamiento, red y procesos compatibles parcialmente.\n"
      L"No ejecutar programas Linux desconocidos o maliciosos.\n";
}
int wmain(int argc,wchar_t** argv) {
    try {
        SetConsoleOutputCP(CP_UTF8);
        if(argc<2 || !wcscmp(argv[1],L"--help") || !wcscmp(argv[1],L"help")) {
            help();return 0;
        }
        auto folder=home();
        auto engine=folder/L"blink.exe";
        const wstring action=argv[1];
        if(action==L"images" || action==L"list") {
            for(const auto& x:{L"debian:12",L"alpine:3.24"}) {
                auto name=wstring(x)==L"debian:12"?L"debian-12":L"alpine-3.24";
                auto path=folder/L"distros"/name;
                std::wcout<<x<<L"  "<<(fs::is_regular_file(path/L"etc"/(x==wstring(L"debian:12")?L"debian_version":L"alpine-release"))?L"lista":L"ausente")<<L"\n";
            }
            return 0;
        }
        if(action==L"doctor") {
            std::wcout<<L"Motor Linux modo usuario (Blink 1.1 ISC); sin Hyper-V.\n";
            return launch(engine,{L"-v"},folder,15000);
        }
        if(action==L"run" || action==L"exec") {
            if(argc<3)throw std::runtime_error("Falta imagen: debian:12 o alpine:3.24.");
            const wstring image=argv[2];
            fs::path root;
            wstring executable;
            if(image==L"debian:12") {root=folder/L"distros"/L"debian-12";executable=L"/bin/bash";}
            else if(image==L"alpine:3.24") {root=folder/L"distros"/L"alpine-3.24";executable=L"/bin/sh";}
            else throw std::runtime_error("Imagen no incluida. Usa portablebox.exe images.");
            if(!fs::is_directory(root) || !fs::is_regular_file(root/L"etc"/L"os-release"))
                throw std::runtime_error("Falta el sistema Linux de la imagen. Extrae el ZIP completo.");
            if(argc>3) {
                if(wcscmp(argv[3],L"--"))throw std::runtime_error("Los argumentos comienzan con --");
                if(argc<5)throw std::runtime_error("Especifica el programa Linux despues de --");
                executable=argv[4];
                if(executable.empty() || executable[0]!=L'/')
                    throw std::runtime_error("El ejecutable invitado debe ser ruta Linux absoluta.");
            }
            // Blink 1.1 trata ':' de -C como separador de overlays.
            // Las rutas Windows C:\... por lo tanto ROMPEN el montaje.
            // CreateProcessW fija cwd=folder, usar una ruta relativa con '/'.
            const fs::path relative = root.lexically_relative(folder);
            if(relative.empty() || relative.is_absolute() || relative.wstring().find(L':')!=wstring::npos)
                throw std::runtime_error("No se pudo construir ruta portable del rootfs.");
            std::vector<wstring> options={L"-C",relative.generic_wstring(),executable};
            for(int i=5;i<argc;i++)options.emplace_back(argv[i]);
            return launch(engine,options,folder);
        }
        throw std::runtime_error("Comando desconocido; usa --help.");
    }catch(const std::exception& e){
        std::cerr<<"ERROR: "<<e.what()<<"\n";
        return 2;
    }
}
