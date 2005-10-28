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
 *  StructQuadSurfMesh.h: Templated Mesh defined on a 2D Structured Grid
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
  A structured grid is a dataset with regular topology but with irregular 
  geometry.  The grid may have any shape but can not be overlapping or 
  self-intersecting. The topology of a structured grid is represented 
  using a 2D, or 3D vector with the points being stored in an index 
  based array. The faces (quadrilaterals) and  cells (Hexahedron) are 
  implicitly define based based upon their indexing.

  Structured grids are typically used in finite difference analysis.

  For more information on datatypes see Schroeder, Martin, and Lorensen,
  "The Visualization Toolkit", Prentice Hall, 1998.
*/

#ifndef SCI_project_StructQuadSurfMesh_h
#define SCI_project_StructQuadSurfMesh_h 1

#include <Core/Datatypes/ImageMesh.h>
#include <Core/Containers/Array2.h>
#include <Core/Geometry/Point.h>
#include <Core/Geometry/BBox.h>
#include <Core/Geometry/CompGeom.h>
#include <sgi_stl_warnings_off.h>
#include <vector>
#include <sgi_stl_warnings_on.h>

namespace SCIRun {

using std::string;

template <class Basis>
class StructQuadSurfMesh : public ImageMesh<Basis>
{
public:
  StructQuadSurfMesh();
  StructQuadSurfMesh(unsigned int x, unsigned int y);
  StructQuadSurfMesh(const StructQuadSurfMesh<Basis> &copy);
  virtual StructQuadSurfMesh *clone() 
  { return new StructQuadSurfMesh<Basis>(*this); }
  virtual ~StructQuadSurfMesh() {}

  //! get the mesh statistics
  virtual BBox get_bounding_box() const;
  virtual void transform(const Transform &t);

  bool get_dim(vector<unsigned int> &array) const;
  void set_dim(vector<unsigned int> dims) {
    ImageMesh<Basis>::set_dim(dims);
    points_.resize(dims[0], dims[1]);
    normals_.resize(dims[0], dims[1]);
  }

  //! Get the size of an elemnt (length, area, volume)
  double get_size(const typename ImageMesh<Basis>::Node::index_type &) const
  { return 0.0; }
  double get_size(typename ImageMesh<Basis>::Edge::index_type idx) const 
  {
    typename ImageMesh<Basis>::Node::array_type arr;
    get_nodes(arr, idx);
    Point p0, p1;
    get_center(p0, arr[0]);
    get_center(p1, arr[1]);
    return (p1.asVector() - p0.asVector()).length();
  }
  double get_size(const typename ImageMesh<Basis>::Face::index_type &idx) const
  {
    typename ImageMesh<Basis>::Node::array_type ra;
    get_nodes(ra,idx);
    Point p0,p1,p2;
    get_point(p0,ra[0]);
    get_point(p1,ra[1]);
    get_point(p2,ra[2]);
    return (Cross(p0-p1,p2-p0)).length()*0.5;
  }

  double get_size(typename ImageMesh<Basis>::Cell::index_type) const 
  { return 0.0; }
  double get_length(typename ImageMesh<Basis>::Edge::index_type idx) const 
  { return get_size(idx); }
  double get_area(const typename ImageMesh<Basis>::Face::index_type &idx) const
  { return get_size(idx); }
  double get_volume(typename ImageMesh<Basis>::Cell::index_type idx) const 
  { return get_size(idx); }

  void get_normal(Vector &,
		  const typename ImageMesh<Basis>::Node::index_type &) const;

  //! get the center point (in object space) of an element
  void get_center(Point &,
		  const typename ImageMesh<Basis>::Node::index_type &) const;
  void get_center(Point &,
		  typename ImageMesh<Basis>::Edge::index_type) const;
  void get_center(Point &,
		  const typename ImageMesh<Basis>::Face::index_type &) const;
  void get_center(Point &,
		  typename ImageMesh<Basis>::Cell::index_type) const {}

  bool locate(typename ImageMesh<Basis>::Node::index_type &, 
	      const Point &) const;
  bool locate(typename ImageMesh<Basis>::Edge::index_type &, 
	      const Point &) const;
  bool locate(typename ImageMesh<Basis>::Face::index_type &, 
	      const Point &) const;
  bool locate(typename ImageMesh<Basis>::Cell::index_type &, 
	      const Point &) const;

  int get_weights(const Point &p, 
		  typename ImageMesh<Basis>::Node::array_type &l, double *w);
  int get_weights(const Point & , 
		  typename ImageMesh<Basis>::Edge::array_type & , double * )
  { ASSERTFAIL("StructQuadSurfMesh::get_weights for edges isn't supported"); }
  int get_weights(const Point &p, 
		  typename ImageMesh<Basis>::Face::array_type &l, double *w);
  int get_weights(const Point & , 
		  typename ImageMesh<Basis>::Cell::array_type & , double * )
  { ASSERTFAIL("StructQuadSurfMesh::get_weights for cells isn't supported"); }


  bool inside3_p(typename ImageMesh<Basis>::Face::index_type i, 
		 const Point &p) const;


  void get_point(Point &point,
		 const typename ImageMesh<Basis>::Node::index_type &index) const
  { get_center(point, index); }
  void set_point(const Point &point,
		 const typename ImageMesh<Basis>::Node::index_type &index);

  void get_random_point(Point &,
			const typename ImageMesh<Basis>::Elem::index_type &,
			int) const
  { ASSERTFAIL("not implemented") }

  class ElemData 
  {
  public:
    ElemData(const StructQuadSurfMesh<Basis>& msh, 
	     const typename Elem::index_type ind) :
      mesh_(msh),
      index_(ind)
    {}
    
    // the following designed to coordinate with ::get_nodes
    inline 
    unsigned node0_index() const {
      return (index_.i_ + mesh_.get_ni()*index_.j_);
    }
    inline 
    unsigned node1_index() const {
      return (index_.i_+ 1 + mesh_.get_ni()*index_.j_);
    }
    inline 
    unsigned node2_index() const {
      return (index_.i_ + 1 + mesh_.get_ni()*(index_.j_ + 1));
      
    }
    inline 
    unsigned node3_index() const {
      return (index_.i_ + mesh_.get_ni()*(index_.j_ + 1));
    }

    // the following designed to coordinate with ::get_edges
    inline 
    unsigned edge0_index() const {
      return index_.i_ + index_.j_ * (mesh_.ni_- 1); 
    }
    inline 
    unsigned edge1_index() const {
      return index_.i_ + (index_.j_ + 1) * (mesh_.ni_ - 1);
    }
    inline 
    unsigned edge2_index() const {
      return index_.i_    *(mesh_.nj_ - 1) + index_.j_ + 
	((mesh_.ni_ - 1) * mesh_.nj_);
     }
    inline 
    unsigned edge3_index() const {
      return (index_.i_ + 1) * (mesh_.nj_ - 1) + index_.j_ + 
	((mesh_.ni_ - 1) * mesh_.nj_);
    }


    inline 
    const Point node0() const {
      return mesh_.points_(index_.i_, index_.j_);
    }
    inline 
    const Point node1() const {
      return mesh_.points_(index_.i_+1, index_.j_);
    }
    inline 
    const Point node2() const {
      return mesh_.points_(index_.i_+1, index_.j_+1);
    }
    inline 
    const Point node3() const {
      return mesh_.points_(index_.i_, index_.j_+1);
    }
    inline 
    const Point node4() const {
      return mesh_.points_(index_.i_, index_.j_);
    }

  private:
    const StructQuadSurfMesh<Basis>          &mesh_;
    const typename ImageMesh<Basis>::Elem::index_type  index_;
  };

  friend class ElemData;

  //! Generate the list of points that make up a sufficiently accurate
  //! piecewise linear approximation of an edge.
  void pwl_approx_edge(vector<vector<double> > &coords, 
		       typename Elem::index_type ci, 
		       typename Edge::index_type ei, 
		       unsigned div_per_unit) const
  {    
    // Needs to match unit_edges in Basis/QuadBilinearLgn.cc 
    // compare get_nodes order to the basis order
    int basis_emap[] = {0, 2, 3, 1}; 
    typename Edge::array_type edges;
    get_edges(edges, ci);
    unsigned count = 0;
    typename Edge::array_type::iterator iter = edges.begin();
    while (iter != edges.end()) {
      if (ei == *iter++) break;
      ++count;
    }

    basis_.approx_edge(basis_emap[count], div_per_unit, coords); 
  }


  bool get_coords(vector<double> &coords, 
		  const Point &p,
		  typename Elem::index_type idx) const
  {
    ElemData ed(*this, idx);
    return this->basis_.get_coords(coords, p, ed); 
  }
  
  void interpolate(Point &pt, const vector<double> &coords, 
		   typename Elem::index_type idx) const
  {
    ElemData ed(*this, idx);
    pt = this->basis_.interpolate(coords, ed);
  }

  // get the Jacobian matrix
  void derivate(const vector<double> &coords, 
		typename Elem::index_type idx, 
		vector<Point> &J) const
  {
    ElemData ed(*this, idx);
    this->basis_.derivate(coords, ed, J);
  }


  virtual bool is_editable() const { return false; }

  virtual void io(Piostream&);
  static PersistentTypeID type_id;
  static  const string type_name(int n = -1);
  virtual const TypeDescription *get_type_description() const;

  virtual bool synchronize(unsigned int);
  
  static const TypeDescription* node_type_description();
  static const TypeDescription* edge_type_description();
  static const TypeDescription* face_type_description();
  static const TypeDescription* cell_type_description();
  static const TypeDescription* elem_type_description() 
  { return face_type_description(); }

  // returns a StructQuadSurfMesh
  static Persistent *maker() { return new StructQuadSurfMesh<Basis>(); }

protected:
  void compute_normals();
  void compute_edge_neighbors(double err = 1.0e-8);

  const Point &point(const typename ImageMesh<Basis>::Node::index_type &idx)
  { return points_(idx.i_, idx.j_); }

  int next(int i) { return ((i%4)==3) ? (i-3) : (i+1); }
  int prev(int i) { return ((i%4)==0) ? (i+3) : (i-1); }

  Array2<Point>  points_;
  Array2<Vector> normals_; //! normalized per node
  Mutex		 normal_lock_;
  unsigned int   synchronized_;
};

template <class Basis>
PersistentTypeID 
StructQuadSurfMesh<Basis>::type_id(StructQuadSurfMesh<Basis>::type_name(-1), 
				   "Mesh", maker);

template <class Basis>
StructQuadSurfMesh<Basis>::StructQuadSurfMesh()
  : normal_lock_("StructQuadSurfMesh Normals Lock"),
    synchronized_(ImageMesh<Basis>::ALL_ELEMENTS_E)
{
}

template <class Basis>  
StructQuadSurfMesh<Basis>::StructQuadSurfMesh(unsigned int x, unsigned int y)
  : ImageMesh<Basis>(x, y, Point(0.0, 0.0, 0.0), Point(1.0, 1.0, 1.0)),
    points_(x, y),
    normals_(x,y),
    normal_lock_("StructQuadSurfMesh Normals Lock"),
    synchronized_(ImageMesh<Basis>::ALL_ELEMENTS_E)
{
}

template <class Basis>
StructQuadSurfMesh<Basis>::StructQuadSurfMesh(const StructQuadSurfMesh &copy)
  : ImageMesh<Basis>(copy),
    normal_lock_("StructQuadSurfMesh Normals Lock"),
    synchronized_(copy.synchronized_)
{
  points_.copy( copy.points_ );
  normals_.copy( copy.normals_ );
}

template <class Basis>
bool
StructQuadSurfMesh<Basis>::get_dim(vector<unsigned int> &array) const
{
  array.resize(2);
  array.clear();

  array.push_back(this->ni_);
  array.push_back(this->nj_);

  return true;
}

template <class Basis>
BBox
StructQuadSurfMesh<Basis>::get_bounding_box() const
{
  BBox result;

  typename ImageMesh<Basis>::Node::iterator ni, nie;
  begin(ni);
  end(nie);
  while (ni != nie)
  {
    Point p;
    get_center(p, *ni);
    result.extend(p);
    ++ni;
  }
  return result;
}

template <class Basis>
void
StructQuadSurfMesh<Basis>::transform(const Transform &t)
{
  typename ImageMesh<Basis>::Node::iterator i, ie;
  begin(i);
  end(ie);

  while (i != ie) {
    points_((*i).i_,(*i).j_) = t.project(points_((*i).i_,(*i).j_));

    ++i;
  }
}

#if 0
template <class Basis>
void
StructQuadSurfMesh<Basis>::get_nodes(typename ImageMesh<Basis>::Node::array_type &array, 
				     typename ImageMesh<Basis>::Edge::index_type idx) const
{
  array.resize(2);

  const int yidx = idx - (this->ni_-1) * this->nj_;
  if (yidx >= 0)
  {
    const int i = yidx / (this->nj_ - 1);
    const int j = yidx % (this->nj_ - 1);
    array[0] = typename ImageMesh<Basis>::Node::index_type(this, i, j);
    array[1] = typename ImageMesh<Basis>::Node::index_type(this, i, j+1);
  }
  else
  {
    const int i = idx % (this->ni_ - 1);
    const int j = idx / (this->ni_ - 1);
    array[0] = typename ImageMesh<Basis>::Node::index_type(this, i, j);
    array[1] = typename ImageMesh<Basis>::Node::index_type(this, i+1, j);
  }
}

template <class Basis>
void
StructQuadSurfMesh<Basis>::get_nodes(typename ImageMesh<Basis>::Node::array_type &array, 
				   const typename ImageMesh<Basis>::Face::index_type &idx) const
{
  const int arr_size = 4;
  array.resize(arr_size);

  for (int i = 0; i < arr_size; i++)
    array[i].mesh_ = idx.mesh_;

  array[0].i_ = idx.i_;   array[0].j_ = idx.j_;
  array[1].i_ = idx.i_+1; array[1].j_ = idx.j_;
  array[2].i_ = idx.i_+1; array[2].j_ = idx.j_+1;
  array[3].i_ = idx.i_;   array[3].j_ = idx.j_+1;
}

template <class Basis>
void
StructQuadSurfMesh<Basis>::get_edges(typename ImageMesh<Basis>::Edge::array_type &array,
				   const typename ImageMesh<Basis>::Face::index_type &idx) const
{
  array.clear();
  array.push_back(idx * 4 + 0);
  array.push_back(idx * 4 + 1);
  array.push_back(idx * 4 + 2);
  array.push_back(idx * 4 + 3);
}

#endif

template <class Basis>
void
StructQuadSurfMesh<Basis>::get_normal(Vector &result,
				  const typename ImageMesh<Basis>::Node::index_type &idx ) const
{
  result = normals_(idx.i_, idx.j_);
}

template <class Basis>
void
StructQuadSurfMesh<Basis>::get_center(Point &result,
				    const typename ImageMesh<Basis>::Node::index_type &idx) const
{
  result = points_(idx.i_, idx.j_);
}

template <class Basis>
void
StructQuadSurfMesh<Basis>::get_center(Point &result, 
		       typename ImageMesh<Basis>::Edge::index_type idx) const
{
  typename ImageMesh<Basis>::Node::array_type arr;
  get_nodes(arr, idx);
  Point p1;
  get_center(result, arr[0]);
  get_center(p1, arr[1]);

  result.asVector() += p1.asVector();
  result.asVector() *= 0.5;
}


template <class Basis>
void
StructQuadSurfMesh<Basis>::get_center(Point &p,
		 const typename ImageMesh<Basis>::Face::index_type &idx) const
{
  typename ImageMesh<Basis>::Node::array_type nodes;
  get_nodes(nodes, idx);
  ASSERT(nodes.size() == 4);
  typename ImageMesh<Basis>::Node::array_type::iterator nai = nodes.begin();
  get_point(p, *nai);
  ++nai;
  Point pp;
  while (nai != nodes.end())
  {
    get_point(pp, *nai);
    p.asVector() += pp.asVector();
    ++nai;
  }
  p.asVector() *= (1.0 / 4.0);
}


template <class Basis>
bool
StructQuadSurfMesh<Basis>::locate(
                            typename ImageMesh<Basis>::Node::index_type &node,
			    const Point &p) const
{
  node.mesh_ = this;
  typename ImageMesh<Basis>::Face::index_type fi;
  if (locate(fi, p)) { // first try the fast way.
    typename ImageMesh<Basis>::Node::array_type nodes;
    get_nodes(nodes, fi);

    double dmin = (p-points_(nodes[0].i_, nodes[0].j_)).length2();
    node = nodes[0];
    for (unsigned int i = 1; i < nodes.size(); i++)
    {
      const double d = (p-points_(nodes[i].i_, nodes[i].j_)).length2();
      if (d < dmin)
      {
	dmin = d;
	node = nodes[i];
      }
    }
    return true;
  }
  else
  {  // do exhaustive search.
    typename ImageMesh<Basis>::Node::iterator ni, nie;
    begin(ni);
    end(nie);
    if (ni == nie) { return false; }

    double min_dist = (p-points_((*ni).i_, (*ni).j_)).length2();
    node = *ni;
    ++ni;

    while (ni != nie)
    {
      const double dist = (p-points_((*ni).i_, (*ni).j_)).length2();
      if (dist < min_dist)
      {
	node = *ni;
	min_dist = dist;
      }
      ++ni;
    }
    return true;
  }
}

template <class Basis>
bool
StructQuadSurfMesh<Basis>::locate(
			    typename ImageMesh<Basis>::Edge::index_type &loc, 
			    const Point &p) const
{
  typename ImageMesh<Basis>::Edge::iterator bi, ei;
  typename ImageMesh<Basis>::Node::array_type nodes;
  begin(bi);
  end(ei);
  loc = 0;
  
  bool found = false;
  double mindist = 0.0;
  while (bi != ei)
  {
    get_nodes(nodes,*bi);

    Point p0, p1;
    get_center(p0, nodes[0]);
    get_center(p1, nodes[1]);

    const double dist = distance_to_line2(p, p0, p1);
    if (!found || dist < mindist)
    {
      loc = *bi;
      mindist = dist;
      found = true;
    }
    ++bi;
  }
  return found;
}

template <class Basis>
int
StructQuadSurfMesh<Basis>::get_weights(const Point &p, 
			       typename ImageMesh<Basis>::Face::array_type &l, 
				       double *w)
{
  typename ImageMesh<Basis>::Face::index_type idx;
  if (locate(idx, p))
  {
    l.resize(1);
    l[0] = idx;
    w[0] = 1.0;
    return 1;
  }
  return 0;
}

template <class Basis>
int 
StructQuadSurfMesh<Basis>::get_weights(const Point &p, 
			       typename ImageMesh<Basis>::Node::array_type &l, 
				       double *w)
{
  typename ImageMesh<Basis>::Face::index_type idx;
  if (locate(idx, p))
  {
    get_nodes(l,idx);
    vector<double> coords(2);
    if (get_coords(coords, p, idx)) {
      return this->basis_.get_weights(coords, w);
    }
  }
  return 0;
}


template <class Basis>
bool 
StructQuadSurfMesh<Basis>::inside3_p(
			     typename ImageMesh<Basis>::Face::index_type i, 
			     const Point &p) const
{
  typename ImageMesh<Basis>::Node::array_type nodes;
  get_nodes(nodes, i);

  unsigned int n = nodes.size();

  Point * pts = new Point[n];
  
  for (unsigned int i = 0; i < n; i++) {
    get_center(pts[i], nodes[i]);
  }

  for (unsigned int i = 0; i < n; i+=2) {
    Point p0 = pts[(i+0)%n];
    Point p1 = pts[(i+1)%n];
    Point p2 = pts[(i+2)%n];

    Vector v01(p0-p1);
    Vector v02(p0-p2);
    Vector v0(p0-p);
    Vector v1(p1-p);
    Vector v2(p2-p);
    const double a = Cross(v01, v02).length(); // area of the whole triangle (2x)
    const double a0 = Cross(v1, v2).length();  // area opposite p0
    const double a1 = Cross(v2, v0).length();  // area opposite p1
    const double a2 = Cross(v0, v1).length();  // area opposite p2
    const double s = a0+a1+a2;

    // If the area of any of the sub triangles is very small then the point
    // is on the edge of the subtriangle.
    // TODO : How small is small ???
//     if( a0 < MIN_ELEMENT_VAL ||
// 	a1 < MIN_ELEMENT_VAL ||
// 	a2 < MIN_ELEMENT_VAL )
//       return true;

    // For the point to be inside a CONVEX quad it must be inside one
    // of the four triangles that can be formed by using three of the
    // quad vertices and the point in question.
    if( fabs(s - a) < ImageMesh<Basis>::MIN_ELEMENT_VAL && a > 
	ImageMesh<Basis>::MIN_ELEMENT_VAL ) {
      delete [] pts;
      return true;
    }
  }
  delete [] pts;
  return false;
}

template <class Basis>
bool
StructQuadSurfMesh<Basis>::locate(
			    typename ImageMesh<Basis>::Face::index_type &face, 
			    const Point &p) const
{
  typename ImageMesh<Basis>::Face::iterator bi, ei;
  begin(bi);
  end(ei);

  while (bi != ei) {
    if( inside3_p( *bi, p ) ) {
      face = *bi;
      return true;
    }

    ++bi;
  }
  return false;
}


template <class Basis>
bool
StructQuadSurfMesh<Basis>::locate(
			    typename ImageMesh<Basis>::Cell::index_type &loc, 
			    const Point &) const
{
  loc = 0;
  return false;
}


template <class Basis>
void
StructQuadSurfMesh<Basis>::set_point(const Point &point, 
				     const typename ImageMesh<Basis>::Node::index_type &index)
{
  points_(index.i_, index.j_) = point;
}

template <class Basis>
bool
StructQuadSurfMesh<Basis>::synchronize(unsigned int tosync)
{
  if (tosync & Mesh::NORMALS_E && !(synchronized_ & Mesh::NORMALS_E))
    compute_normals();
  return true;
}

template <class Basis>
void
StructQuadSurfMesh<Basis>::compute_normals()
{
  normal_lock_.lock();
  if (synchronized_ & Mesh::NORMALS_E) {
    normal_lock_.unlock();
    return;
  }
  normals_.resize(points_.dim1(), points_.dim2()); // 1 per node

  // build table of faces that touch each node
  Array2< vector<typename ImageMesh<Basis>::Face::index_type> >
    node_in_faces(points_.dim1(), points_.dim2());

  //! face normals (not normalized) so that magnitude is also the area.
  Array2<Vector> face_normals((points_.dim1()-1),(points_.dim2()-1));

  // Computing normal per face.
  typename ImageMesh<Basis>::Node::array_type nodes(4);
  typename ImageMesh<Basis>::Face::iterator iter, iter_end;
  begin(iter);
  end(iter_end);
  while (iter != iter_end)
  {
    get_nodes(nodes, *iter);

    Point p0, p1, p2, p3;
    get_point(p0, nodes[0]);
    get_point(p1, nodes[1]);
    get_point(p2, nodes[2]);
    get_point(p3, nodes[3]);

    // build table of faces that touch each node
    node_in_faces(nodes[0].i_,nodes[0].j_).push_back(*iter);
    node_in_faces(nodes[1].i_,nodes[1].j_).push_back(*iter);
    node_in_faces(nodes[2].i_,nodes[2].j_).push_back(*iter);
    node_in_faces(nodes[3].i_,nodes[3].j_).push_back(*iter);

    Vector v0 = p1 - p0;
    Vector v1 = p2 - p1;
    Vector n = Cross(v0, v1);
    face_normals((*iter).i_, (*iter).j_) = n;

    ++iter;
  }

  //Averaging the normals.
  typename ImageMesh<Basis>::Node::iterator nif_iter, nif_iter_end;
  begin( nif_iter );
  end( nif_iter_end );

  while (nif_iter != nif_iter_end) {
    vector<typename ImageMesh<Basis>::Face::index_type> v = node_in_faces((*nif_iter).i_, 
							(*nif_iter).j_);
    typename vector<typename ImageMesh<Basis>::Face::index_type>::const_iterator fiter = 
      v.begin();
    Vector ave(0.L,0.L,0.L);
    while(fiter != v.end()) {
      ave += face_normals((*fiter).i_,(*fiter).j_);
      ++fiter;
    }
    ave.safe_normalize();
    normals_((*nif_iter).i_, (*nif_iter).j_) = ave;
    ++nif_iter;
  }
  synchronized_ |= Mesh::NORMALS_E;
  normal_lock_.unlock();
}

#define STRUCT_QUAD_SURF_MESH_VERSION 2

template <class Basis>
void
StructQuadSurfMesh<Basis>::io(Piostream& stream)
{
  stream.begin_class(type_name(-1), STRUCT_QUAD_SURF_MESH_VERSION);
  ImageMesh<Basis>::io(stream);
  
  Pio(stream, points_);

  stream.end_class();
}

template <class Basis>
const string
StructQuadSurfMesh<Basis>::type_name(int n)
{
  ASSERT((n >= -1) && n <= 1);
  if (n == -1)
  {
    static const string name = type_name(0) + FTNS + type_name(1) + FTNE;
    return name;
  }
  else if (n == 0)
  {
    static const string nm("StructQuadSurfMesh");
    return nm;
  }
  else 
  {
    return find_type_name((Basis *)0);
  }
}

template <class Basis>
const TypeDescription*
get_type_description(StructQuadSurfMesh<Basis> *)
{
  static TypeDescription *td = 0;
  if (!td)
  {
    const TypeDescription *sub = SCIRun::get_type_description((Basis*)0);
    TypeDescription::td_vec *subs = scinew TypeDescription::td_vec(1);
    (*subs)[0] = sub;
    td = scinew TypeDescription("StructQuadSurfMesh", subs,
				string(__FILE__),
				"SCIRun", 
				TypeDescription::MESH_E);
  }
  return td;
}

template <class Basis>
const TypeDescription*
StructQuadSurfMesh<Basis>::get_type_description() const
{
  return SCIRun::get_type_description((StructQuadSurfMesh<Basis> *)0);
}

template <class Basis>
const TypeDescription*
StructQuadSurfMesh<Basis>::node_type_description()
{
  static TypeDescription *td = 0;
  if (!td)
  {
    const TypeDescription *me = 
      SCIRun::get_type_description((StructQuadSurfMesh<Basis> *)0);
    td = scinew TypeDescription(me->get_name() + "::Node",
				string(__FILE__),
				"SCIRun", 
				TypeDescription::MESH_E);
  }
  return td;
}

template <class Basis>
const TypeDescription*
StructQuadSurfMesh<Basis>::edge_type_description()
{
  static TypeDescription *td = 0;
  if (!td)
  {
    const TypeDescription *me = 
      SCIRun::get_type_description((StructQuadSurfMesh<Basis> *)0);
    td = scinew TypeDescription(me->get_name() + "::Edge",
				string(__FILE__),
				"SCIRun", 
				TypeDescription::MESH_E);
  }
  return td;
}

template <class Basis>
const TypeDescription*
StructQuadSurfMesh<Basis>::face_type_description()
{
  static TypeDescription *td = 0;
  if (!td)
  {
    const TypeDescription *me = 
      SCIRun::get_type_description((StructQuadSurfMesh<Basis> *)0);
    td = scinew TypeDescription(me->get_name() + "::Face",
				string(__FILE__),
				"SCIRun", 
				TypeDescription::MESH_E);
  }
  return td;
}

template <class Basis>
const TypeDescription*
StructQuadSurfMesh<Basis>::cell_type_description()
{
  static TypeDescription *td = 0;
  if (!td)
  {
    const TypeDescription *me = 
      SCIRun::get_type_description((StructQuadSurfMesh<Basis> *)0);
    td = scinew TypeDescription(me->get_name() + "::Cell",
				string(__FILE__),
				"SCIRun", 
				TypeDescription::MESH_E);
  }
  return td;
}

} // namespace SCIRun

#endif // SCI_project_StructQuadSurfMesh_h
