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

//    File   : HDF5DataReader.h
//    Author : Allen Sanderson
//             School of Computing
//             University of Utah
//    Date   : May 2003

#if !defined(HDF5DataReader_h)
#define HDF5DataReader_h

#include <Dataflow/Network/Module.h>

#include <Core/Util/TypeDescription.h>
#include <Core/Util/DynamicLoader.h>

#include <Packages/Teem/Core/Datatypes/NrrdData.h>
#include <Packages/Teem/Dataflow/Ports/NrrdPort.h>

#include <Core/GuiInterface/GuiVar.h>

#include <Packages/Teem/share/share.h>

namespace SCITeem {

using namespace SCIRun;

#define MAX_PORTS 6
#define MAX_DIMS 6

class TeemSHARE HDF5DataReader : public Module {
public:
  HDF5DataReader(GuiContext *context);

  virtual ~HDF5DataReader();

  virtual void execute();

  void parseDatasets( string new_datasets,
		      vector<string>& paths,
		      vector<string>& datasets );

  vector<int> getDatasetDims( string filename, string group, string dataset );

  //  float* readGrid( string filename );
  //  float* readData( string filename );
  NrrdDataHandle readDataset( string filename, string path, string dataset );

  virtual void tcl_command(GuiArgs&, void*);

private:
  GuiString filename_;
  GuiString datasets_;
  GuiString dumpname_;

  GuiInt nDims_;

  GuiInt mergeData_;
  GuiInt assumeSVT_;

  vector< GuiInt* > gDims_;
  vector< GuiInt* > gStarts_;
  vector< GuiInt* > gCounts_;
  vector< GuiInt* > gStrides_;

  string old_filename_;
  string old_datasets_;
  time_t old_filemodification_;

  int mergedata_;
  int assumesvt_;

  int dims_[MAX_DIMS];
  int starts_[MAX_DIMS];
  int counts_[MAX_DIMS];
  int strides_[MAX_DIMS];

  NrrdDataHandle nHandles_[MAX_PORTS];

  bool error_;
};


} // end namespace SCIRun

#endif // HDF5DataReader_h
