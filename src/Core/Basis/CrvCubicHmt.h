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
//    File   : CrvCubicHmt.h
//    Author : Martin Cole, Frank B. Sachse
//    Date   : Dec 04 2004

#if !defined(CrvCubicHmt_h)
#define CrvCubicHmt_h

#include <CrvLinearLgn.h>

namespace SCIRun {

template <class T>
class CrvCubicHmt : public CrvApprox<T>
{
public:
  typedef T value_type;

  CrvCubicHmt() {}
  virtual ~CrvCubicHmt() {}
  
  int polynomial_order() const { return 3; }

  // Value at coord
  template <class CellData>
  T interpolate(const vector<double> &coords, const CellData &cd) const
  {
    const double x=coords[0];  
    return T((x-1)*(x-1)*(1 + 2*x) * cd.node0() 
	     +(x-1)*(x-1)*x * cd.node1()
	     +(3 - 2*x)*x*x * derivs_[cd.node0_index()]  
	     +(-1+x)*x*x * derivs_[cd.node1_index()]);
  }
  
  //! First derivative at coord.
  template <class CellData>
  void derivate(const vector<double> &coords, const CellData &cd, 
		vector<double> &derivs) const
  {
    const double x=coords[0]; 
 
    derivs.size(1);

    derivs[0] = 6*(-1 + x)*x * cd.node0() 
      +(1 - 4*x + 3*x*x) * cd.node1()
      -6*(-1 + x)*x * derivs_[cd.node0_index()]  
      +x*(-2 + 3*x) * derivs_[cd.node1_index()];
  }
  
  //! return the parametric coordinates for value within the element.
  //! iterative solution...
  template <class CellData>
  void get_coords(vector<double> &coords, const T& value, 
		  const CellData &cd) const;  

  //! add a derivative value (dx) for nodes
  void add_derivative(const T &p) { derivs_.push_back(p); }

  static  const string type_name(int n = -1);

  virtual void io (Piostream& str);
protected:
  //! next_guess is the next Newton iteration step.
  template <class CellData>
  double next_guess(double xi, const T &val, const CellData &cd) const;

  //! find a reasonable initial guess for starting Newton iteration.
  template <class CellData>
  double initial_guess(const T &val, const CellData &cd) const;

  //! Additional support values.

  //! Cubic Hermitian only needs additonal derivatives stored at each node
  //! in the topology.
  vector<T>          derivs_; 
};


template <class T>
const TypeDescription* get_type_description(CrvCubicHmt<T> *)
{
  static TypeDescription* td = 0;
  if(!td){
    const TypeDescription *sub = SCIRun::get_type_description((T*)0);
    TypeDescription::td_vec *subs = scinew TypeDescription::td_vec(1);
    (*subs)[0] = sub;
    td = scinew TypeDescription(CrvCubicHmt<T>::type_name(0), subs, 
				string(__FILE__),
				"SCIRun");
  }
  return td;
}

template <class T>
const TypeDescription* get_type_description(BiCrvCubicHmt<T> *)
{
  static TypeDescription *td = 0;
  if (!td)
  {
    td = scinew TypeDescription(BiCrvCubicHmt<T>::type_name(-1), 
				string(__FILE__), "SCIRun");
  }
  return td;
}

template <class T>
const TypeDescription* get_type_description(TriCrvCubicHmt<T> *)
{
  static TypeDescription *td = 0;
  if (!td)
  {
    td = scinew TypeDescription(TriCrvCubicHmt<T>::type_name(-1), 
				string(__FILE__), "SCIRun");
  }
  return td;
}

template <class T>
const string
CrvCubicHmt<T>::type_name(int n)
{
  ASSERT((n >= -1) && n <= 1);
  if (n == -1)
  {
    static const string name = type_name(0) + FTNS + type_name(1) + FTNE;
    return name;
  }
  else if (n == 0)
  {
    static const string nm("CrvCubicHmt");
    return nm;
  } else {
    return find_type_name((T *)0);
  }
}

const int CRVCUBICHMT_BASIS_VERSION = 1;
template <class T>
void
CrvCubicHmt<T>::io(Piostream &stream)
{
  stream.begin_class(type_name(-1), CRVCUBICHMT_VERSION);
  Pio(stream, derivs_);
  stream.end_class();
}


template <class T>
double calc_next_guess(const double xi, const T &val,
		       const T &interp, const T &deriv, const T &deriv2);

//! f(u) =C'(u) dot (C(u) - P)
//! the distance from P to C(u) is minimum when f(u) = 0
template <>
double calc_next_guess<Point>(const double xi, const Point &val,
			      const Point &interp, const Point &deriv, 
			      const Point &deriv2);
  
template <class T>
double 
calc_next_guess(const double xi, const T &val,
		const T &interp, const T &deriv, const T &deriv2) 
{
  T num = val - interpolate(xi, cd); 
  T den = derivate(xi, cd);
  return num / den + xi;
}


template <class T>
template <class CellData>
double 
CrvCubicHmt<T>::next_guess(double coord, const T &val, 
			   const CellData &cd) const 
{
  vector<double> xi(1, coord);
  return calc_next_guess(xi[0], val, interpolate(xi, cd), 
			 derivate(xi, cd), derivate2(xi, cd));
}

template <class T>
template <class CellData>
double 
CrvCubicHmt<T>::initial_guess(const T &val, const CellData &cd) const
{
  double dist = distance(val, cd.node0());
  double guess = 0.;
  double cur_d = distance(val, cd.node1());
  if (cur_d < dist) {
    dist = cur_d;
    guess = 1.;
  }
  int end = 4;
  vector<double> cur(1,0.L);
  for (int g = 1; g < end; g++) {
    cur[0] = g*(1. / end);
    T cur_val = interpolate(cur, cd);
    cur_d = distance(val, cur_val);
    if (cur_d < dist) {
      dist = cur_d;
      guess = cur[0];
    }
  }
  return guess;
}

template <class T>
template <class CellData>
void 
CrvCubicHmt<T>::get_coords(vector<double> &coords, const T& value, 
			   const CellData &cd) const
{

  //! Step 1: get a good guess on the curve, evaluate equally spaced points 
  //!         on the curve and use the closest as our starting point for 
  //!         Newton iteration.
  
  double cur = initial_guess(value, cd);
  double last = 0.;
  
  //! Now closest has our initialization param for Newton iteration.
  //! Step 2: Newton iteration.
  
  while (fabs(cur - last) > 0.00001) {
    last = cur;
    cur = next_guess(cur, value, cd);
  }
  coords.clear();
  coords.push_back(cur);
}
 

} //namespace SCIRun

#endif // CrvCubicHmt_h
