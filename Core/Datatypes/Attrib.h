// Attrib.h - the base attribute class.
//
//  Written by:
//   Eric Kuehne, Alexei Samsonov
//   Department of Computer Science
//   University of Utah
//   April 2000, December 2000
//
//  Copyright (C) 2000 SCI Institute
//
//  General storage class for Fields.
//

#ifndef SCI_project_Attrib_h
#define SCI_project_Attrib_h 1

#include <vector>
#include <string>
#include <iostream>

#include <Core/Datatypes/Datatype.h>
#include <Core/Containers/LockingHandle.h>
#include <Core/Exceptions/DimensionMismatch.h>
#include <Core/Geometry/Vector.h>
#include <Core/Geometry/Point.h>
#include <Core/Util/FancyAssert.h>
#include <Core/Datatypes/TypeName.h>
#include <Core/Persistent/PersistentSTL.h>

namespace SCIRun {

using namespace std;

/////////
// Structure to hold Neumann BC related values
class NeumannBC {
public:  
  // GROUP: public data
  //////////
  // 
  NeumannBC(){};
  NeumannBC(Vector v, double d): dir(v), val(d){};
  //////////
  // Direction to take derivative in
  Vector dir;
  //////////
  // Value of the derivative
  double val;
};

//////////
// PIO for NeumannBC objects
void  Pio(Piostream&, NeumannBC&);
ostream& operator<<(ostream&, NeumannBC&);

class Attrib;
typedef LockingHandle<Attrib> AttribHandle;

class Attrib : public Datatype 
{
public:

  // GROUP:  Constructors/Destructor
  //////////
  //
  Attrib();
  virtual ~Attrib();
  
  // GROUP: Class interface functions
  //////////
  // 
  
  /////////
  // Casts down to handle to attribute of specific type.
  // Returns empty handle if it was not successeful cast
  template <class T> LockingHandle<T> downcast(T*) {
    T* rep = dynamic_cast<T*>(this);
    return LockingHandle<T>(rep);
  }

  //////////
  // Get information about the attribute
  virtual string getInfo() = 0;
  
  //////////
  // Returns type name:
  // 0 - the class name
  // n!=0 - name of the n-th parameter (for templatazed types)
  virtual string getTypeName(int n) = 0;
 
  //////////
  // Attribute naming
  void setName(string name){
    d_name=name;
  };

  string getName(){
    return d_name;
  };

  // GROUP: Support of persistent representation
  //////////
  // Persistent IO

  virtual void io(Piostream&);
  static  PersistentTypeID type_id;
  static  string typeName(int);

  // GROUP: Public Data
  //////////
  // 
  string      d_unitName;
  
  //////////
  // Attribute creation data
  string      d_authorName;
  string      d_date;
  string      d_orgName;
  
protected:
  string      d_name;
};

}  // end namespace SCIRun

#endif
