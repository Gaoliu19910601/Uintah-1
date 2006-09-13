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
 *  PortInstance.cc:
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   October 2001
 *
 */

#include <Framework/PortInstance.h>
#include <iostream>

namespace SCIRun {

// default keys
const std::string PortInstance::MAX_C("MAX_CONNECTIONS");
const std::string PortInstance::MIN_C("MIN_CONNECTIONS");
const std::string PortInstance::PROXY("ABLE_TO_PROXY");
const std::string PortInstance::NAME("cca.portName");
const std::string PortInstance::TYPE("cca.portType");
const std::string PortInstance::MODEL("sci.model");

std::string PortInstance::getType()
{
  return "default type";
}

std::string PortInstance::getModel()
{
  return "default model";
}

bool PortInstance::available()
{
  std::cerr << "default available() is called!"
            << std::endl;
  return true;
}

PortInstance* PortInstance::getPeer()
{
  std::cerr << "default getPeer() is called!"
            << std::endl;
  return 0;
}

PortInstance::PortInstance()
{
}

PortInstance::~PortInstance()
{
}

} // end namespace SCIRun
