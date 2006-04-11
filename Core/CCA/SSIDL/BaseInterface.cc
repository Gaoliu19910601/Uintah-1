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
 *  BaseInterface: Implementation of SSIDL.BaseInterface for PIDL
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   September 1999
 *
 *  Copyright (C) 1999 SCI Group
 */

#include <Core/CCA/SSIDL/sidl_sidl.h>
#include <Core/CCA/PIDL/TypeInfo.h>
#include <Core/Util/NotFinished.h>

using SSIDL::BaseInterface;
using SSIDL::ClassInfo;


void BaseInterface::addRef()
{
  SCIRun::Object::addReference();
}

void BaseInterface::deleteRef()
{
  SCIRun::Object::deleteReference();
}

bool BaseInterface::isSame(const BaseInterface::pointer& object)
{
  // Not tested!
  return this == object.getPointer();
}

bool BaseInterface::isType(const std::string& name)
{
  const SCIRun::TypeInfo* typeInfo = BaseInterface::_static_getTypeInfo();
  // Not tested!
  // Is there likely to be a naming clash between the PIDL and babel component-based objects?
  if (typeInfo->isa(name) == SCIRun::TypeInfo::VTABLE_INVALID) {
    return false;
  } else {
    return true;
  }
}

ClassInfo::pointer BaseInterface::getClassInfo()
{
  NOT_FINISHED("ClassInfo .SSIDL.getClassInfo()");
  return ClassInfo::pointer(0);
}
