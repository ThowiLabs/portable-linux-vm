#pragma once
#ifndef _WIN32
#error "Este archivo solo compila para Windows"
#endif
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
#include "core.hpp"

namespace plvm {
inline fs::path executable_dir() {
    std::wstring buffer(32768,L'\0');
    DWORD n=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
    if(!n || n>=buffer.size()) throw std::runtime_error("No se pudo determinar ruta del exe");
    buffer.resize(n);
    return fs::path(buffer).parent_path();
}
inline bool whpx_present() {
    HMODULE dll=LoadLibraryExW(L"WinHvPlatform.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!dll) return false;
    using WHvCapability = HRESULT(WINAPI*)(int,void*,UINT32,UINT32*);
    auto fn=reinterpret_cast<WHvCapability>(GetProcAddress(dll,"WHvGetCapability"));
    UINT32 present=0,written=0;
    const bool ok=fn && SUCCEEDED(fn(0,&present,sizeof(present),&written)) && written==sizeof(present) && present!=0;
    FreeLibrary(dll);
    return ok;
}
inline int child_process(const fs::path& executable,const std::vector<std::wstring>& args) {
    std::wstring command=quote_arg(executable.wstring());
    for(const auto& arg:args){command+=L" ";command+=quote_arg(arg);}
    STARTUPINFOW si{}; si.cb=sizeof(si);
    PROCESS_INFORMATION pi{};
    auto dir=executable.parent_path().wstring();
    BOOL ok=CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,0,nullptr,
        dir.c_str(),&si,&pi);
    if(!ok) {
        auto err=GetLastError();
        throw std::runtime_error("No se pudo iniciar proceso Win32 (codigo "+std::to_string(err)+")");
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess,INFINITE);
    DWORD result=1;
    GetExitCodeProcess(pi.hProcess,&result);
    CloseHandle(pi.hProcess);
    return static_cast<int>(result);
}
}
