
#include <stdlib.h>
#include <iostream>
using std::cerr;
#include "Allocator.h"

struct X {
    int crap;
    X();
    ~X();
};

X::X()
{
    static int count;
    crap=count++;
}

X::~X()
{
    cerr << "~X() : crap=" << crap << "\n";
}

using SCICore::Malloc::Allocator;

void* operator new(size_t, Allocator*, const char*);
#ifdef _BOOL
void* operator new[](size_t, Allocator*, const char*);
#endif

main()
{
    char* p=new char[100];
    delete[] p;
    X* pp=new X;
    delete pp;
    pp=new X[5];
    delete[] pp;
    pp=new (SCICore::Malloc::default_allocator, "1") X;
    delete pp;
    pp=new (SCICore::Malloc::default_allocator, "2") X[5];
    delete[] pp;
    return 0;
}
