#ifndef UINTAH_HOMEBREW_Array3Window_H
#define UINTAH_HOMEBREW_Array3Window_H

#include <SCICore/Geometry/IntVector.h>
#include "RefCounted.h"
#include "Array3Data.h"

/**************************************

CLASS
   Array3Window
   
GENERAL INFORMATION

   Array3Window.h

   Steven G. Parker
   Department of Computer Science
   University of Utah

   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
  
   Copyright (C) 2000 SCI Group

KEYWORDS
   Array3Window

DESCRIPTION
   Long description...
  
WARNING
  
****************************************/

namespace Uintah {
   using SCICore::Geometry::IntVector;
   template<class T> class Array3Window : public RefCounted {
   public:
      Array3Window(Array3Data<T>*);
      Array3Window(Array3Data<T>*, const IntVector& offset,
		   const IntVector& lowIndex, const IntVector& highIndex);
      virtual ~Array3Window();
      
      inline Array3Data<T>* getData() const {
	 return data;
      }
      
      void copy(const Array3Window<T>*);
      void copy(const Array3Window<T>*, const IntVector& low, const IntVector& high);
      void initialize(const T&);
      void initialize(const T&, const IntVector& s, const IntVector& e);
      inline IntVector getLowIndex() const {
	 return lowIndex;
      }
      inline IntVector getHighIndex() const {
	 return highIndex;
      }
      inline IntVector getOffset() const {
	 return offset;
      }
      inline T& get(const IntVector& idx) {
	 ASSERT(data);
	 CHECKARRAYBOUNDS(idx.x(), lowIndex.x(), highIndex.x());
	 CHECKARRAYBOUNDS(idx.y(), lowIndex.y(), highIndex.y());
	 CHECKARRAYBOUNDS(idx.z(), lowIndex.z(), highIndex.z());
	 return data->get(idx-offset);
      }
      
      ///////////////////////////////////////////////////////////////////////
      //
      // Return pointer to the data 
      // (**WARNING**not complete implementation)
      //
      inline T* getPointer() {
	return (data->getPointer());
      }
      
      ///////////////////////////////////////////////////////////////////////
      //
      // Return const pointer to the data 
      // (**WARNING**not complete implementation)
      //
      inline const T* getPointer() const {
	return (data->getPointer());
      }

   private:
      
      Array3Data<T>* data;
      IntVector offset;
      IntVector lowIndex;
      IntVector highIndex;
      Array3Window(const Array3Window<T>&);
      Array3Window<T>& operator=(const Array3Window<T>&);
   };
   
   template<class T>
      void Array3Window<T>::initialize(const T& val)
      {
	 data->initialize(val, lowIndex-offset, highIndex-offset);
      }
   
   template<class T>
      void Array3Window<T>::copy(const Array3Window<T>* from)
      {
	 data->copy(lowIndex-offset, highIndex-offset, from->data,
		    from->lowIndex-from->offset, from->highIndex-from->offset);
      }
   
   template<class T>
      void Array3Window<T>::copy(const Array3Window<T>* from,
				 const IntVector& low, const IntVector& high)
      {
	 data->copy(low-offset, high-offset, from->data,
		    low-from->offset, high-from->offset);
      }
   
   template<class T>
      void Array3Window<T>::initialize(const T& val,
				       const IntVector& s,
				       const IntVector& e)
      {
	 CHECKARRAYBOUNDS(s.x(), lowIndex.x(), highIndex.x());
	 CHECKARRAYBOUNDS(s.y(), lowIndex.y(), highIndex.y());
	 CHECKARRAYBOUNDS(s.z(), lowIndex.z(), highIndex.z());
	 CHECKARRAYBOUNDS(e.x(), s.x(), highIndex.x()+1);
	 CHECKARRAYBOUNDS(e.y(), s.y(), highIndex.y()+1);
	 CHECKARRAYBOUNDS(e.z(), s.z(), highIndex.z()+1);
	 data->initialize(val, s-offset, e-offset);
      }
   
   template<class T>
      Array3Window<T>::Array3Window(Array3Data<T>* data)
      : data(data), offset(0,0,0), lowIndex(0,0,0), highIndex(data->size())
      {
	 data->addReference();
      }
   
   template<class T>
      Array3Window<T>::Array3Window(Array3Data<T>* data,
				    const IntVector& offset,
				    const IntVector& lowIndex,
				    const IntVector& highIndex)
      : data(data), offset(offset), lowIndex(lowIndex), highIndex(highIndex)
      {
	 CHECKARRAYBOUNDS(lowIndex.x()-offset.x(), 0, data->size().x());
	 CHECKARRAYBOUNDS(lowIndex.y()-offset.y(), 0, data->size().y());
	 CHECKARRAYBOUNDS(lowIndex.z()-offset.z(), 0, data->size().z());
	 CHECKARRAYBOUNDS(highIndex.x()-offset.x(), 0, data->size().x()+1);
	 CHECKARRAYBOUNDS(highIndex.y()-offset.y(), 0, data->size().y()+1);
	 CHECKARRAYBOUNDS(highIndex.z()-offset.z(), 0, data->size().z()+1);
	 data->addReference();
      }
   
   template<class T>
      Array3Window<T>::~Array3Window()
      {
	 if(data && data->removeReference())
	    delete data;
      }
   
}

//
// $Log$
// Revision 1.8.2.1  2000/10/26 10:06:06  moulding
// merge HEAD into FIELD_REDESIGN
//
// Revision 1.9  2000/10/11 17:39:38  sparker
// Added copy with range
// Fixed bug in Array3Data::copy
// Fixed compiler warnings
//
// Revision 1.8  2000/09/20 15:48:30  sparker
// Added .copy() method to copy one Array3 from another
//
// Revision 1.7  2000/06/15 21:57:15  sparker
// Added multi-patch support (bugzilla #107)
// Changed interface to datawarehouse for particle data
// Particles now move from patch to patch
//
// Revision 1.6  2000/06/11 04:05:07  bbanerje
// Added first cut of getPointer() needed for fortran calls.
//
// Revision 1.5  2000/05/10 20:02:58  sparker
// Added support for ghost cells on node variables and particle variables
//  (work for 1 patch but not debugged for multiple)
// Do not schedule fracture tasks if fracture not enabled
// Added fracture directory to MPM sub.mk
// Be more uniform about using IntVector
// Made regions have a single uniform index space - still needs work
//
// Revision 1.4  2000/04/26 06:48:46  sparker
// Streamlined namespaces
//
// Revision 1.3  2000/03/21 02:22:57  dav
// few more updates to make it compile including moving Array3 stuff out of namespace as I do not know where it should be
//
// Revision 1.2  2000/03/16 22:07:58  dav
// Added the beginnings of cocoon docs.  Added namespaces.  Did a few other coding standards updates too
//
//

#endif
