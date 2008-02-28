//
// 
// StackTrace.cc 
//
//   Used to display actual function names when looking at abridged
//   stack traces.  See README for more information.
//
// Author: J. Davison de St. Germain
// Date:   Feb. 27, 2008
// 
// Copyright (C) 2008 - C-SAFE, University of Utah
//

#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <string>
#include <vector>

using namespace std;

struct Info_S {
  string name;
  char   type;
  int    address;

  Info_S() { name = ""; type = 0; address = 0; }
};

vector< Info_S * > infoVector;

Info_S *
findLocation( int value ) 
{
  int low = 0;
  int high = infoVector.size() - 1;

  while( low <= high ) {
    int mid = (low + high) / 2;

    int addr = infoVector[mid]->address;

    if( addr > value ) {
      high = mid - 1;
    }
    else if( addr < value) {
      low = mid + 1;
    }
    else {
      return infoVector[mid];
    }
  }

  // See if it is 'close' to the address.
  if( low != 0 && low != infoVector.size() && value < infoVector[low]->address ) {
    return infoVector[ low-1 ];
  }

  return NULL;
}

void
readSymbolLocations( char * filename )
{
  FILE * fp = fopen( filename, "r" );

  if( fp == NULL ) {
    printf( "Error reading file '%s'.  Goodbye.\n\n", filename );
    exit( 1 );
  }

  int cnt = 0;

  while( !feof( fp ) ) {

    cnt++;
    if( cnt % 10000 == 0 ) {
      printf( "Number of symbols read: %d\n", cnt );
    }
    
    Info_S * info = new Info_S();
    char name[1024], address[1024];
    
    const int LINE_SIZE = 4000;
    char      line[ LINE_SIZE ];

    fgets( line, LINE_SIZE, fp );

    int numRead = sscanf( line, "%s %c %s", address, &info->type, name );
    if( numRead <= 0 ) {
      continue;
    }

    sscanf( address, "%x", &info->address );
    info->name = name;

    if( info->address != 0 ) {
      infoVector.push_back( info );
    }
  }
  printf( "\nTotal Number of Symbols: %d\n\n", cnt );
}

void
printStackTrace( char * filename )
{
  FILE * fp = fopen( filename, "r" );

  if( fp == NULL ) {
    printf( "Error reading file '%s'.  Goodbye.\n\n", filename );
    exit( 1 );
  }

  vector< string > stacktrace;

  printf("stack trace (raw):\n\n");

  while( !feof( fp ) ) {
    const int LINE_SIZE = 4000;
    char      line[ LINE_SIZE ];

    fgets( line, LINE_SIZE, fp );
    
    printf("%s", line);
    stacktrace.push_back( line );
  }

  printf("\n\n");
  printf("stack trace (with names):\n\n");

  for( int pos = 0; pos < stacktrace.size(); ++pos ) {

    string line = stacktrace[ pos ];
    int    loc  = line.rfind( "[" );
    string addrStr = line.substr( loc + 1, 8 );

    int    addr;

    sscanf( addrStr.c_str(), "%x", &addr );

    Info_S * info;

    info = findLocation( addr ); //, infoVector.size()/2, 0 );
    string name = "unknown";
    if( info ) {
      name = info->name;
    }
    printf( "%x -- %s\n", addr, name.c_str() );
  }
  printf( "\n" );
}

#define DO_TESTING 0

#if DO_TESTING

// Get the stack trace and print it out.  1st in raw form, then with
// associated function names.
void
bar()
{
  void* callstack[128];
  int i, frames = backtrace(callstack, 128);
  char** strs = backtrace_symbols(callstack, frames);

  printf("stack trace (raw):\n\n");
  for (i = 0; i < frames; ++i) {
    printf("%d: %s\n", i, strs[i]);
  }
  printf("\n\n");

  printf("stack trace (with names):\n\n");
  for (i = 0; i < frames; ++i) {

    string line = strs[i];
    int    loc  = line.rfind( "[" );
    string addrStr = line.substr( loc + 1, 8 );

    int    addr;

    sscanf( addrStr.c_str(), "%x", &addr );

    Info_S * info;

    info = findLocation( addr ); //, infoVector.size()/2, 0 );
    string name = "unknown";
    if( info ) {
      name = info->name;
    }
    printf( "%x -- %s\n", addr, name.c_str() );
  }
  free(strs);
}

// Dummy functions  to create a deeper stack trace.
void
foo() 
{
  bar();
}

double
doit( int value ) 
{
  foo();
  return 1.0;
}

// Tests the binary search lookup with a number of addresses.
void
test() 
{
  printf( "\n\n" );
  vector<int> addresses;
  
  addresses.push_back( 0x400bbf );
  addresses.push_back( 0x400bc0 );
  addresses.push_back( 0x400bc1 );
  addresses.push_back( 0x400db0 );
  addresses.push_back( 0x400ddc );
  addresses.push_back( 0x401af1 );
  addresses.push_back( 0x401af2 );
  addresses.push_back( 0x401af3 );
  addresses.push_back( 0x504987 );
  addresses.push_back( 0x504988 );
  addresses.push_back( 0x504989 );

  for( int pos = 0; pos < addresses.size(); pos++ ) {
    int addr = addresses[pos];

    Info_S * info = findLocation( addr ); //, infoVector.size()/2, 0 );
    string name = "unknown";
    int    address = 0;
    if( info ) {
      name = info->name;
      address = info->address;
    }
    printf( "%x -- %x -- %s\n\n\n", addr, address, name.c_str() );
  }
} // end test()
#endif // DO_TESTING

void
usage( string arg = "" )
{
  if( arg != "" ) {
    printf( "\n" );
    printf( "Bad argument: %s\n", arg.c_str() );
  }
  printf( "\n" );
  printf( "Usage: StackTrace <symbol_file> <stack_trace_file>\n" );
  printf( "\n" );
  printf( "       Reads in function symbols from 'symbol_file'.  ('Symbol_file' must be sorted!)\n" );
  printf( "       Then reads in the stack trace from 'stack_trace_file', looks up the function\n" );
  printf( "       addresses, and displays their names.\n" );
  printf( "\n" );
  exit( 1 );
}

int
main( int argc, char *argv[] )
{
  if( argc != 3 ) {
    usage();
  }

  readSymbolLocations( argv[1] );

#if DO_TESTING
  doit(1);
#endif

  printStackTrace( argv[2] );
}
