/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.

  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANK KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.

  The Original Source Code is SCIRun, released March 12, 2001.

  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994
  University of Utah. All Rights Reserved.
*/

/*
 *  ImageMesh.h: Templated Mesh defined on a 2D Regular Grid
 *
 *  Written by:
 *   Michael Callahan
 *   Department of Computer Science
 *   University of Utah
 *   January 2001
 *
 *  Copyright (C) 2001 SCI Group
 *
 */

#ifndef SCI_project_ImageMesh_h
#define SCI_project_ImageMesh_h 1

#include <Core/Datatypes/Mesh.h>
#include <Core/Datatypes/FieldIterator.h>
#include <Core/Geometry/Transform.h>

namespace SCIRun {

using std::string;

class SCICORESHARE ImageMesh : public Mesh
{
public:
  struct ImageIndex;
  friend struct ImageIndex;

  struct ImageIndex
  {
  public:
    ImageIndex() : i_(0), j_(0), mesh_(0) {}

    ImageIndex(unsigned i, unsigned j) 
      : i_(i), j_(j), mesh_(0) {}

    ImageIndex(const ImageMesh *m, unsigned i, unsigned j) 
      : i_(i), j_(j), mesh_(m) {}

    operator unsigned() const { 
      if (mesh_ == 0) 
        return i_*j_; 
      else
        return i_ + j_ * mesh_->ni_;
    }

    unsigned i_, j_;

    const ImageMesh *mesh_;
  };

  struct IFaceIndex : public ImageIndex
  {
    IFaceIndex() : ImageIndex() {}
    IFaceIndex(unsigned i, unsigned j) 
      : ImageIndex(i, j) {}
    friend void Pio(Piostream&, IFaceIndex&);
    friend const TypeDescription* get_type_description(IFaceIndex *);
    friend const string find_type_name(IFaceIndex *);
  };

  struct INodeIndex : public ImageIndex
  {
    INodeIndex() : ImageIndex() {}
    INodeIndex(unsigned i, unsigned j) 
      : ImageIndex(i, j) {}
    friend void Pio(Piostream&, INodeIndex&);
    friend const TypeDescription* get_type_description(INodeIndex *);
    friend const string find_type_name(INodeIndex *);
  };

  struct ImageIter : public ImageIndex
  {
    ImageIter() : ImageIndex(0, 0) {}
    ImageIter(const ImageMesh *m, unsigned i, unsigned j)
      : ImageIndex(m, i, j) {}

    const ImageIndex &operator *() { return *this; }

    bool operator ==(const ImageIter &a) const
    {
      return i_ == a.i_ && j_ == a.j_ && mesh_ == a.mesh_;
    }

    bool operator !=(const ImageIter &a) const
    {
      return !(*this == a);
    }
  };

  struct INodeIter : public ImageIter
  {
    INodeIter() : ImageIter() {}
    INodeIter(const ImageMesh *m, unsigned i, unsigned j)
      : ImageIter(m, i, j) {}

    const INodeIndex &operator *() const { return (const INodeIndex&)(*this); }

    INodeIter &operator++()
    {
      i_++;
      if (i_ >= mesh_->min_i_ + mesh_->ni_) {
	i_ = mesh_->min_i_;
	j_++;
      }
      return *this;
    }

  private:

    INodeIter operator++(int)
    {
      INodeIter result(*this);
      operator++();
      return result;
    }
  };


  struct IFaceIter : public ImageIter
  {
    IFaceIter() : ImageIter() {}
    IFaceIter(const ImageMesh *m, unsigned i, unsigned j)
      : ImageIter(m, i, j) {}

    const IFaceIndex &operator *() const { return (const IFaceIndex&)(*this); }

    IFaceIter &operator++()
    {
      i_++;
      if (i_ >= mesh_->min_i_+mesh_->ni_-1) {
	i_ = mesh_->min_i_;
	j_++;
      }
      return *this;
    }

  private:

    IFaceIter operator++(int)
    {
      IFaceIter result(*this);
      operator++();
      return result;
    }
  };

  //typedef ImageIndex      under_type;

  //! Index and Iterator types required for Mesh Concept.
  struct Node {
    typedef INodeIndex                       index_type;
    typedef INodeIter                        iterator;
    typedef INodeIndex                       size_type;
    typedef vector<index_type>               array_type;
  };			
			
  struct Edge {		
    typedef EdgeIndex<unsigned int>          index_type;
    typedef EdgeIterator<unsigned int>       iterator;
    typedef EdgeIndex<unsigned int>          size_type;
    typedef vector<index_type>               array_type;
  };			
			
  struct Face {		
    typedef IFaceIndex                       index_type;
    typedef IFaceIter                        iterator;
    typedef IFaceIndex                       size_type;
    typedef vector<index_type>               array_type;
  };			
			
  struct Cell {		
    typedef CellIndex<unsigned int>          index_type;
    typedef CellIterator<unsigned int>       iterator;
    typedef CellIndex<unsigned int>          size_type;
    typedef vector<index_type>               array_type;
  };

  typedef Face Elem;

  friend class INodeIter;
  friend class IFaceIter;

  ImageMesh()
    : min_i_(0), min_j_(0),
      ni_(1), nj_(1) {}
  ImageMesh(unsigned x, unsigned y, const Point &min, const Point &max);
  ImageMesh(ImageMesh* mh, unsigned int mx, unsigned int my,
	    unsigned int x, unsigned int y)
    : min_i_(mx), min_j_(my), ni_(x), nj_(y), transform_(mh->transform_) {}
  ImageMesh(const ImageMesh &copy)
    : min_i_(copy.min_i_), min_j_(copy.min_j_),
      ni_(copy.get_ni()), nj_(copy.get_nj()), transform_(copy.transform_) {}
  virtual ImageMesh *clone() { return new ImageMesh(*this); }
  virtual ~ImageMesh() {}

  //! get the mesh statistics
  unsigned get_min_i() const { return min_i_; }
  unsigned get_min_j() const { return min_j_; }
  Array1<unsigned int> get_min() const;
  unsigned get_ni() const { return ni_; }
  unsigned get_nj() const { return nj_; }
  Array1<unsigned int> get_dim() const;
  Vector diagonal() const;
  virtual BBox get_bounding_box() const;
  virtual void transform(Transform &t);

  //! set the mesh statistics
  void set_min_i(unsigned i) {min_i_ = i; }
  void set_min_j(unsigned j) {min_j_ = j; }
  void set_min(Array1<unsigned int> mins);
  void set_ni(unsigned i) { ni_ = i; }
  void set_nj(unsigned j) { nj_ = j; }
  void set_dim(Array1<unsigned int> dims);

  void begin(Node::iterator &) const;
  void begin(Edge::iterator &) const;
  void begin(Face::iterator &) const;
  void begin(Cell::iterator &) const;

  void end(Node::iterator &) const;
  void end(Edge::iterator &) const;
  void end(Face::iterator &) const;
  void end(Cell::iterator &) const;

  void size(Node::size_type &) const;
  void size(Edge::size_type &) const;
  void size(Face::size_type &) const;
  void size(Cell::size_type &) const;

  //! get the child elements of the given index
  void get_nodes(Node::array_type &, Edge::index_type) const;
  void get_nodes(Node::array_type &, Face::index_type) const;
  void get_nodes(Node::array_type &, Cell::index_type) const {}
  void get_edges(Edge::array_type &, Face::index_type) const {}
  void get_edges(Edge::array_type &, Cell::index_type) const {}
  void get_faces(Face::array_type &, Cell::index_type) const {}

  //! get the parent element(s) of the given index
  unsigned get_edges(Edge::array_type &, Node::index_type) const { return 0; }
  unsigned get_faces(Face::array_type &, Node::index_type) const { return 0; }
  unsigned get_faces(Face::array_type &, Edge::index_type) const { return 0; }
  unsigned get_cells(Cell::array_type &, Node::index_type) const { return 0; }
  unsigned get_cells(Cell::array_type &, Edge::index_type) const { return 0; }
  unsigned get_cells(Cell::array_type &, Face::index_type) const { return 0; }

  //! return all face_indecies that overlap the BBox in arr.
  void get_faces(Face::array_type &arr, const BBox &box);

  //! similar to get_faces() with Face::index_type argument, but
  //  returns the "other" face if it exists, not all that exist
  bool get_neighbor(Face::index_type & /*neighbor*/, Face::index_type /*from*/,
		    Edge::index_type /*idx*/) const {
    ASSERTFAIL("ImageMesh::get_neighbor not implemented.");
  }

  //! get the center point (in object space) of an element
  void get_center(Point &, const Node::index_type &) const;
  void get_center(Point &, Edge::index_type) const;
  void get_center(Point &, const Face::index_type &) const;
  void get_center(Point &, Cell::index_type) const {}

  bool locate(Node::index_type &, const Point &);
  bool locate(Edge::index_type &, const Point &) const { return false; }
  bool locate(Face::index_type &, const Point &);
  bool locate(Cell::index_type &, const Point &) const { return false; }

  void get_weights(const Point &p, Node::array_type &l, vector<double> &w);
  void get_weights(const Point &, Edge::array_type &, vector<double> &) {ASSERTFAIL("ImageMesh::get_weights for edges isn't supported");}
  void get_weights(const Point &p, Face::array_type &l, vector<double> &w);
  void get_weights(const Point &, Cell::array_type &, vector<double> &) {ASSERTFAIL("ImageMesh::get_weights for cells isn't supported");}

  void get_point(Point &p, Node::index_type i) const
  { get_center(p, i); }
  void get_normal(Vector &/* result */, Node::index_type /* index */) const
  { ASSERTFAIL("not implemented") }

  void get_random_point(Point &p, const Face::index_type &fi, 
			int seed=0) const;

  virtual void io(Piostream&);
  static PersistentTypeID type_id;
  static  const string type_name(int n = -1);
  virtual const TypeDescription *get_type_description() const;

  // Unsafe due to non-constness of unproject.
  Transform &get_transform() { return transform_; }
  Transform &set_transform(const Transform &trans) 
  { transform_ = trans; return transform_; }

protected:

  //! the min_Node::index_type ( incase this is a subLattice )
  unsigned min_i_, min_j_;
  //! the Node::index_type space extents of a ImageMesh
  //! (min=min_Node::index_type, max=min+extents-1)
  unsigned ni_, nj_;

  //! the object space extents of a ImageMesh
  Transform transform_;

  // returns a ImageMesh
  static Persistent *maker() { return new ImageMesh(); }
};

typedef LockingHandle<ImageMesh> ImageMeshHandle;

const TypeDescription* get_type_description(ImageMesh *);
const TypeDescription* get_type_description(ImageMesh::Node *);
const TypeDescription* get_type_description(ImageMesh::Edge *);
const TypeDescription* get_type_description(ImageMesh::Face *);
const TypeDescription* get_type_description(ImageMesh::Cell *);

} // namespace SCIRun

#endif // SCI_project_ImageMesh_h
