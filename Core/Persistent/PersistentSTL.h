/*
 * PersistentSTL.h: Persistent i/o for STL containers
 *    Author: David Hart, Alexei Samsonov
 *            Department of Computer Science
 *            University of Utah
 *            Mar. 2000, Dec 2000
 *    Copyright (C) 2000 SCI Group
 * 
 */

#ifndef SCI_project_PersistentSTL_h
#define SCI_project_PersistentSTL_h 1

#include <Core/Persistent/Persistent.h>
#include <map>
#include <vector>
#include <list>

namespace SCIRun {

using std::map;
using std::vector;
using std::list;

#define MAP_VERSION 1

//////////
// Persistent io for maps
template <class Key, class Data>
SCICORESHARE inline void
Pio(Piostream& stream, map<Key, Data>& data) {

#ifdef __GNUG__
#else
#endif

  map<Key, Data>::iterator iter;
  int i, n;
  Key k;
  Data d;
  
  stream.begin_class("Map", MAP_VERSION);

				// if reading from stream
  if (stream.reading()) {	
				// get map size 
    Pio(stream, n);
				// read elements
    for (i = 0; i < n; i++) {
      Pio(stream, k);
      Pio(stream, d);
      data[k] = d;
    }
    
  }
				// if writing to stream
  else {
				// write map size
    int n = data.size();
    Pio(stream, n);
				// write elements
    for (iter = data.begin(); iter != data.end(); iter++) {
				// have to copy iterator elements,
				// since passing them directly in a
				// call to Pio can be invalid because
				// Pio passes data by reference
      Key ik = (*iter).first;
      Data dk = (*iter).second;
      Pio(stream, ik);
      Pio(stream, dk);
    }
    
  }

  stream.end_class();
  
}

//////////
// PIO for vectors
#define STLVECTOR_VERSION 1

template <class T> 
SCICORESHARE void Pio(Piostream& stream, vector<T>& data)
{ 
  
  stream.begin_class("STLVector", STLVECTOR_VERSION);
  
  int size=data.size();
  stream.io(size);
  
  if(stream.reading()){
    data.resize(size);
  }
  
  vector<T>::iterator ii;
  
  for (ii=data.begin(); ii!=data.end(); ii++)
    Pio(stream, *ii);

  stream.end_class();  
}

//////////
// PIO for lists
#define STLLIST_VERSION 1

template <class T> 
SCICORESHARE void Pio(Piostream& stream, list<T>& data)
{ 
  stream.begin_cheap_delim();
  
  int size=data.size();
  stream.io(size);
  
  if(stream.reading()){
    data.resize(size);
  }
  
  list<T>::iterator ii;
  for (ii=data.begin(); ii!=data.end(); ii++)
    Pio(stream, *ii);
     
  stream.end_cheap_delim();  
}


} // End namespace SCIRun

#endif // SCI_project_PersistentSTL_h

