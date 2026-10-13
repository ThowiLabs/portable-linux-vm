#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
static void printenv(const wchar_t* name){
  DWORD n=GetEnvironmentVariableW(name,nullptr,0);
  if(!n)return;
  std::vector<wchar_t> v(n);
  if(GetEnvironmentVariableW(name,v.data(),n))
    std::wcout<<L"ENV "<<name<<L"="<<v.data()<<L"\n";
}
int wmain(int n,wchar_t** a){
  std::vector<wchar_t> cwd(32768);
  if(GetCurrentDirectoryW(DWORD(cwd.size()),cwd.data()))
    std::wcout<<L"CWD="<<cwd.data()<<L"\n";
  for(int i=1;i<n;i++)std::wcout<<L"ARG"<<i<<L"="<<a[i]<<L"\n";
  printenv(L"PATH");printenv(L"HOME");printenv(L"LANG");printenv(L"LC_ALL");
  return 0;
}
