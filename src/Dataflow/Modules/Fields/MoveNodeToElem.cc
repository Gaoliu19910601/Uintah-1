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
 *  MoveNodeToElem.cc:  Rotate and flip field to get it into "standard" view
 *
 *  Written by:
 *   Michael Callahan
 *   Department of Computer Science
 *   University of Utah
 *   March 2001
 *
 *  Copyright (C) 2001 SCI Group
 */

#include <Dataflow/Network/Module.h>
#include <Dataflow/Network/Ports/FieldPort.h>
#include <Dataflow/Modules/Fields/MoveNodeToElem.h>
#include <iostream>

namespace SCIRun {


class MoveNodeToElem : public Module
{
public:
  MoveNodeToElem(GuiContext* ctx);
  virtual ~MoveNodeToElem();

  virtual void execute();

protected:
  int ifield_generation_;
};


DECLARE_MAKER(MoveNodeToElem)

MoveNodeToElem::MoveNodeToElem(GuiContext* ctx)
  : Module("MoveNodeToElem", ctx, Filter, "FieldsData", "SCIRun"),
    ifield_generation_(0)
{
}


MoveNodeToElem::~MoveNodeToElem()
{
}


void
MoveNodeToElem::execute()
{
  // Get input field.
  FieldIPort *ifp = (FieldIPort *)get_iport("Node Field");
  FieldHandle ifield;
  if (!(ifp->get(ifield) && ifield.get_rep()))
  {
    return;
  }

  // Get the output port now, because we may be able to pass the field
  // directly through if it is already cell centered.
  FieldOPort *ofp = (FieldOPort *)get_oport("Elem Field");

  string ext = "";
  const TypeDescription *mtd = ifield->mesh()->get_type_description();
  if (mtd->get_name().find("LatVolMesh") != string::npos)
  {
    if (ifield->basis_order() != 1)
    {
      remark("Field is already node centered.  Passing through.");
      ofp->send_and_dereference(ifield);
      return;
    }
    ext = "Lat";
  }
  else if (mtd->get_name().find("ImageMesh") != string::npos)
  {
    if (ifield->basis_order() != 1)
    {
      remark("Field is already node centered.  Passing through.");
      ofp->send_and_dereference(ifield);
      return;
    }
    ext = "Img";
  }
  else
  {
    error("Unsupported mesh type.  This only works on LatVols and Images.");
    return;
  }

  if (ifield_generation_ != ifield->generation || !ofp->have_data())
  {
    const TypeDescription *ftd = ifield->get_type_description();
    TypeDescription::td_vec *tdv = 
      ifield->get_type_description(Field::FDATA_TD_E)->get_sub_type();
    const string actype = (*tdv)[0]->get_name();
    const string btype =
      "ConstantBasis<" + actype + "> ";
    const string fts =
      ifield->get_type_description(Field::FIELD_NAME_ONLY_E)->get_name() + "<" +
      ifield->get_type_description(Field::MESH_TD_E)->get_name() + "," +
      btype + "," +
      ifield->get_type_description(Field::FDATA_TD_E)->get_name() + "> ";
    CompileInfoHandle ci = MoveNodeToElemAlgo::get_compile_info(ftd, fts, ext);
    Handle<MoveNodeToElemAlgo> algo;
    if (!DynamicCompilation::compile(ci, algo, false, this))
    {
      error("Unable to compile MoveNodeToElem algorithm.");
      return;
    }

    FieldHandle ofield = algo->execute(this, ifield);
  
    ofp->send_and_dereference(ofield);
  }
}


CompileInfoHandle
MoveNodeToElemAlgo::get_compile_info(const TypeDescription *fsrc,
                                     const string &fdst,
                                     const string &ext)
{
  // Use cc_to_h if this is in the .cc file, otherwise just __FILE__
  static const string include_path(TypeDescription::cc_to_h(__FILE__));
  const string template_class_name("MoveNodeToElemAlgo" + ext);
  static const string base_class_name("MoveNodeToElemAlgo");

  CompileInfo *rval = 
    scinew CompileInfo(template_class_name + "." +
		       fsrc->get_filename() + "." +
                       to_filename(fdst) + ".",
                       base_class_name, 
                       template_class_name, 
                       fsrc->get_name() + ", " + fdst);

  // Add in the include path to compile this obj
  rval->add_include(include_path);
  rval->add_basis_include("../src/Core/Basis/Constant.h");
  fsrc->fill_compile_info(rval);

  return rval;
}


} // End namespace SCIRun

