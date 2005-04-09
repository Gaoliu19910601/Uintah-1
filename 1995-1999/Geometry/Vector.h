
#ifndef Geometry_Vector_h
#define Geometry_Vector_h 1

#include <Classlib/Assert.h>

class Point;
class clString;
class Piostream;
class ostream;
class istream;

class Vector {
    double _x,_y,_z;
#if SCI_ASSERTION_LEVEL >= 4
    int uninit;
#endif
public:
    inline Vector(double x, double y, double z): _x(x), _y(y), _z(z)
#if SCI_ASSERTION_LEVEL >= 4
	, uninit(0)
#endif
	    { }
    inline Vector(const Vector&);
    inline Vector();
    double length() const;
    double length2() const;
    friend inline double Dot(const Vector&, const Vector&);
    friend inline double Dot(const Point&, const Vector&);
    friend inline double Dot(const Vector&, const Point&);
    Vector& operator=(const Vector&);

    // checks if one vector is exactly the same as another
    int operator==(const Vector&) const;

    Vector operator*(const double) const;
    Vector& operator*=(const double);
    Vector operator/(const double) const;
    Vector operator/(const Vector&) const;
    Vector operator+(const Vector&) const;
    Vector& operator+=(const Vector&);
    Vector operator-() const;
    Vector operator-(const Vector&) const;
    Vector& operator-=(const Vector&);
    inline double normalize();
    Vector normal() const;
    friend inline Vector Cross(const Vector&, const Vector&);
    friend inline Vector Abs(const Vector&);
    void x(double);
    inline double x() const;
    void y(double);
    inline double y() const;
    void z(double);
    inline double z() const;
    void rotz90(const int);
    Point point() const;
    
    clString string() const;
    
    friend class Point;
    friend class Transform;
    
    friend inline Vector Interpolate(const Vector&, const Vector&, double);
    
    void find_orthogonal(Vector&, Vector&) const;
    
    friend void Pio(Piostream&, Vector&);

    inline Point asPoint() const;
    inline double minComponent() const {
	if(_x<_y){
	    if(_x<_z)
		return _x;
	    else
		return _z;
	} else {
	    if(_y<_z)
		return _y;
	    else
		return _z;
	}
    }
    inline double maxComponent() const {
	if(_x>_y){
	    if(_x>_z)
		return _x;
	    else
		return _z;
	} else {
	    if(_y>_z)
		return _y;
	    else
		return _z;
	}
    }
};

ostream& operator<<(ostream& os, const Vector& p);
istream& operator>>(istream& os, Vector& p);

#include <Math/Expon.h>
#include <Math/MiscMath.h>
#include <Geometry/Point.h>

#include "Vector.icc"

inline Point Vector::asPoint() const {
    return Point(_x,_y,_z);
}

#endif
