
/*
 *  Parallel: Automatically instantiate several threads
 *
 *  Written by:
 *   Author: Steve Parker
 *   Department of Computer Science
 *   University of Utah
 *   Date: June 1997
 *
 *  Copyright (C) 1997 SCI Group
 */

#ifndef Core_Thread_Parallel_h
#define Core_Thread_Parallel_h

#include <Core/Thread/ParallelBase.h>
#include <Core/Thread/Semaphore.h>

namespace SCIRun {
/**************************************
 
				       CLASS
				       Parallel
   
				       KEYWORDS
				       Thread
   
				       DESCRIPTION
				       Helper class to make instantiating threads to perform a parallel
				       task easier.
   
****************************************/
template<class T> class Parallel  : public ParallelBase {
public:
  //////////
  // Create a parallel object, using the specified member
  // function instead of <i>parallel</i>.  This will
  // typically be used like:
  // <b><pre>Thread::parallel(Parallel&lt;MyClass&gt;(this, &amp;MyClass::mymemberfn), nthreads);</pre></b>
  Parallel(T* obj, void (T::*pmf)(int));
	    
  //////////
  // Destroy the Parallel object - the threads will remain alive.
  virtual ~Parallel();
  T* d_obj;
  void (T::*d_pmf)(int);
protected:
  virtual void run(int proc);
private:
  // Cannot copy them
  Parallel(const Parallel&);
  Parallel<T>& operator=(const Parallel<T>&);
};

template<class T>
void
Parallel<T>::run(int proc)
{
    // Copy out do make sure that the call is atomic
    T* obj=d_obj;
    void (T::*pmf)(int) = d_pmf;
    if(d_wait)
	d_wait->up();
    (obj->*pmf)(proc);
    // Cannot do anything here, since the object may be deleted by the
    // time we return
}

template<class T>
Parallel<T>::Parallel(T* obj, void (T::*pmf)(int))
    : d_obj(obj), d_pmf(pmf)
{
    d_wait=0; // This may be set by Thread::parallel
} // End namespace SCIRun

template<class T>
Parallel<T>::~Parallel()
{
}

} //End namespace SCIRun

#endif




