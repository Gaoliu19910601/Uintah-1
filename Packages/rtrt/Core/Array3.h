
/*
 *  Array3.h: Interface to dynamic 3D array class
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifndef Packages_rtrt_Core_Array3_h
#define Packages_rtrt_Core_Array3_h 1

namespace rtrt {

template<class T>
class Array3 {
    T*** objs;
    int* refcnt;
    int dm1;
    int dm2;
    int dm3;
    void allocate();
    Array3<T>& operator=(const Array3&);
public:
    Array3();
    Array3(int, int, int);
    ~Array3();
    inline T& operator()(int d1, int d2, int d3) const
        {
            return objs[d1][d2][d3];
        }
    inline int dim1() const {return dm1;}
    inline int dim2() const {return dm2;}
    inline int dim3() const {return dm3;}
    void resize(int, int, int);
    void initialize(const T&);

    inline T* get_dataptr() {return objs[0][0];}
    inline unsigned long get_datasize() {
	return dm1*dm2*dm3*sizeof(T);
    }
    void share(const Array3<T>& copy);
};

} // end namespace rtrt

#include <Packages/rtrt/Core/Array3.cc>
  

#endif // Packages_rtrt_Core_Array3_h

