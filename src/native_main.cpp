#include "native_whpx.hpp"
#include <cwchar>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace nativevm;
static void help() {
    std::cout <<
        "PortableVM Native 0.2 - WHPX sin QEMU\n"
        "  nativevm.exe doctor\n"
        "  nativevm.exe smoke\n"
        "  nativevm.exe list\n"
        "  nativevm.exe validate NOMBRE [--ram 256]\n"
        "  nativevm.exe add NOMBRE --kernel RUTA --initrd RUTA\n"
        "  nativevm.exe run NOMBRE [--ram 256] [--timeout 15]\n"
        "  nativevm.exe inspect distros\\alpine\\vmlinuz\n"
        "  nativevm.exe boot --kernel distros\\alpine\\vmlinuz --initrd distros\\alpine\\initramfs"
        " [--ram 256] [--timeout 15]\n"
        "Motor nativo: sin BIOS, ISO, QCOW2 ni dispositivos VirtIO aun.\n";
}
static std::wstring arg(std::vector<std::wstring>& a,size_t& i) {
    if(i+1>=a.size())throw std::runtime_error("Falta valor de opcion");
    return a[++i];
}
static size_t decimal(const std::wstring& s,size_t low,size_t high) {
    if(s.empty())throw std::runtime_error("Numero vacio");
    uint64_t val=0;
    for(auto c:s) {
        if(c<L'0'||c>L'9')throw std::runtime_error("Numero debe tener solo digitos");
        val=val*10+(c-L'0');
        if(val>high)throw std::runtime_error("Numero demasiado grande");
    }
    if(val<low)throw std::runtime_error("Numero demasiado pequeno");
    return static_cast<size_t>(val);
}
static fs::path home() {
    std::wstring exe(32768,L'\0');
    DWORD n=GetModuleFileNameW(nullptr,exe.data(),DWORD(exe.size()));
    if(!n || n>=exe.size())throw std::runtime_error("No se pudo leer ruta del ejecutable");
    exe.resize(n);
    return fs::path(exe).parent_path();
}
static fs::path profile(const std::wstring& name) {
    if(name.empty()||name.size()>48)throw std::runtime_error("Nombre de distro invalido");
    for(auto c:name) if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'-'||c==L'_'))
        throw std::runtime_error("Nombre de distro: solo ASCII, guion y guion bajo");
    std::wstring upper=name;
    std::transform(upper.begin(),upper.end(),upper.begin(),::towupper);
    for(auto forbidden:{L"CON",L"PRN",L"AUX",L"NUL",L"COM1",L"COM2",L"LPT1",L"LPT2"})
        if(upper==forbidden)throw std::runtime_error("Nombre reservado por Windows");
    return home()/L"distros"/name;
}
int wmain(int argc,wchar_t** argv) {
    try {
        SetConsoleOutputCP(CP_UTF8);
        std::vector<std::wstring> args(argv,argv+argc);
        if(args.size()<2 || args[1]==L"--help" || args[1]==L"help") {help();return 0;}
        if(args[1]==L"doctor") {
            try {
                Hypervisor hv;
                std::cout << (hv.present()?"WHPX habilitado":"WHPX instalado pero no activo")
                          << "\nNo requiere QEMU, BIOS ni DLL externas.\n";
                return hv.present()?0:2;
            }catch(const std::exception& e) {
                std::cerr<<"WHPX no disponible: "<<e.what()<<"\n";return 2;
            }
        }
        if(args[1]==L"validate") {
            if(args.size()!=3 && args.size()!=5)throw std::runtime_error("Uso: validate NOMBRE [--ram 256]");
            size_t ramMiB=256;
            if(args.size()==5) {
                if(args[3]!=L"--ram")throw std::runtime_error("Uso: validate NOMBRE [--ram 256]");
                ramMiB=decimal(args[4],128,1024);
            }
            auto dir=profile(args[2]);
            auto kernel=read_file(dir/L"vmlinuz",256*1024*1024);
            auto initrd=read_file(dir/L"initramfs",128*1024*1024);
            auto info=inspect_kernel(kernel);
            const auto layout=calculate_layout(ramMiB*1024*1024,info,initrd.size(),
                std::string("console=ttyS0,115200 earlyprintk=serial,ttyS0 panic=5").size());
            std::cout<<"Perfil verificado (no ejecuta WHPX): "
                <<"RAM="<<ramMiB<<" MiB, kernel="<<layout.kernel_size
                <<" bytes, initramfs="<<layout.initrd_size
                <<" bytes, direccion inicio=0x"<<std::hex<<layout.kernel_addr
                <<", initramfs=0x"<<layout.initrd_addr<<std::dec<<"\n";
            return 0;
        }
        if(args[1]==L"list") {
            const auto dir=home()/L"distros";
            if(!fs::exists(dir)) {std::cout<<"Sin distribuciones registradas.\n";return 0;}
            for(const auto& d:fs::directory_iterator(dir)) {
                if(d.is_directory() && fs::is_regular_file(d.path()/L"vmlinuz") && fs::is_regular_file(d.path()/L"initramfs"))
                    std::wcout<<d.path().filename().wstring()<<L"\n";
            }
            return 0;
        }
        if(args[1]==L"add") {
            if(args.size()!=7 || args[3]!=L"--kernel" || args[5]!=L"--initrd")
                throw std::runtime_error("Uso: add NOMBRE --kernel RUTA --initrd RUTA");
            auto dest=profile(args[2]);
            if(fs::exists(dest))throw std::runtime_error("Nombre ya registrado");
            auto k=fs::absolute(fs::path(args[4])),i=fs::absolute(fs::path(args[6]));
            auto image=read_file(k,256*1024*1024);
            auto rd=read_file(i,128*1024*1024);
            inspect_kernel(image);
            fs::create_directories(dest);
            try {
                fs::copy_file(k,dest/L"vmlinuz",fs::copy_options::none);
                fs::copy_file(i,dest/L"initramfs",fs::copy_options::none);
            }catch(...) {std::error_code ignored;fs::remove_all(dest,ignored);throw;}
            std::cout<<"Distribucion registrada en el USB.\n";return 0;
        }
        if(args[1]==L"inspect") {
            if(args.size()!=3)throw std::runtime_error("Uso: inspect RUTA_KERNEL");
            auto bytes=read_file(fs::path(args[2]),256*1024*1024);
            auto info=inspect_kernel(bytes);
            std::cout<<"Kernel Linux bzImage valido, protocolo "<<(info.protocol>>8)<<"."
                <<(info.protocol&255)<<"\nBytes protegidos: "<<info.protected_size
                <<"\nInitrd max: 0x"<<std::hex<<info.initrd_addr_max<<std::dec<<"\n";
            return 0;
        }
        if(args[1]==L"smoke") {
            Hypervisor hv;
            hv.initialize(2*1024*1024);
            constexpr char message[]="WHPX ejecuta instrucciones x86 reales\r\n";
            std::vector<uint8_t> code={0xba,0xf8,0x03}; // mov dx,03f8
            for(auto c:std::string(message,sizeof(message)-1)) {
                code.push_back(0xb0);code.push_back(uint8_t(c)); // mov al,c
                code.push_back(0xee); // out dx,al
            }
            code.push_back(0xf4); // hlt
            std::copy(code.begin(),code.end(),hv.memory+0x1000);
            hv.setup_cpu(0x1000,0x8000,0);
            std::string output;
            int rc=execution_loop(hv,5,output,true);
            if(rc==0 && output==std::string(message,sizeof(message)-1))
                std::cout<<"PRUEBA WHPX COMPLETA: vCPU, RAM, salida UART y HLT validos.\n";
            else {std::cerr<<"Prueba incompleta: salida distinta o error CPU.\n";return 8;}
            return 0;
        }
        if(args[1]==L"boot" || args[1]==L"run") {
            fs::path kernel,initrd;
            bool saved=args[1]==L"run";
            if(saved) {
                if(args.size()<3)throw std::runtime_error("Uso: run NOMBRE");
                auto dir=profile(args[2]);
                kernel=dir/L"vmlinuz";initrd=dir/L"initramfs";
            }
            size_t memoryMiB=256,timeout=15;
            for(size_t i=saved?3:2;i<args.size();++i) {
                if(args[i]==L"--kernel" && !saved)kernel=fs::absolute(fs::path(arg(args,i)));
                else if(args[i]==L"--initrd" && !saved)initrd=fs::absolute(fs::path(arg(args,i)));
                else if(args[i]==L"--ram")memoryMiB=decimal(arg(args,i),128,1024);
                else if(args[i]==L"--timeout")timeout=decimal(arg(args,i),1,120);
                else throw std::runtime_error("Opcion no reconocida; usa --help");
            }
            if(kernel.empty()||initrd.empty())throw std::runtime_error("Requiere --kernel y --initrd");
            auto image=read_file(kernel,256*1024*1024);
            auto initramfs=read_file(initrd,128*1024*1024);
            auto info=inspect_kernel(image);
            Hypervisor hv;
            hv.initialize(memoryMiB*1024*1024);
            const std::string cmd="console=ttyS0,115200 earlyprintk=serial,ttyS0 panic=5";
            setup_boot_params(hv.memory,hv.memory_size,image,info,initramfs,cmd);
            hv.setup_cpu(0x100000,0x80000,0x90000);
            std::cout<<"Arranque Linux experimental (WHPX puro, sin chipset virtual).\n"
              <<"Kernel="<<info.protected_size<<" bytes, initramfs="<<initramfs.size()
              <<" bytes, RAM="<<memoryMiB<<" MiB; tiempo limite "<<timeout<<" s\n";
            std::string output;
            int rc=execution_loop(hv,static_cast<int>(timeout),output);
            if(rc==0)std::cout<<"CPU invitada finalizo la prueba. Esto no confirma una shell Linux.\n";
            return rc;
        }
        throw std::runtime_error("Comando desconocido, usa --help");
    } catch(const std::exception& e) {
        std::cerr<<"ERROR: "<<e.what()<<"\n";
        return 1;
    }
}
