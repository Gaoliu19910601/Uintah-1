/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is Vtk, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/

/*
 *  InPort.h: 
 *
 *  Written by:
 *   Keming Zhang
 *   Department of Computer Science
 *   University of Utah
 *   January 2004
 *
 */

#ifndef SCIRun_Vtk_InPort_h
#define SCIRun_Vtk_InPort_h

#include <SCIRun/Vtk/Port.h>

namespace SCIRun {
  namespace vtk {
    class OutPort;
    class InPort: public Port{
    public:
      //constructor
      InPort();

      //virtual destructor
      virtual ~InPort();

      //user should define how to connect to an output port
      virtual void connect(OutPort* port)=NULL;

      //user should define if an output port is compatible to this input port
      virtual bool accept(OutPort* port)=NULL;
    };
  }
}

#endif
