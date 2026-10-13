#include <windows.h>
#include <iostream>
int wmain(int n,wchar_t** a){ for(int i=1;i<n;i++)std::wcout<<L"ARG"<<i<<L"="<<a[i]<<L"\n";return 0;}
