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
 *  StructCurveMesh.h: Templated Mesh defined on a 1D Structured Grid
 *
 *  Written by:
 *   Allen R. Sanderson
 *   Department of Computer Science
 *   University of Utah
 *   November 2002
 *
 *  Copyright (C) 2002 SCI Group
 *
 */

/*
  A sturctured curve is a dataset with regular topology but with irregular geometry.
  The line defined may have any shape but can not be overlapping or self-intersecting.
  
  The topology of structured curve is represented using a 1D vector with
  the points being stored in an index based array. The ordering of the curve is
  implicity defined based based upon its indexing.

  For more information on datatypes see Schroeder, Martin, and Lorensen,
  "The Visualization Toolkit", Prentice Hall, 1998.
 */

#ifndef SCI_project_StructCurveMesh_h
#define SCI_project_StructCurveMesh_h 1

#include <Core/Datatypes/ScanlineMesh.h>
#include <Core/Containers/Array1.h>
#include <Core/Geometry/Point.h>

namespace SCIRun {

using std::string;

class SCICORESHARE StructCurveMesh : public ScanlineMesh
{
public:
  StructCurveMesh() {}
  StructCurveMesh(unsigned int n);
  StructCurveMesh(const StructCurveMesh &copy);
  virtual StructCurveMesh *clone() { return new StructCurveMesh(*this); }
  virtual ~StructCurveMesh() {}

  //! get the mesh statistics
  double get_cord_length() const;
  virtual BBox get_bounding_box() const;
  virtual void transform(Transform &t);

  //! get the child elements of the given index
  void get_nodes(Node::array_type &, Edge::index_type) const;
  void get_nodes(Node::array_type &, Face::index_type) const {}
  void get_nodes(Node::array_type &, Cell::index_type) const {}
  void get_edges(Edge::array_type &, Face::index_type) const {}
  void get_edges(Edge::array_type &, Cell::index_type) const {}
  //void get_faces(Face::array_type &, Cell::index_type) const {}

  //! get the parent element(s) of the given index
  void get_edges(Edge::array_type &a, Node::index_type idx) const
  { a.push_back(Edge::index_type(idx));}
  //bool get_faces(Face::array_type &, Node::index_type) const { return 0; }
  //bool get_faces(Face::array_type &, Edge::index_type) const { return 0; }
  //bool get_cells(Cell::array_type &, Node::index_type) const { return 0; }
  //bool get_cells(Cell::array_type &, Edge::index_type) const { return 0; }
  //bool get_cells(Cell::array_type &, Face::index_type) const { return 0; }

  //! return all edge_indecies that overlap the BBox in arr.
  void get_edges(Edge::array_type &arr, const BBox &box) const
  { ASSERTFAIL("ScanlineMesh::get_edges for BBox is not implemented."); }

  //! similar to get_cells() with Face::index_type argument, but
  //  returns the "other" cell if it exists, not all that exist
  bool get_neighbor(Cell::index_type & /*neighbor*/, Cell::index_type /*from*/,
		    Face::index_type /*idx*/) const {
    ASSERTFAIL("StructCurveMesh::get_neighbor not implemented.");
  }

  //! get the center point (in object space) of an element
  void get_center(Point &, const Node::index_type &) const;
  void get_center(Point &, const Edge::index_type &) const;
  void get_center(Point &, const Face::index_type &) const {}
  void get_center(Point &, const Cell::index_type &) const {}

  bool locate(Node::index_type &, const Point &) const;
  bool locate(Edge::index_type &, const Point &) const;
  bool locate(Face::index_type &, const Point &) const { return false; }
  bool locate(Cell::index_type &, const Point &) const { return false; }

  void get_weights(const Point &, Node::array_type &, vector<double> &);
  void get_weights(const Point &, Edge::array_type &, vector<double> &);
  void get_weights(const Point &, Face::array_type &, vector<double> &)
  {ASSERTFAIL("StructCurveMesh::get_weights for faces isn't supported");}
  void get_weights(const Point &, Cell::array_type &, vector<double> &)
  {ASSERTFAIL("StructCurveMesh::get_weights for cells isn't supported");}

  void get_point(Point &point, Node::index_type &index) const
  { get_center(point,index); }
  void set_point(Node::index_type &index, const Point &point )
  { points_[index] = point; }

  void get_normal(Vector &vector, Node::index_type &index) const
  { ASSERTFAIL("not implemented") }

  virtual bool is_editable() const { return true; }
    
  virtual void io(Piostream&);
  static PersistentTypeID type_id;
  static  const string type_name(int n = -1);
  virtual const TypeDescription *get_type_description() const;

private:

  //! the points
  Array1<Point> points_;

  // returns a StructCurveMesh
  static Persistent *maker() { return new StructCurveMesh(); }
};

typedef LockingHandle<StructCurveMesh> StructCurveMeshHandle;

const TypeDescription* get_type_description(StructCurveMesh *);
const TypeDescription* get_type_description(StructCurveMesh::Node *);
const TypeDescription* get_type_description(StructCurveMesh::Edge *);
const TypeDescription* get_type_description(StructCurveMesh::Face *);
const TypeDescription* get_type_description(StructCurveMesh::Cell *);

} // namespace SCIRun

#endif // SCI_project_StructCurveMesh_h
