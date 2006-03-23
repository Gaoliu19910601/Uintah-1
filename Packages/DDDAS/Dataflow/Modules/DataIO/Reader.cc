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
 * C++ (CC) FILE : StreamReader.cc
 *
 * DESCRIPTION   : Continuously reads a series of .out mesh files from a 
 *                 specified directory (hard-coded) and 
 *                 produces a LatVol representation of each set of solution 
 *                 points generated by the AggieFEM/ccfv code. There are 64000
 *                 cell-centered solution points in the image, represented as 
 *                 a structured grid.  Each set of solution points represents 
 *                 one time step.  
 *   
 *
 * AUTHOR(S)     : Jenny Simpson
 *                 SCI Institute
 *                 University of Utah 
 *
 *                 (Modified from SampleLattice module written by Michael 
 *                 Callahan)
 * 
 * CREATED       : 6/2003
 *
 * MODIFIED      : Thu Feb 26 15:15:13 MST 2004
 *
 * NOTES         : Most of the code contained in this module has been 
 *                 borrowed from the SampleLattice module created by Mike 
 *                 Callahan.
 * 
 *                 This is a *test* module and is still around for legacy 
 *                 purposes only.  As such, path to the directory where the 
 *                 solution files are read from is hard-coded for testing 
 *                 purposes.  
 *
 *  Copyright (C) 2003 SCI Group
 */

// SCIRun includes

#include <Dataflow/Network/Module.h>
#include <Dataflow/Network/Ports/FieldPort.h>
#include <Core/Malloc/Allocator.h>
#include <Core/Basis/Constant.h>
#include <Core/Basis/HexTrilinearLgn.h>
#include <Core/Datatypes/LatVolMesh.h>
#include <Core/Containers/FData.h>
#include <Core/Datatypes/GenericField.h>
#include <Core/Geometry/BBox.h>
#include <Core/Geometry/Point.h>
#include <Core/GuiInterface/GuiVar.h>
#include <Core/Containers/StringUtil.h>
#include <Packages/DDDAS/share/share.h>

// Standard lib includes

#include <iostream>
#include <fstream>
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>

namespace DDDAS {

using namespace SCIRun;

typedef LatVolMesh<HexTrilinearLgn<Point> >                       LVMesh;
typedef ConstantBasis<double>                                     DatBasis;
typedef GenericField<LVMesh, DatBasis,  FData3d<double, LVMesh> > LVField;
  
// ****************************************************************************
// ******************************** Class: Reader *****************************
// ****************************************************************************

class Reader : public Module {

public:

  //! Virtual interface

  Reader(GuiContext* ctx);

  virtual ~Reader();

  virtual void execute();

  virtual void tcl_command(GuiArgs&, void*);

  //! Functions specific to LatVol mesh data

  double * read_ccfv( string filename, int& size );

  void fill_mesh( string filename, FieldOPort * ofp );
  
};
 
 
DECLARE_MAKER(Reader)

/*===========================================================================*/
// 
// Reader
//
// Description : Constructor
//
// Arguments   : 
//
// GuiContext* ctx - GUI context
//
Reader::Reader(GuiContext* ctx)
  : Module("Reader", ctx, Source, "DataIO", "DDDAS")
{ 
}

/*===========================================================================*/
// 
// ~Reader
//
// Description : Destructor
//
// Arguments   : none
//
Reader::~Reader()
{
}

/*===========================================================================*/
// 
// execute
//
// Description : The execute function for this module.  This is the control
//               center for the module.
//
// Arguments   : none
//
void Reader::execute()
{
  // Declare input field
  
  // We don't use the input field right now, but we still check to make sure
  // it can be initialized in case it needs to be used later.
  FieldIPort *ifp = (FieldIPort *)get_iport("Input Field");
  FieldHandle ifieldhandle;
  if (!ifp) {
    error("(Reader::execute) Unable to initialize iport 'Input Field'.");
    return;
  }

  // Declare output field
  FieldOPort *ofp = (FieldOPort *)get_oport("Output Sample Field");
  if (!ofp) {
    error("(Reader::execute) Unable to initialize oport 'Output Sample Field'.");
    return;
  }

  // Fill the mesh with solution points found in data files and output mesh
  DIR *dirp;
  struct dirent *dp;
  char *filename;
  char *dir = "/home/sci/simpson/data_stream/current/data/data_64K/";
  string dir_str = dir;
  
  dirp = opendir(dir);
  if (!dirp)
  {
    error("(Reader::execute) Directory Read Error");
    return;
  }

  
  while (1)
  {
    errno = 0;
    dp = readdir(dirp);
    if (!dp) break;

    filename = dp->d_name;
    string fn = filename;

    if(filename[0] != '.') // Exclude hidden files
    {
      string fn_str = filename;
      string filepath = dir_str + fn_str;
      cerr << "(Reader::execute) filename: " << filepath << endl;

      // Fill mesh
      fill_mesh( filepath, ofp );
    }
    
  }

  if(errno) error("(Reader::execute) Directory Read error");
  closedir(dirp);

  // TODO: Need to change so that last send is a send, not send_intermediate  
  ofp->reset();
}

/*===========================================================================*/
//
// tcl_command 
//
// Description : The tcl_command function for this module.
//
// Arguments   :
//
// GuiArgs& args - GUI arguments
//
// void* userdata - ???
// 
void Reader::tcl_command(GuiArgs& args, void* userdata)
{
  Module::tcl_command(args, userdata);
}

/*===========================================================================*/
// 
// fill_mesh
//
// Description : Reads solution points from an array, populates a LatVol mesh 
//               with them, and sends the mesh down the SCIRun pipeline to be 
//               eventually visualized by the Viewer.
//
// Arguments   :
// 
// string filename - The file containing the solution points for one mesh
//
// FieldOPort * ofp - The output port to send the mesh to after it is 
//                    constructed
//
void Reader::fill_mesh( string filename, FieldOPort * ofp )
{
  cerr << "(Reader::fill_mesh) Inside" << endl;

  cerr << "(Reader::fill_mesh) filename = " << filename << endl;

  Point minb, maxb;
  minb = Point(-1.0, -1.0, -1.0);
  maxb = Point(1.0, 1.0, 1.0);

  Vector diag((maxb.asVector() - minb.asVector()) * (0.0/100.0));
  minb -= diag;
  maxb += diag;
  
  // Read in the scalar values (solution points) from the file, store them in
  // an array
  double * sol_pts;
  int num_sols;
  
  sol_pts = read_ccfv( filename, num_sols );

  // Error checking -- returns if data file did not exist
  if( sol_pts == 0 )
  {
    return;
  }

  cerr << "(Reader::fill_mesh) Creating blank mesh" << endl;
  
  // Create blank mesh.
  int cube_root = (int) cbrt( num_sols );
  unsigned int sizex;
  unsigned int sizey;
  unsigned int sizez;
  sizex = sizey = sizez = Max(2, cube_root) + 1;
  LVMesh::handle_type mesh = scinew LVMesh(sizex, sizey, sizez, minb, maxb);

  cerr << "(Reader::fill_mesh) Assign data to cell centers" << endl;
  cerr << "(Reader::fill_mesh) Create Image Field" << endl;
  // Create Image Field.
  FieldHandle ofh;

  LVField *lvf = scinew LVField(mesh);

  LVField::fdata_type::iterator itr = lvf->fdata().begin();

  // Iterator for solution points array
  int i = 0; 
  while (itr != lvf->fdata().end())
  {
    assert( i < num_sols );
    *itr = sol_pts[i];
    ++itr;
    i++;
  }
  cerr << "(Reader::execute) number of field data points = " << i << endl;

  ofh = lvf;

  cerr << "(Reader::fill_mesh) Deallocate memory for sol_pts array" << endl;
  
  // Deallocate memory for sol_pts array
  delete[] sol_pts;

  cerr << "(Reader::fill_mesh) Sending data to output field" << endl;
  
  ofp->send_intermediate(ofh);

  cerr << "(Reader::fill_mesh) Leaving" << endl;
}

/*===========================================================================*/
// 
// read_ccfv  
//
// Description : Read in a file generated by ccfv and store its solution points
//               as an array of doubles.  Returns a pointer to the array of
//               solution points. Assumes that if the file exists, it is of the
//               correct format.
//
// Arguments   :
// 
// string filename - Name of file to parse for solution points
//
// int& size - The size of the mesh in the file, i.e. the number of solution 
//             points / nodes in the mesh.  This is returned by reference.
//
double * Reader::read_ccfv( string filename, int& size )
{
  cerr << "(Reader::read_ccfv) Inside" << endl;
  /*
    Here's an sample of what the file should look like:

    Data File Format
    ----------------

    [char *s (file name)]
    solution u -size=[int Ncells (number of solution pts)] -components=1 -type=nodal
    [double solution(0) (Solution point 0.  See notes below)]
    [double solution(1)]
    [double solution(2)]
    [double solution(3)]
    [double solution(4)]
    [double solution(5)]
    ...
    [double solution(Ncells)]

    Sample File
    -----------

    sat.out
    solution u  -size=64000 -components=1 -type=nodal
    0.584279
    0.249236
    0.0711161
    0.0134137
    0.00190625
    0.000223068
    2.70226e-05
    ...
    
  */
  
  // Open up an input file stream
  ifstream input( filename.c_str() );
  if ( !input )
  {
    error( "(Reader::read_ccfv)  File not found." );
    return 0;
  }

  // Set up variables corresponding to the file values
  string filename_h;
  string solution;
  int components;
  string type;
  string str;
  char ch;
  double sol;
  
  // Parse the header, assigning variable values

  input >> filename_h
        >> str
        >> solution
        >> ch >> ch >> ch >> ch >> ch >> ch
        >> size
        >> ch >> ch >> ch >> ch >> ch >> ch 
        >> ch >> ch >> ch >> ch >> ch >> ch
        >> components
        >> ch >> ch >> ch >> ch >> ch >> ch
        >> type;

  // Allocate memory for solution points
  double * sol_pts = new double[size]; // Replace new with scinew?
  
  // Populate the field values array with solution points
  for( int i = 0; i < size; i++ )
  {
    input >> sol;
    sol_pts[i] = sol;
  }

  // Close file
  input.close();

  cerr << "(Reader::read_ccfv) Leaving" << endl;
  return sol_pts;
}
 

} // End namespace DDDAS




