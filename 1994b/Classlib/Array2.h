
/*
 *  Array2.h: Interface to dynamic 2D array class
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#ifndef SCI_Classlib_Array2_h
#define SCI_Classlib_Array2_h 1

#include <Classlib/Assert.h>

#ifdef __GNUG__
#pragma interface
#endif

class Piostream;

template<class T>
class Array2 {
    T** objs;
    int dm1;
    int dm2;
    void allocate();
public:
    Array2();
    Array2(const Array2&);
    Array2(int, int);
    Array2<T>& operator=(const Array2&);
    ~Array2();
    inline T& operator()(int d1, int d2) const
	{
	    ASSERTL3(d1>=0 && d1<dm1);
	    ASSERTL3(d2>=0 && d2<dm2);
	    return objs[d1][d2];
	}
    inline int dim1() const {return dm1;}
    inline int dim2() const {return dm2;}
    void newsize(int, int);
    void initialize(const T&);

    inline T** get_dataptr() {return objs;}

    friend void Pio(Piostream&, Array2<T>&);
    friend void Pio(Piostream&, Array2<T>*&);

};


#endif
