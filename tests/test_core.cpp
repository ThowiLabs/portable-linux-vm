#include "../src/core.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>
using namespace plvm;
int main() {
 assert(valid_name(L"alpine"));
 assert(valid_name(L"ubuntu_24"));
 assert(!valid_name(L"../a"));
 assert(!valid_name(L"CON"));
 assert(!valid_name(L"a/b"));
 assert(!valid_name(L""));
 assert(number(L"4096",256,65536,"ram")==4096);
 try{number(L"4x",1,32,"cpu");assert(false);}catch(const std::runtime_error&){}
 assert(disk_size(L"16G")==L"16G");
 try{disk_size(L"100M");assert(false);}catch(const std::runtime_error&){}
 assert(quote_arg(L"C:\\Virtual Machines\\") == L"\"C:\\Virtual Machines\\\\\"");
 assert(drive_arg(fs::path(L"C:\\imagenes\\a,b.qcow2")).find(L"a,,b.qcow2")!=std::wstring::npos);
 fs::path dir=fs::temp_directory_path()/"portablevm-testdata";fs::create_directories(dir);
 save_vm(dir/"vm.cfg",VM{2048,4});
 auto vm=read_vm(dir/"vm.cfg");
 assert(vm.ram==2048&&vm.cpus==4);
 fs::path qc=dir/"test.qcow2";
 { std::ofstream image(qc,std::ios::binary);
   unsigned char h[20]={0x51,0x46,0x49,0xfb,0,0,0,3};
   image.write(reinterpret_cast<const char*>(h),20);
 }
 assert(is_qcow2(qc));
 assert(!has_backing_file(qc));
 {std::fstream image(qc,std::ios::in|std::ios::out|std::ios::binary);
   image.seekp(15);char one=1;image.write(&one,1);
 }
 assert(has_backing_file(qc));
 fs::remove_all(dir);
 std::cout<<"OK: nombre, rangos, argumentos Win32, QCOW2, configuracion portable\n";
}
