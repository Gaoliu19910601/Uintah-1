//  The contents of this file are subject to the University of Utah Public
//  License (the "License"); you may not use this file except in compliance
//  with the License.
//  
//  Software distributed under the License is distributed on an "AS IS"
//  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
//  License for the specific language governing rights and limitations under
//  the License.
//  
//  The Original Source Code is SCIRun, released March 12, 2001.
//  
//  The Original Source Code was developed by the University of Utah.
//  Portions created by UNIVERSITY are Copyright (C) 2001, 1994
//  University of Utah. All Rights Reserved.
//  
//    File   : TendEpireg.cc
//    Author : Martin Cole
//    Date   : Mon Sep  8 09:46:49 2003

#include <Dataflow/Network/Module.h>
#include <Core/Malloc/Allocator.h>
#include <Core/GuiInterface/GuiVar.h>
#include <Teem/Dataflow/Ports/NrrdPort.h>

#include <sstream>
#include <iostream>
using std::endl;
#include <stdio.h>

namespace SCITeem {

using namespace SCIRun;

class TendEpireg : public Module {
public:
  TendEpireg(SCIRun::GuiContext *ctx);
  virtual ~TendEpireg();
  virtual void execute();

private:
  NrrdIPort*      inrrd_;
  NrrdOPort*      onrrd_;

  GuiString    dwi_list_;
  GuiString    gradient_list_;
  GuiInt       reference_;
  GuiDouble    blur_x_;
  GuiDouble    blur_y_;
  GuiDouble    threshold_;
  GuiInt       cc_analysis_;
  GuiDouble    fitting_;
  GuiString    kernel_;
  GuiInt       base_;
};

DECLARE_MAKER(TendEpireg)

TendEpireg::TendEpireg(SCIRun::GuiContext *ctx) : 
  Module("TendEpireg", ctx, Filter, "Tensor", "Teem"), 
  dwi_list_(ctx->subVar("dwi_list")),
  gradient_list_(ctx->subVar("gradient_list")),
  reference_(ctx->subVar("reference")),
  blur_x_(ctx->subVar("blur_x")),
  blur_y_(ctx->subVar("blur_y")),
  threshold_(ctx->subVar("threshold")),
  cc_analysis_(ctx->subVar("cc_analysis")),
  fitting_(ctx->subVar("fitting")),
  kernel_(ctx->subVar("kernel")),
  base_(ctx->subVar("base"))
{
}

TendEpireg::~TendEpireg() {
}

void 
TendEpireg::execute()
{
  NrrdDataHandle nrrd_handle;
  update_state(NeedData);
  inrrd_ = (NrrdIPort *)get_iport("nin");
  onrrd_ = (NrrdOPort *)get_oport("nout");

  if (!inrrd_) {
    error("Unable to initialize iport 'Nrrd'.");
    return;
  }
  if (!onrrd_) {
    error("Unable to initialize oport 'Nrrd'.");
    return;
  }
  if (!inrrd_->get(nrrd_handle))
    return;

  if (!nrrd_handle.get_rep()) {
    error("Empty input Nrrd.");
    return;
  }

  Nrrd *nin = nrrd_handle->nrrd;

  error("This module is a stub.  Implement me.");

  //onrrd_->send(NrrdDataHandle(nrrd_joined));
}

} // End namespace SCITeem
