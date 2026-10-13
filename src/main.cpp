#include "core.hpp"
#include "winproc.hpp"
#include <iostream>
#include <system_error>
#include <vector>

using namespace plvm;
using std::wstring;
namespace fs=std::filesystem;

static void help() {
    std::wcout <<
    L"Portable Linux VM v0.1 (Windows x64, C++20)\n\n"
    L"  portablevm.exe doctor\n"
    L"  portablevm.exe list\n"
    L"  portablevm.exe init NOMBRE [--size 16G] [--ram 2048] [--cpus 2] [--iso archivo.iso]\n"
    L"  portablevm.exe import NOMBRE disco.qcow2 [--ram 2048] [--cpus 2]\n"
    L"  portablevm.exe iso NOMBRE archivo.iso\n"
    L"  portablevm.exe eject NOMBRE\n"
    L"  portablevm.exe info NOMBRE\n"
    L"  portablevm.exe run NOMBRE [--accel auto|whpx|tcg] [--console] [--share CARPETA] [--no-iso] [--dry-run]\n"
    L"  portablevm.exe check NOMBRE\n"
    L"  portablevm.exe docker-help\n\n"
    L"Todo el contenido se guarda relativo a la carpeta del programa.\n"
    L"Se necesitan engine\\qemu-system-x86_64.exe y engine\\qemu-img.exe, con sus DLL.\n";
}
static fs::path vm_dir(const fs::path& root,const wstring& name) {
    if(!valid_name(name)) throw std::runtime_error("Nombre invalido: usa letras ASCII, numeros, _ o - (max 48)");
    return root/L"vms"/name;
}
static fs::path expect_vm(const fs::path& root,const wstring& name) {
    auto dir=vm_dir(root,name);
    if(!fs::is_regular_file(dir/L"vm.cfg") || !is_qcow2(dir/L"disk.qcow2"))
        throw std::runtime_error("VM inexistente o disco QCOW2 invalido");
    return dir;
}
static wstring option_value(const std::vector<wstring>& args,size_t& i) {
    if(i+1>=args.size()) throw std::runtime_error("Opcion requiere valor");
    return args[++i];
}
static void copy_iso(const fs::path& source,const fs::path& dest) {
    if(!is_iso(source)) throw std::runtime_error("El ISO indicado no existe o no tiene extension .iso");
    if(fs::exists(dest) && fs::equivalent(source,dest)) return;
    auto tmp=dest;tmp+=L".part";
    if(fs::exists(tmp)) throw std::runtime_error("Hay una copia ISO incompleta .part; revisala");
    try {
        fs::copy_file(source,tmp,fs::copy_options::none);
        if(fs::exists(dest)) fs::remove(dest);
        fs::rename(tmp,dest);
    } catch(...) {std::error_code ec;fs::remove(tmp,ec);throw;}
}
static void doctor(const fs::path& root) {
    std::wcout << L"Directorio portable: " << root.wstring() << L"\n";
    auto qemu=root/L"engine"/L"qemu-system-x86_64.exe";
    auto img=root/L"engine"/L"qemu-img.exe";
    std::wcout << L"Motor QEMU Windows: " << (fs::is_regular_file(qemu)?L"encontrado":L"FALTA") << L"\n";
    std::wcout << L"Herramienta qcow2: " << (fs::is_regular_file(img)?L"encontrada":L"FALTA") << L"\n";
    std::wcout << L"API WHPX disponible: " << (whpx_present()?L"SI":L"NO / no habilitada") << L"\n";
    std::wcout << L"Si WHPX no funciona se puede utilizar TCG con --accel tcg.\n";
    std::wcout << L"Los binarios QEMU necesitan tambien sus DLL y archivos de recursos.\n";
}
static void list(const fs::path& root) {
    auto base=root/L"vms";
    if(!fs::exists(base)) {std::wcout<<L"No hay maquinas virtuales.\n";return;}
    int count=0;
    for(const auto& entry:fs::directory_iterator(base)) {
        if(!entry.is_directory())continue;
        const auto name=entry.path().filename().wstring();
        if(!valid_name(name))continue;
        try {
            const auto vm=read_vm(entry.path()/L"vm.cfg");
            if(!is_qcow2(entry.path()/L"disk.qcow2"))continue;
            std::wcout<<L" - "<<name<<L": "<<vm.ram<<L" MiB, "<<vm.cpus<<L" vCPU, QCOW2";
            if(fs::exists(entry.path()/L"install.iso"))std::wcout<<L", ISO adjunta";
            std::wcout<<L"\n";++count;
        }catch(...) {std::wcerr<<L"AVISO: configuracion invalida: "<<name<<L"\n";}
    }
    if(!count)std::wcout<<L"No hay VMs validas.\n";
}
static void init(const fs::path& root,const std::vector<wstring>& a,bool importing) {
    if(a.size()<(importing?4:3))throw std::runtime_error("Faltan argumentos");
    auto name=a[2]; auto dir=vm_dir(root,name);
    VM vm;
    wstring size=L"16G";
    fs::path iso,origin;
    size_t index=3;
    if(importing) {origin=fs::absolute(fs::path(a[3]));index=4;}
    for(size_t i=index;i<a.size();++i) {
        if(a[i]==L"--size"&&!importing)size=disk_size(option_value(a,i));
        else if(a[i]==L"--ram")vm.ram=number(option_value(a,i),256,65536,"RAM");
        else if(a[i]==L"--cpus")vm.cpus=number(option_value(a,i),1,32,"CPU");
        else if(a[i]==L"--iso"&&!importing)iso=fs::absolute(fs::path(option_value(a,i)));
        else throw std::runtime_error("Opcion desconocida o no permitida");
    }
    if(fs::exists(dir))throw std::runtime_error("La carpeta de esa VM ya existe");
    if(importing && !is_qcow2(origin))throw std::runtime_error("El archivo origen no tiene cabecera QCOW2 valida");
    if(importing && has_backing_file(origin))throw std::runtime_error("QCOW2 tiene backing file externo: consolida con qemu-img convert antes de importar");
    if(!iso.empty()&&!is_iso(iso))throw std::runtime_error("Archivo ISO no valido");
    fs::create_directories(dir);
    try {
        auto disk=dir/L"disk.qcow2";
        if(importing)fs::copy_file(origin,disk,fs::copy_options::none);
        else {
            auto tool=root/L"engine"/L"qemu-img.exe";
            if(!fs::is_regular_file(tool))throw std::runtime_error("Falta engine\\qemu-img.exe y sus dependencias");
            int code=child_process(tool,{L"create",L"-f",L"qcow2",disk.wstring(),size});
            if(code)throw std::runtime_error("qemu-img create fallo (codigo "+std::to_string(code)+")");
        }
        if(!is_qcow2(disk))throw std::runtime_error("El disco generado no es QCOW2 valido");
        if(!iso.empty())copy_iso(iso,dir/L"install.iso");
        save_vm(dir/L"vm.cfg",vm);
        std::wcout<<L"VM preparada en "<<dir.wstring()<<L"\n";
        std::wcout<<L"Arrancar: portablevm.exe run "<<name<<L"\n";
    }catch(...) {std::error_code ignored;fs::remove_all(dir,ignored);throw;}
}
static void info(const fs::path& root,const wstring& name) {
    auto dir=expect_vm(root,name);
    auto vm=read_vm(dir/L"vm.cfg");
    std::wcout<<L"Nombre: "<<name<<L"\nRAM: "<<vm.ram<<L" MiB\nvCPU: "<<vm.cpus
       <<L"\nDisco: "<<(dir/L"disk.qcow2").wstring()<<L"\nISO: "
       <<(fs::is_regular_file(dir/L"install.iso")?L"adjunta":L"no")<<L"\n";
}
static void attach_iso(const fs::path& root,const wstring& name,const fs::path& iso) {
    auto dir=expect_vm(root,name);
    copy_iso(fs::absolute(iso),dir/L"install.iso");
    std::wcout<<L"ISO copiada junto a la VM: "<<name<<L"\n";
}
static void docker_help() {
    std::wcout << L"Docker funciona DENTRO del Linux invitado, no en Windows directamente.\n"
        L"En un Alpine Linux instalado (no el entorno live):\n"
        L"  apk update\n  apk add docker\n  rc-update add docker default\n"
        L"  service docker start\n  docker run --rm -it alpine sh\n"
        L"  docker run --rm -it debian:stable-slim bash\n\n"
        L"Requiere red, cgroups, kernel compatible, almacenamiento y RAM suficientes.\n"
        L"Un contenedor NO arranca un kernel distinto ni instala un ISO.\n";
}
static int run_vm(const fs::path& root,const std::vector<wstring>& a) {
    if(a.size()<3)throw std::runtime_error("Falta nombre de VM");
    auto name=a[2]; auto dir=expect_vm(root,name); auto cfg=read_vm(dir/L"vm.cfg");
    wstring accel=L"auto";
    bool console=false,without_iso=false,dry=false;
    fs::path share;
    for(size_t i=3;i<a.size();++i) {
        if(a[i]==L"--accel")accel=option_value(a,i);
        else if(a[i]==L"--console")console=true;
        else if(a[i]==L"--no-iso")without_iso=true;
        else if(a[i]==L"--dry-run")dry=true;
        else if(a[i]==L"--share")share=fs::absolute(fs::path(option_value(a,i)));
        else throw std::runtime_error("Opcion run desconocida");
    }
    if(accel!=L"auto"&&accel!=L"whpx"&&accel!=L"tcg")throw std::runtime_error("Aceleracion: auto, whpx o tcg");
    if(!share.empty()&&!fs::is_directory(share))throw std::runtime_error("Carpeta compartida no existe");
    auto qemu=root/L"engine"/L"qemu-system-x86_64.exe";
    if(!dry&&!fs::is_regular_file(qemu))throw std::runtime_error("Falta engine\\qemu-system-x86_64.exe, con sus DLL");
    const wstring backend=accel==L"auto"?L"whpx:tcg":accel;
    std::vector<wstring> cmd={L"-name",name,L"-machine",L"pc,accel="+backend,
        L"-m",std::to_wstring(cfg.ram),L"-smp",std::to_wstring(cfg.cpus),
        L"-drive",drive_arg(dir/L"disk.qcow2"),
        L"-netdev",L"user,id=lan0",L"-device",L"virtio-net-pci,netdev=lan0"};
    auto iso=dir/L"install.iso";
    if(!without_iso&&fs::is_regular_file(iso)) {
        cmd.insert(cmd.end(),{L"-cdrom",iso.wstring(),L"-boot",L"order=dc,menu=on"});
    }
    if(!share.empty()) {
        // Disco FAT virtual de solo lectura; evita permisos de escritura sobre Windows.
        cmd.insert(cmd.end(),{L"-hdb",L"fat:"+share.wstring()});
        std::wcout<<L"AVISO: carpeta compartida en modo lectura (FAT virtual, nombres ASCII recomendados).\n";
    }
    if(console)cmd.insert(cmd.end(),{L"-nographic"});
    else cmd.insert(cmd.end(),{L"-monitor",L"none"});
    if(dry) {
        std::wcout<<L"Comando (simulacion; no inicia QEMU):\n"<<quote_arg(qemu.wstring());
        for(const auto& arg:cmd)std::wcout<<L" "<<quote_arg(arg);
        std::wcout<<L"\n";
        return 0;
    }
    std::wcout<<L"Iniciando "<<name<<L" [acelerador "<<backend<<L"]...\n";
    if(!console)std::wcout<<L"QEMU abrira su pantalla grafica. --console requiere Linux configurado para ttyS0.\n";
    if(!without_iso&&fs::is_regular_file(iso))std::wcout<<L"ISO adjunta: al terminar la instalacion ejecuta: portablevm eject "<<name<<L"\n";
    return child_process(qemu,cmd);
}
int wmain(int argc,wchar_t** argv) {
    try {
        SetConsoleOutputCP(CP_UTF8);
        std::vector<wstring> a;
        for(int i=0;i<argc;++i)a.emplace_back(argv[i]);
        const auto root=executable_dir();
        if(a.size()<2 || a[1]==L"help" || a[1]==L"--help") {help();return 0;}
        const auto& command=a[1];
        if(command==L"doctor") {doctor(root);return 0;}
        if(command==L"list") {list(root);return 0;}
        if(command==L"docker-help") {docker_help();return 0;}
        if(command==L"init") {init(root,a,false);return 0;}
        if(command==L"import") {init(root,a,true);return 0;}
        if(a.size()<3)throw std::runtime_error("Indica nombre de VM");
        auto dir=expect_vm(root,a[2]);
        if(command==L"info") {info(root,a[2]);return 0;}
        if(command==L"iso") {
            if(a.size()!=4)throw std::runtime_error("Uso: iso NOMBRE archivo.iso");
            attach_iso(root,a[2],fs::path(a[3]));return 0;
        }
        if(command==L"eject") {
            auto src=dir/L"install.iso";
            if(!fs::exists(src))throw std::runtime_error("La VM no tiene ISO adjunta");
            auto target=dir/L"installer-backup.iso";
            if(fs::exists(target))throw std::runtime_error("installer-backup.iso ya existe; no se sobreescribe");
            fs::rename(src,target);
            std::wcout<<L"ISO desadjuntada y conservada: "<<target.wstring()<<L"\n";return 0;
        }
        if(command==L"run")return run_vm(root,a);
        if(command==L"check") {
            auto tool=root/L"engine"/L"qemu-img.exe";
            if(!fs::is_regular_file(tool))throw std::runtime_error("Falta engine\\qemu-img.exe");
            std::wcout<<L"ATENCION: ejecuta esto solo con la VM completamente apagada.\n";
            return child_process(tool,{L"check",L"-f",L"qcow2",(dir/L"disk.qcow2").wstring()});
        }
        throw std::runtime_error("Comando desconocido: usa help");
    }catch(const std::exception& e) {
        std::cerr<<"ERROR: "<<e.what()<<"\n";
        return 2;
    }
}
