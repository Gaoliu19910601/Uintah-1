/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2004 Scientific Computing and Imaging Institute,
   University of Utah.

   License for the specific language governing rights and limitations under
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/


/*
 *  BridgeModule.h:
 *
 *  Written by:
 *   Kostadin Damevski
 *   Department of Computer Science
 *   University of Utah
 *   September 2003
 *
 */

#ifndef Framework_Framework_BridgeModule_h
#define Framework_Framework_BridgeModule_h

#include <Core/GuiInterface/TCLInterface.h>
#include <Dataflow/Network/Module.h>
#include <Dataflow/Network/Network.h>
#include <Framework/Dataflow/SCIRunComponentModel.h>
#include <Framework/Bridge/BridgeComponent.h>

namespace SCIRun {
  class BridgeModule : public Module {
  public:
    BridgeModule(BridgeComponent* bc)
    : Module("BridgeModule",
             SCIRunComponentModel::gui->createContext("BridgeModule_" + bc->bridgeID),
             Filter),
      component(bc)
    {
      SCIRunComponentModel::net->add_instantiated_module(this);
    }
    virtual ~BridgeModule() {}
    virtual void execute() {component->execute();}
  private:
    BridgeComponent* component;
  };
}

#endif
