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
 *  ItPDSimGui.h
 *
 *  Written by:
 *   Yarden Livnat
 *   Deinterfacement of Computer Science
 *   University of Utah
 *   Oct 2001
 *
 *  Copyright (C) 2001 SCI Group
 */

#ifndef SCI_ItPDSimGui_h
#define SCI_ItPDSimGui_h 

#include <Core/PartsGui/PartGui.h>
#include <Core/Util/Signals.h>

namespace SCIRun {

class ItPDSimPartGui : public PartGui {
public:
  Signal1<float> df;

public:
  ItPDSimPartGui( const string &name, const string &script ="ItPDSimPartGui"); 
  virtual ~ItPDSimPartGui();

  void attach( PartInterface *) {}
  virtual void tcl_command( TCLArgs &, void *);
};

} // namespace SCIRun

#endif // SCI_ItPDSimGui_h

