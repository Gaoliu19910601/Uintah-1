/*
 *  ShowField.cc
 *
 *  Written by:
 *   Martin Cole
 *   School of Computing
 *   University of Utah
 *   Aug 31, 2000
 *
 *  Copyright (C) 2000 SCI Group
 */
#include <Core/Malloc/Allocator.h>
#include <Dataflow/Network/Module.h>
#include <Core/Datatypes/Field.h>
#include <Core/Datatypes/TetVol.h>
#include <Core/Datatypes/LatticeVol.h>
#include <Core/Datatypes/FieldAlgo.h>

#include <Dataflow/Ports/ColorMapPort.h>
#include <Dataflow/Ports/GeometryPort.h>
#include <Dataflow/Ports/FieldPort.h>

#include <Core/Geom/GeomGroup.h>
#include <Core/Geom/Material.h>
#include <Core/Geom/Switch.h>
#include <Core/Geom/GeomSphere.h>
#include <Core/Geom/GeomLine.h>
#include <Core/Geom/GeomTri.h>
#include <Core/Geom/Pt.h>

#include <Core/GuiInterface/GuiVar.h>
#include <Core/Util/DebugStream.h>

#include <map.h>
#include <iostream>
using std::cerr;
using std::cin;
using std::endl;
#include <sstream>
using std::ostringstream;

namespace SCIRun {


class ShowField : public Module 
{
  
  //! Private Data
  DebugStream              dbg_;  

  //! input ports
  FieldIPort*              geom_;
  FieldIPort*              data_;
  ColorMapIPort*           color_;
  //! output port
  GeometryOPort           *ogeom_;  

  //! Scene graph ID's
  int                      node_id_;
  int                      edge_id_;
  int                      face_id_;

  //! top level nodes for switching on and off..
  //! nodes.
  GeomSwitch*              node_switch_;
  bool                     nodes_on_;
  //! edges.
  GeomSwitch*              edge_switch_;
  bool                     edges_on_;
  //! faces.
  GeomSwitch*              face_switch_;
  bool                     faces_on_;

  //! holds options for how to visualize nodes.
  GuiString                node_display_type_;
  GuiInt                   showProgress_;

  //! Private Methods
  template <class Field>
  bool render(Field *f, Field *f1, ColorMapHandle cm);

public:
  ShowField(const clString& id);
  virtual ~ShowField();
  virtual void execute();

  inline void add_sphere(const Point &p, double scale, GeomGroup *g, 
			 MaterialHandle m0);
  inline void add_axis(const Point &p, double scale, GeomGroup *g, 
		       MaterialHandle m0);
  inline void add_point(const Point &p, GeomPts *g, 
			MaterialHandle m0);
  inline void add_edge(const Point &p1, const Point &p2, double scale, 
		       GeomGroup *g, MaterialHandle m0);
  inline void add_face(const Point &p1, const Point &p2, const Point &p3, 
		       MaterialHandle m0, MaterialHandle m1, MaterialHandle m2,
		       GeomGroup *g);

  virtual void tcl_command(TCLArgs& args, void* userdata);

};

ShowField::ShowField(const clString& id) : 
  Module("ShowField", id, Filter), 
  dbg_("ShowField", true),
  geom_(0),
  data_(0),
  color_(0),
  ogeom_(0),
  node_id_(0),
  edge_id_(0),
  face_id_(0),
  node_switch_(0),
  nodes_on_(true),
  edge_switch_(0),
  edges_on_(true),
  face_switch_(0),
  faces_on_(true),
  node_display_type_("node_display_type", id, this),
  showProgress_("show_progress", id, this)
 {
  // Create the input ports
  geom_ = scinew FieldIPort(this, "Field-Geometry", FieldIPort::Atomic);
  add_iport(geom_);
  data_ = scinew FieldIPort(this, "Field-ColorIndex", FieldIPort::Atomic);
  add_iport(data_);
  color_ = scinew ColorMapIPort(this, "ColorMap", FieldIPort::Atomic);
  add_iport(color_);
    
  // Create the output port
  ogeom_ = scinew GeometryOPort(this, "Geometry", GeometryIPort::Atomic);
  add_oport(ogeom_);
 }

ShowField::~ShowField() {}

void 
ShowField::execute()
{

  // tell module downstream to delete everything we have sent it before.
  // This is typically viewer, it owns the scene graph memory we create here.
    
  ogeom_->delAll(); 
  FieldHandle geom_handle;
  geom_->get(geom_handle);
  if(!geom_handle.get_rep()){
    cerr << "No Geometry in port 1 field" << endl;
    return;
  }
  FieldHandle data_handle;
  data_->get(data_handle);
  if(!data_handle.get_rep()){
    cerr << "No Data in port 2 field" << endl;
    return;
  }
  ColorMapHandle color_handle;
  color_->get(color_handle);
  if(!color_handle.get_rep()){
    cerr << "No ColorMap in port 3 ColorMap" << endl;
    return;
  }

  bool error = false;
  string msg;
  string name = geom_handle->get_type_name(0);
  if (name == "TetVol") {
    if (geom_handle->get_type_name(1) == "double") {
      TetVol<double> *tv = 0;
      TetVol<double> *tv1 = 0;
      tv = dynamic_cast<TetVol<double>*>(geom_handle.get_rep());
      tv1 = dynamic_cast<TetVol<double>*>(data_handle.get_rep());
      if (tv && tv1) { 
	// need faces and edges.
	tv->finish_mesh();
	render(tv, tv1, color_handle); 
      }
      else { error = true; msg = "Not a valid TetVol."; }
    } else {
      error = true; msg ="TetVol of unknown type.";
    }
  } /*else if (name == "LatticeVol") {
    if (geom_handle->get_type_name(1) == "double") {
      LatticeVol<double> *lv = 0;
      lv = dynamic_cast<LatticeVol<double>*>(geom_handle.get_rep());
      if (lv) { render(lv); }
      else { error = true; msg = "Not a valid LatticeVol."; }
    } else {
      error = true; msg ="LatticeVol of unknown type.";
    }
    }*/ else if (error) {
    cerr << "ShowField Error: " << msg << endl;
    return;
  }

  if (nodes_on_) node_id_ = ogeom_->addObj(node_switch_, "Nodes");
  if (edges_on_) edge_id_ = ogeom_->addObj(edge_switch_, "Edges");
  if (faces_on_) face_id_ = ogeom_->addObj(face_switch_, "Faces");
  ogeom_->flushViews();
}

template <class Field>
bool
ShowField::render(Field *geom, Field *c_index, ColorMapHandle cm) 
{
  typename Field::mesh_handle_type mesh = geom->get_typed_mesh();

  if ((nodes_on_)) { // && (node_id_ == 0)) {
    GeomGroup* nodes = scinew GeomGroup;
    node_switch_ = scinew GeomSwitch(nodes);
    GeomPts *pts = 0;
    node_display_type_.reset();
    if (node_display_type_.get() == "Points") {
      pts = scinew GeomPts(mesh->nodes_size());
    }
    // First pass over the nodes
    typename Field::mesh_type::node_iterator niter = mesh->node_begin();  
    while (niter != mesh->node_end()) {
    
      Point p;
      mesh->get_point(p, *niter);
      double val;
      if (! c_index->value(val, *niter)) { return false; }

      if (node_display_type_.get() == "Spheres") {
	add_sphere(p, 0.03, nodes, cm->lookup(val));
      } else if (node_display_type_.get() == "Axes") {
	add_axis(p, 0.03, nodes, cm->lookup(val));
      } else {
	add_point(p, pts, cm->lookup(val));
      }
      ++niter;
    }
    if (node_display_type_.get() == "Points") {
      nodes->add(pts);
    }
  }

  if ((edges_on_)) { // && (edge_id_ == 0)) {
    GeomGroup* edges = scinew GeomGroup;
    edge_switch_ = scinew GeomSwitch(edges);
    // Second pass over the edges
    //mesh->compute_edges();
    typename Field::mesh_type::edge_iterator eiter = mesh->edge_begin();  
    while (eiter != mesh->edge_end()) {        
      typename Field::mesh_type::node_array nodes;
      mesh->get_nodes(nodes, *eiter); ++eiter;
    
      Point p1, p2;
      mesh->get_point(p1, nodes[0]);
      mesh->get_point(p2, nodes[1]);
      double val1;
      if (! c_index->value(val1, nodes[0])) { return false; }
      double val2;
      if (! c_index->value(val2, nodes[1])) { return false; } 
      val1 = (val1 + val2) * .5;
      add_edge(p1, p2, 1.0, edges, cm->lookup(val1));
    }
  }

  if ((faces_on_)) {// && (face_id_ == 0)) {
    GeomGroup* faces = scinew GeomGroup;
    face_switch_ = scinew GeomSwitch(faces);
    // Second pass over the faces
    //mesh->compute_faces();
    typename Field::mesh_type::face_iterator fiter = mesh->face_begin();  
    while (fiter != mesh->face_end()) {        
      typename Field::mesh_type::node_array nodes;
      mesh->get_nodes(nodes, *fiter); ++fiter;
      
      Point p1, p2, p3;
      mesh->get_point(p1, nodes[0]);
      mesh->get_point(p2, nodes[1]);
      mesh->get_point(p3, nodes[2]);
      double val1;
      if (! c_index->value(val1, nodes[0])) { return false; }
      double val2;
      if (! c_index->value(val2, nodes[1])) { return false; } 
      double val3;
      if (! c_index->value(val3, nodes[2])) { return false; } 
      add_face(p1, p2, p3, cm->lookup(val1), cm->lookup(val2), 
	       cm->lookup(val3), faces);
    }
  }
}

void 
ShowField::add_face(const Point &p0, const Point &p1, const Point &p2, 
		    MaterialHandle m0, MaterialHandle m1, MaterialHandle m2,
		    GeomGroup *g) 
{
  GeomTri *t = new GeomTri(p0, p1, p2, m0, m1, m2);
  g->add(t);
}

void 
ShowField::add_edge(const Point &p0, const Point &p1,  
		    double scale, GeomGroup *g, MaterialHandle mh) {
  GeomLine *l = new GeomLine(p0, p1);
  l->setLineWidth(scale);
  g->add(scinew GeomMaterial(l, mh));
}

void 
ShowField::add_sphere(const Point &p0, double scale, 
		      GeomGroup *g, MaterialHandle mh) {
  GeomSphere *s = scinew GeomSphere(p0, scale, 8, 4);
  g->add(scinew GeomMaterial(s, mh));
}

void 
ShowField::add_axis(const Point &p0, double scale, 
		    GeomGroup *g, MaterialHandle mh) 
{
  static const Vector x(1., 0., 0.);
  static const Vector y(0., 1., 0.);
  static const Vector z(0., 0., 1.);

  Point p1 = p0 + x * scale;
  Point p2 = p0 - x * scale;
  GeomLine *l = new GeomLine(p1, p2);
  l->setLineWidth(3.0);
  g->add(scinew GeomMaterial(l, mh));
  p1 = p0 + y * scale;
  p2 = p0 - y * scale;
  l = new GeomLine(p1, p2);
  l->setLineWidth(3.0);
  g->add(scinew GeomMaterial(l, mh));
  p1 = p0 + z * scale;
  p2 = p0 - z * scale;
  l = new GeomLine(p1, p2);
  l->setLineWidth(3.0);
  g->add(scinew GeomMaterial(l, mh));
}

void 
ShowField::add_point(const Point &p, GeomPts *pts, MaterialHandle mh) {
  pts->add(p, mh->diffuse);
}

void 
ShowField::tcl_command(TCLArgs& args, void* userdata) {
  if(args.count() < 2){
    args.error("ShowField needs a minor command");
    return;
  }
  dbg_ << "tcl_command: " << args[1] << endl;

  if (args[1] == "node_display_type") {
    // toggle spheresP
    // Call reset so that we really get the value, not the cached one.
    node_display_type_.reset();
    if (node_display_type_.get() == "Spheres") {
      dbg_ << "Render Spheres." << endl;
    } else if (node_display_type_.get() == "Axes") {
      dbg_ << "Render Axes." << endl;
    } else {
      dbg_ << "Render Points." << endl;
    }
    // Tell viewer to redraw itself.
    ogeom_->flushViews();

  } else if (args[1] == "toggle_display_nodes"){
    // Toggle the GeomSwitch.
    nodes_on_ = ! nodes_on_;
    if (node_switch_) node_switch_->set_state(nodes_on_);
    // Tell viewer to redraw itself.
    ogeom_->flushViews();
  } else if (args[1] == "toggle_display_edges"){
    // Toggle the GeomSwitch.
    edges_on_ = ! edges_on_;
    if (edge_switch_) edge_switch_->set_state(edges_on_);
    // Tell viewer to redraw itself.
    ogeom_->flushViews();
  } else if (args[1] == "toggle_display_faces"){
    // Toggle the GeomSwitch.
    faces_on_ = ! faces_on_;
    if (face_switch_) face_switch_->set_state(faces_on_);
    // Tell viewer to redraw itself.
    ogeom_->flushViews();
  } else {
    Module::tcl_command(args, userdata);
  }
}

extern "C" Module* make_ShowField(const clString& id) {
  return new ShowField(id);
}

} // End namespace SCIRun


