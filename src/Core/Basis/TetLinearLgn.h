//  
//  For more information, please see: http://software.sci.utah.edu
//  
//  The MIT License
//  
//  Copyright (c) 2004 Scientific Computing and Imaging Institute,
//  University of Utah.
//  
//  License for the specific language governing rights and limitations under
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//  
//  The above copyright notice and this permission notice shall be included
//  in all copies or substantial portions of the Software.
//  
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//  
//    File   : TetLinearLgn.h
//    Author : Martin Cole, Frank B. Sachse
//    Date   : Dec 04 2004

#if !defined(TetLinearLgn_h)
#define TetLinearLgn_h

#include <vector>
#include <string>
#include <Core/Basis/TriLinearLgn.h>
#include <Core/Geometry/Point.h>
#include <Core/Util/TypeDescription.h>
#include <Core/Datatypes/Datatype.h>
#include <Core/Geometry/Transform.h>
#include <float.h>

namespace SCIRun {

using std::vector;
using std::string;

class TetApprox {  
public:
  static double UnitVertices[4][3];
  static int UnitEdges[6][3];
  static int UnitFaces[4][3];

  TetApprox() {}
  virtual ~TetApprox() {}
  
  //! Approximate edge for element by piecewise linear segments
  //! return: coords describes an array of points approximating the edge.
  virtual void approx_edge(const unsigned edge, const unsigned div_per_unit, 
			   vector<vector<double> > &coords) const
  {
    coords.resize(div_per_unit + 1);

    const double *v0 = UnitVertices[UnitEdges[edge][0]];
    const double *v1 = UnitVertices[UnitEdges[edge][1]];

    const double &p1x = v0[0];
    const double &p1y = v0[1];
    const double &p1z = v0[2];
    const double dx = v1[0] - p1x;
    const double dy = v1[1] - p1y;
    const double dz = v1[2] - p1z;

    for(unsigned i = 0; i <= div_per_unit; i++) {
      const double d = (double)div_per_unit / (double)i;
      vector<double> &tmp = coords[i];
      tmp[0] = p1x + d * dx;
      tmp[1] = p1y + d * dy;
      tmp[2] = p1z + d * dz;
    } 	
  }
  
  virtual int get_approx_face_elements() const { return 3; }
  
  virtual void approx_face(const unsigned face, 
			   const unsigned div_per_unit, 
			   vector<vector<vector<double> > > &coords) const
  {
    const double *v0 = UnitVertices[UnitFaces[face][0]];
    const double *v1 = UnitVertices[UnitFaces[face][1]];
    const double *v2 = UnitVertices[UnitFaces[face][2]];
	
    coords.resize(div_per_unit);
    const double d = 1. / div_per_unit;
    for(unsigned j = 0; j<div_per_unit; j++) {
      const double dj = (double)div_per_unit / (double)j;
      unsigned e = 0;
      coords[j].resize((div_per_unit - j) * 2 + 1);
      vector<double> &tmp = coords[j][e++];

      tmp[0] = v0[0] + dj * (v2[0] - v0[0]);
      tmp[1] = v0[1] + dj * (v2[1] - v0[1]);
      tmp[2] = v0[2] + dj * (v2[2] - v0[2]);

      for(unsigned i = 0; i<div_per_unit - j; i++) {
	const double di = (double)div_per_unit / (double)i;
	tmp = coords[j][e++];
	tmp[0] = v0[0] + (dj + d) * (v2[0] - v0[0]) + di * (v1[0] - v0[0]);
	tmp[1] = v0[1] + (dj + d) * (v2[1] - v0[1]) + di * (v1[1] - v0[1]);
	tmp[2] = v0[2] + (dj + d) * (v2[2] - v0[2]) + di * (v1[2] - v0[2]);

	tmp = coords[j][e++];
	tmp[0] = v0[0] + dj * (v2[0] - v0[0]) + (di + d) * (v1[0] - v0[0]);
	tmp[1] = v0[1] + dj * (v2[1] - v0[1]) + (di + d) * (v1[1] - v0[1]);
	tmp[2] = v0[2] + dj * (v2[2] - v0[2]) + (di + d) * (v1[2] - v0[2]);
      }
    }
  }
};


template <class T>
class TetLinearLgn : public TetApprox {
public:
  typedef T value_type;

  static int GaussianNum;
  static double GaussianPoints[4][2];
  static double GaussianWeights[4];
  
  TetLinearLgn() {}
  virtual ~TetLinearLgn() {}

  int polynomial_order() const { return 1; }

  // Value at coord
  template <class ElemData>
  T interpolate(const vector<double> &coords, const ElemData &cd) const
  {
    const double x = coords[0], y = coords[1], z = coords[2];  
    return (T)((1 - x - y - z) * cd.node0() + x * cd.node1() + 
	       y * cd.node2() + z * cd.node3());
  }
  
  //! First derivative at coord.
  template <class ElemData>
  void derivate(const vector<double> &coords, const ElemData &cd, 
		vector<double> &derivs) const
  {
    derivs.resize(3);
	
    derivs[0] = -cd.node0() + cd.node1();
    derivs[1] = -cd.node0() + cd.node2();
    derivs[2] = -cd.node0() + cd.node3();
  }
  
  //! return the parametric coordinates for value within the element.
  //! iterative solution...
  template <class ElemData>
  void get_coords(vector<double> &coords, const T& value, 
		  const ElemData &cd) const;  

  static  const string type_name(int n = -1);

  virtual void io (Piostream& str);

protected:
  double distance(const T&, const T&) const;
};


template <class T>
const TypeDescription* get_type_description(TetLinearLgn<T> *)
{
  static TypeDescription *td = 0;
  if (!td)
  {
    td = scinew TypeDescription(TetLinearLgn<T>::type_name(-1), 
				string(__FILE__), "SCIRun");
  }
  return td;
}

template <class T>
const string
TetLinearLgn<T>::type_name(int n)
{
  ASSERT((n >= -1) && n <= 1);
  if (n == -1)
  {
    static const string name = type_name(0) + FTNS + type_name(1) + FTNE;
    return name;
  }
  else if (n == 0)
  {
    static const string nm("TetLinearLgn");
    return nm;
  } else {
    return find_type_name((T *)0);
  }
}


template <class T>
template <class ElemData>
void 
TetLinearLgn<T>::get_coords(vector<double> &coords, const T& value, 
			    const ElemData &cd) const
{
}

const int TETLINEARLGN_VERSION = 1;
template <class T>
void
TetLinearLgn<T>::io(Piostream &stream)
{
  stream.begin_class(type_name(-1), TETLINEARLGN_VERSION);
  stream.end_class();
}

template <class T>
int TetLinearLgn<T>::GaussianNum = 4;

template <class T>
double TetLinearLgn<T>::GaussianPoints[4][2] = {
  {1./6., 1./6., 0., 1./6.}, {2./3., 1./6., 0., 1./6.}, 
  {1./6., 2./3., 0., 1./6.}, {1./6., 1./6., 2./3., 0.}};

template <class T>
double TetLinearLgn<T>::GaussianWeights[4] = {.25, .25, 25.};

} //namespace SCIRun

#endif // TetLinearLgn_h
