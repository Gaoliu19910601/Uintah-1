/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/

/*
 *  ColumnMatrixToText.cc
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   April 2003
 *
 *  Copyright (C) 2003 SCI Group
 */

// This program will read in a SCIRun ColumnMatrix, and will save
// it out to a text version: a .txt file.
// The .txt file will contain one data value per line; the file will
// also have a one line header, specifying the number of entries in
// the matrix, unless the user specifies the -noHeader command-line 
// argument.

#include <Core/Datatypes/ColumnMatrix.h>
#include <Core/Persistent/Pstreams.h>
#include <StandAlone/convert/FileUtils.h>
#if defined(__APPLE__)
#  include <Core/Datatypes/MacForceLoad.h>
#endif
#include <iostream>
#include <fstream>
#include <stdlib.h>

using std::cerr;
using std::ifstream;
using std::endl;

using namespace SCIRun;

bool header;

void setDefaults() {
  header=true;
}

int parseArgs(int argc, char *argv[]) {
  int currArg = 3;
  while (currArg < argc) {
    if (!strcmp(argv[currArg],"-noHeader")) {
      header=false;
      currArg++;
    } else {
      cerr << "Error - unrecognized argument: "<<argv[currArg]<<"\n";
      return 0;
    }
  }
  return 1;
}

void printUsageInfo(char *progName) {
  cerr << "\n Usage: "<<progName<<" ColumnMatrix textfile [-noHeader]\n\n";
  cerr << "\t This program will read in a SCIRun ColumnMatrix, and will \n";
  cerr << "\t save it out to a text version: a .txt file. \n";
  cerr << "\t The .txt file will contain one data value per line; the \n";
  cerr << "\t file will also have a one line header, specifying the \n";
  cerr << "\t number of entries in the matrix, unless the user specifies \n";
  cerr << "\t the -noHeader command-line argument.\n\n";
}

int
main(int argc, char **argv) {
  if (argc < 3 || argc > 4) {
    printUsageInfo(argv[0]);
    return 0;
  }
#if defined(__APPLE__)  
  macForceLoad(); // Attempting to force load (and thus instantiation of
	          // static constructors) Core/Datatypes;
#endif
  setDefaults();

  char *matrixName = argv[1];
  char *textfileName = argv[2];
  if (!parseArgs(argc, argv)) {
    printUsageInfo(argv[0]);
    return 0;
  }

  MatrixHandle handle;
  Piostream* stream=auto_istream(matrixName);
  if (!stream) {
    cerr << "Couldn't open file "<<matrixName<<".  Exiting...\n";
    exit(0);
  }
  Pio(*stream, handle);
  if (!handle.get_rep()) {
    cerr << "Error reading matrix from file "<<matrixName<<".  Exiting...\n";
    exit(0);
  }
  ColumnMatrix *cm = dynamic_cast<ColumnMatrix *>(handle.get_rep());
  if (!cm) {
    cerr << "Error -- input field wasn't a ColumnMatrix\n";
    exit(0);
  }

  int nr=cm->nrows();
  cerr << "Number of rows = "<<nr<<"\n";
  FILE *fTxt = fopen(textfileName, "wt");
  if (!fTxt) {
    cerr << "Error -- couldn't open output file "<<textfileName<<"\n";
    exit(0);
  }
  if (header) fprintf(fTxt, "%d\n", nr);
  for (int r=0; r<nr; r++)
    fprintf(fTxt, "%lf\n", (*cm)[r]);
  fclose(fTxt);
  return 0;  
}    
