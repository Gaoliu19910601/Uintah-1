/*
 *  OldSFRGtoNewLatticeVol.cc: Converter
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   February 2001
 *
 *  Copyright (C) 2001 SCI Group
 */

#include <FieldConverters/Core/Datatypes/ScalarFieldRG.h>
#include <Core/Datatypes/LatticeVol.h>
#include <Core/Datatypes/LatVolMesh.h>
#include <Core/Persistent/Pstreams.h>

#include <iostream>
#include <fstream>
#include <stdlib.h>

using std::cerr;
using std::ifstream;
using std::endl;

using namespace SCIRun;
using namespace FieldConverters;

int main(int argc, char **argv) {
  ScalarFieldHandle handle;
  
  if (argc !=3) {
    cerr << "Usage: "<<argv[0]<<" OldSFRG NewLatticeVol\n";
    exit(0);
  }
  Piostream* stream=auto_istream(argv[1]);
  if (!stream) {
    cerr << "Error - couldn't open file "<<argv[1]<<".  Exiting...\n";
    exit(0);
  }
  Pio(*stream, handle);
  if (!handle.get_rep()) {
    cerr << "Error reading ScalarField from file "<<argv[1]<<".  Exiting...\n";
    exit(0);
  }
  
  ScalarFieldRGBase *base=dynamic_cast<ScalarFieldRGBase*>(handle.get_rep());
  if (!base) {
    cerr << "Error - input Field wasn't an SFRG.\n";
    exit(0);
  }

  FieldHandle fH;

  // create a lattice Field and give it a handle
  LatticeVol<double> *lf = new LatticeVol<double>;
  fH = FieldHandle(lf);

  // create a mesh identical to the base mesh
  LatVolMesh *mesh  = 
    dynamic_cast<LatVolMesh*>(lf->get_typed_mesh().get_rep());
  mesh->set_nx(base->nx);
  mesh->set_ny(base->ny);
  mesh->set_nz(base->nz);
  mesh->set_min(base->get_point(0,0,0));
  mesh->set_max(base->get_point(base->nx-1,base->ny-1,base->nz-1));
  cerr << "node index space extents = " 
       << base->nx << ", " << base->ny << ", " << base->nz << endl;
  cerr << "object space extents     = "
       << mesh->get_min() << ", " << mesh->get_max() << endl;

  // create storage for the data, and copy base's data into it
  FData3d<double> fdata = lf->fdata();
  fdata.newsize(base->nx+1,base->ny+1,base->nz+1);
  LatVolMesh::NodeIter iter = mesh->node_begin();
  int i=0,j=0,k=0;
  while (iter != mesh->node_end()) {
    fdata[*iter]=base->get_value(i,j,k);
    ++iter;
    ++i;
    if (i>=base->nx) {
      i=0; ++j;
      if (j>=base->ny) {
	j=0; ++k;
      }
    }
  }
  
  TextPiostream out_stream(argv[2], Piostream::Write);
  Pio(out_stream, fH);

  return 0;  
}    
