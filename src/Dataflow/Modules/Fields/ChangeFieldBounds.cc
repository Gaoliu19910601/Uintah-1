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


//    File   : ChangeFieldBounds.cc
//    Author : McKay Davis
//    Date   : July 2002


#include <Dataflow/Network/Module.h>
#include <Core/Malloc/Allocator.h>
#include <Core/Datatypes/FieldInterface.h>
#include <Dataflow/share/share.h>

#include <Dataflow/Ports/FieldPort.h>
#include <Dataflow/Ports/GeometryPort.h>
#include <Dataflow/Ports/MatrixPort.h>
#include <Core/Geometry/Transform.h>
#include <Core/Thread/CrowdMonitor.h>
#include <Dataflow/Widgets/BoxWidget.h>
#include <Dataflow/Network/NetworkEditor.h>
#include <Core/Datatypes/DenseMatrix.h>
#include <Core/Containers/StringUtil.h>
#include <map>
#include <iostream>

namespace SCIRun {

using std::endl;
using std::pair;

class PSECORESHARE ChangeFieldBounds : public Module {
public:
  ChangeFieldBounds(GuiContext* ctx);
  virtual ~ChangeFieldBounds();

  GuiDouble		outputcenterx_;	// the out geometry (center point and size)
  GuiDouble		outputcentery_;
  GuiDouble		outputcenterz_;
  GuiDouble		outputsizex_;
  GuiDouble		outputsizey_;
  GuiDouble		outputsizez_;
  GuiInt		useoutputcenter_;   // center checkbox
  GuiInt		useoutputsize_;   // size checkbox

  CrowdMonitor		widget_lock_;
  BoxWidget *		box_;
  Transform		box_initial_transform_;
  Transform		field_initial_transform_;
  BBox			box_initial_bounds_;
  int			generation_;
  int			widgetid_;

  void clear_vals();
  void update_input_attributes(FieldHandle);
  void build_widget(FieldHandle);

  virtual void execute();
  virtual void widget_moved(bool);
};

  DECLARE_MAKER(ChangeFieldBounds)

ChangeFieldBounds::ChangeFieldBounds(GuiContext* ctx)
  : Module("ChangeFieldBounds", ctx, Source, "Fields", "SCIRun"),
    outputcenterx_(ctx->subVar("outputcenterx")),
    outputcentery_(ctx->subVar("outputcentery")),
    outputcenterz_(ctx->subVar("outputcenterz")),
    outputsizex_(ctx->subVar("outputsizex")),
    outputsizey_(ctx->subVar("outputsizey")),
    outputsizez_(ctx->subVar("outputsizez")),
    useoutputcenter_(ctx->subVar("useoutputcenter")),
    useoutputsize_(ctx->subVar("useoutputsize")),
    widget_lock_("ChangeFieldBounds widget lock"),
    generation_(-1),
    widgetid_(0)
{
  box_ = scinew BoxWidget(this, &widget_lock_, 1.0, false, false);
  box_->Connect((GeometryOPort*)get_oport("Transformation Widget"));
}

ChangeFieldBounds::~ChangeFieldBounds()
{
  delete box_;
}



void
ChangeFieldBounds::clear_vals() 
{
  gui->execute(string("set ")+id+"-inputcenterx \"---\"");
  gui->execute(string("set ")+id+"-inputcentery \"---\"");
  gui->execute(string("set ")+id+"-inputcenterz \"---\"");
  gui->execute(string("set ")+id+"-inputsizex \"---\"");
  gui->execute(string("set ")+id+"-inputsizey \"---\"");
  gui->execute(string("set ")+id+"-inputsizez \"---\"");
  gui->execute(id+" update_multifields");
}


void
ChangeFieldBounds::update_input_attributes(FieldHandle f) 
{
  Point center;
  Vector size;
  
  BBox bbox = f->mesh()->get_bounding_box();

  if (!bbox.valid()) {
    warning("Input field is empty -- using unit cube.");
    bbox.extend(Point(0,0,0));
    bbox.extend(Point(1,1,1));
  }
  size = bbox.diagonal();
  center = bbox.center();

  gui->execute(string("set ")+id+"-inputcenterx "+to_string(center.x()));
  gui->execute(string("set ")+id+"-inputcentery "+to_string(center.y()));
  gui->execute(string("set ")+id+"-inputcenterz "+to_string(center.z()));
  gui->execute(string("set ")+id+"-inputsizex "+to_string(size.x()));
  gui->execute(string("set ")+id+"-inputsizey "+to_string(size.y()));
  gui->execute(string("set ")+id+"-inputsizez "+to_string(size.z()));

  gui->execute(id+" update_multifields");
}


void
ChangeFieldBounds::build_widget(FieldHandle f)
{
  double l2norm;
  Point center;
  Vector size;
  BBox bbox = f->mesh()->get_bounding_box();
  if (!bbox.valid()) {
    warning("Input field is empty -- using unit cube.");
    bbox.extend(Point(0,0,0));
    bbox.extend(Point(1,1,1));
  }
  box_initial_bounds_ = bbox;

  // build a widget identical to the BBox
  size = Vector(bbox.max()-bbox.min());
  if (fabs(size.x())<1.e-4) {
    size.x(2.e-4); 
    bbox.extend(bbox.min()-Vector(1.e-4,0,0));
  }
  if (fabs(size.y())<1.e-4) {
    size.y(2.e-4); 
    bbox.extend(bbox.min()-Vector(0,1.e-4,0));
  }
  if (fabs(size.z())<1.e-4) {
    size.z(2.e-4); 
    bbox.extend(bbox.min()-Vector(0,0,1.e-4));
  }
  center = Point(bbox.min() + size/2.);



  Vector sizex(size.x(),0,0);
  Vector sizey(0,size.y(),0);
  Vector sizez(0,0,size.z());

  Point right(center + sizex/2.);
  Point down(center + sizey/2.);
  Point in(center +sizez/2.);

  l2norm = size.length();

  // Translate * Rotate * Scale.
  Transform r;
  Point unused;
  box_initial_transform_.load_identity();
  box_initial_transform_.pre_scale(Vector((right-center).length(),
			     (down-center).length(),
			     (in-center).length()));
  r.load_frame(unused, (right-center).normal(),
	       (down-center).normal(),
	       (in-center).normal());
  box_initial_transform_.pre_trans(r);
  box_initial_transform_.pre_translate(center.asVector());


  box_->SetScale(l2norm * 0.015);
  box_->SetPosition(center, right, down, in);

  GeomGroup *widget_group = scinew GeomGroup;
  widget_group->add(box_->GetWidget());

  GeometryOPort *ogport = (GeometryOPort*)get_oport("Transformation Widget");
  if (!ogport) {
    error("Unable to initialize oport 'Transformation Widget'.");
    return;
  }
  widgetid_ = ogport->addObj(widget_group,"ChangeFieldBounds Transform widget",
			     &widget_lock_);
  ogport->flushViews();
}

void
ChangeFieldBounds::execute()
{
  FieldIPort *iport = (FieldIPort*)get_iport("Input Field"); 
  if (!iport) {
    error("Unable to initialize iport 'Input Field'.");
    return;
  }
  
  // The input port (with data) is required.
  FieldHandle fh;
  if (!iport->get(fh) || !fh.get_rep())
  {
    clear_vals();
    return;
  }

  // The output port is required.
  FieldOPort *oport = (FieldOPort*)get_oport("Output Field");
  if (!oport) {
    error("Unable to initialize oport 'Output Field'.");
    return;
  }

  gui->execute(id + " set_state Executing 0");

  // build the transform widget and set the the initial
  // field transform.
  if (generation_ != fh.get_rep()->generation) 
  {
    generation_ = fh.get_rep()->generation;
    // get and display the attributes of the input field
    update_input_attributes(fh);
    build_widget(fh);
    BBox bbox = fh->mesh()->get_bounding_box();
    if (!bbox.valid()) {
      warning("Input field is empty -- using unit cube.");
      bbox.extend(Point(0,0,0));
      bbox.extend(Point(1,1,1));
    }
    Vector size(bbox.max()-bbox.min());
     if (fabs(size.x())<1.e-4) {
      size.x(2.e-4); 
      bbox.extend(bbox.min()-Vector(1.e-4,0,0));
    }
    if (fabs(size.y())<1.e-4) {
      size.y(2.e-4); 
      bbox.extend(bbox.min()-Vector(0,1.e-4,0));
    }
    if (fabs(size.z())<1.e-4) {
      size.z(2.e-4); 
      bbox.extend(bbox.min()-Vector(0,0,1.e-4));
    }
    Point center(bbox.min() + size/2.);
    Vector sizex(size.x(),0,0);
    Vector sizey(0,size.y(),0);
    Vector sizez(0,0,size.z());

    Point right(center + sizex/2.);
    Point down(center + sizey/2.);
    Point in(center +sizez/2.);

    //double l2norm(size.length());
    Transform r;
    Point unused;
    field_initial_transform_.load_identity();
    field_initial_transform_.pre_scale(Vector((right-center).length(),
					      (down-center).length(),
					      (in-center).length()));
    r.load_frame(unused, (right-center).normal(),
		 (down-center).normal(),
		 (in-center).normal());
    field_initial_transform_.pre_trans(r);
    field_initial_transform_.pre_translate(center.asVector());
  }

  if (useoutputsize_.get() || useoutputcenter_.get()) {
    Point center, right, down, in;
    outputcenterx_.reset(); outputcentery_.reset(); outputcenterz_.reset();
    outputsizex_.reset(); outputsizey_.reset(); outputsizez_.reset();
    if (outputsizex_.get() <= 0 || 
	outputsizey_.get() <= 0 || 
	outputsizez_.get() <= 0) {
      error("Degenerate BBox requested.");
      return;                    // degenerate 
    }
    Vector sizex, sizey, sizez;
    box_->GetPosition(center,right,down,in);
    if (useoutputsize_.get()) {
      sizex=Vector(outputsizex_.get(),0,0);
      sizey=Vector(0,outputsizey_.get(),0);
      sizez=Vector(0,0,outputsizez_.get());
    } else {
      sizex=(right-center)*2;
      sizey=(down-center)*2;
      sizez=(in-center)*2;
    }
    if (useoutputcenter_.get()) {
      center = Point(outputcenterx_.get(),
		     outputcentery_.get(),
		     outputcenterz_.get());
    }
    right = Point(center + sizex/2.);
    down = Point(center + sizey/2.);
    in = Point(center + sizez/2.);
    box_->SetPosition(center,right,down,in);
  }

  // Transform the mesh if necessary.
  // Translate * Rotate * Scale.
  Point center, right, down, in;
  box_->GetPosition(center, right, down, in);
  Transform t, r;
  Point unused;
  t.load_identity();
  t.pre_scale(Vector((right-center).length(),
       (down-center).length(),
       (in-center).length()));
  r.load_frame(unused, (right-center).normal(),
	 (down-center).normal(),
	 (in-center).normal());
  t.pre_trans(r);
  t.pre_translate(center.asVector());

  Transform inv(field_initial_transform_);
  inv.invert();
  t.post_trans(inv);

  // Change the input field handle here.
  fh.detach();
  fh->mesh_detach();
  fh->mesh()->transform(t);

  oport->send(fh);

  // The output port is required.
  MatrixOPort *moport = (MatrixOPort*)get_oport("Transformation Matrix");
  if (!moport) {
    error("Unable to initialize oport 'Transformation Matrix'.");
    return;
  }  

  // convert the transform into a matrix and send it out   
  DenseMatrix *matrix_transform = scinew DenseMatrix(t);
  MatrixHandle mh = matrix_transform;
  moport->send(mh);
}

    
void ChangeFieldBounds::widget_moved(bool last)
{
  if (last) {
    Point center, right, down, in;
    outputcenterx_.reset(); outputcentery_.reset(); outputcenterz_.reset();
    outputsizex_.reset(); outputsizey_.reset(); outputsizez_.reset();
    box_->GetPosition(center,right,down,in);
    outputcenterx_.set(center.x());
    outputcentery_.set(center.y());
    outputcenterz_.set(center.z());
    outputsizex_.set((right.x()-center.x())*2.);
    outputsizey_.set((down.y()-center.y())*2.);
    outputsizez_.set((in.z()-center.z())*2.);
    want_to_execute();
  }
}


} // End namespace SCIRun
