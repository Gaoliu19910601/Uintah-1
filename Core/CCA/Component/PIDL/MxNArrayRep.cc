/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/


/*
 *  MxNArrayRep.cc 
 *
 *  Written by:
 *   Kostadin Damevski 
 *   Department of Computer Science
 *   University of Utah
 *   May 2002 
 *
 *  Copyright (C) 2002 SCI Group
 */

#include "MxNArrayRep.h"
using namespace SCIRun;

int SCIRun::gcd(int a, int b, int &x, int &y) {
  std::vector<int> q;
  int ret;
  if (a > b) {
    while ((a % b) != 0) {
      q.push_back((int)a/b);
      int t = a;
      a = b;
      b = t % a;      
    }
    ret = b;
  }
  else {
    while ((b % a) != 0) {
      q.push_back((int)b/a);
      int t = b;
      b = a;
      a = t % b;      
    }
    ret = a;
  }

  int size = q.size();
  int s[size+2];
  s[0] = 1; s[1]=0;
  for(int i=2; i < (size+2); i++) {
    s[i] = s[i-2] - (q[i-2]*s[i-1]);
  }

  int t[size+2];
  t[0] = 0; t[1]=1;
  for(int i=2; i < (size+2); i++) {
    t[i] = t[i-2] - (q[i-2]*t[i-1]);
  }  

  if (a > b) {
    x = s[size+1];
    y = t[size+1];
  } else {
    x = t[size+1];
    y = s[size+1];
  }
  return ret;
}

int SCIRun::crt(int a1, int m1, int a2, int m2) {
  int x,y;
  int m = m1 * m2;
  int M1 = m/m1;
  if (gcd(M1,m1,x,y) != 1) {
    //CRT does not apply in this case
    //return mod and hope for best
    return (a1 % m1); 
  }
  int M1_p = x % m1;
  int M2 = m/m2;
  if (gcd(M2,m2,x,y) != 1) {
    //CRT does not apply in this case
    //return mod and hope for best
    return (a2 % m2);
  }
  int M2_p = x % m2;
  return (((a1*M1*M1_p)+(a2*M2*M2_p)) % m);
}

int SCIRun::gcd(int m,int n) {
  if(n == 0)
    return m;
  else 
    return (SCIRun::gcd(n, m % n));
}

int SCIRun::lcm(int m,int n) {
  return ((m*n) / gcd(m,n));
}

MxNArrayRep::MxNArrayRep(int dimno, Index* dimarr[], Reference* remote_ref = NULL) 
  : mydimarr(dimarr), mydimno(dimno)
{
  if (remote_ref != NULL)  remoteRef = (*remote_ref);
  received = false;
}

MxNArrayRep::MxNArrayRep(CIA::array2<int>& arr, Reference* remote_ref = NULL) 
{
  mydimno = arr.size2();
  mydimarr = new Index* [mydimno];
  for(int i=0; i < mydimno; i++) {
    mydimarr[i] = new Index(arr[0][i],arr[1][i],arr[2][i]);
  }
  if (remote_ref != NULL)  remoteRef = (*remote_ref);
  received = false;
}

MxNArrayRep::~MxNArrayRep()
{
  //Free the dimension array:
  for(int i=0; i < mydimno ; i++) {
    delete mydimarr[i];
    mydimarr[i] = 0;
  }
}

CIA::array2<int> MxNArrayRep::getArray()
{
  CIA::array2<int> dist(3,mydimno);
  for(int i=0; i<mydimno; i++) {
    dist[0][i] = mydimarr[i]->myfirst;
    dist[1][i] = mydimarr[i]->mylast;
    dist[2][i] = mydimarr[i]->mystride;    
  }
  return dist;
}

unsigned int MxNArrayRep::getDimNum()
{
  return mydimno;
}

unsigned int MxNArrayRep::getFirst(int dimno)
{
  if (dimno <= mydimno)
    return mydimarr[dimno-1]->myfirst;
  else
    return 0;
}

unsigned int MxNArrayRep::getLast(int dimno)
{
  if (dimno <= mydimno)
    return mydimarr[dimno-1]->mylast;
  else
    return 0;
}

unsigned int MxNArrayRep::getStride(int dimno)
{
  if (dimno <= mydimno)
    return mydimarr[dimno-1]->mystride;
  else
    return 0;
}

unsigned int MxNArrayRep::getSize(int dimno)
{
  if (dimno <= mydimno) {
    int fst = mydimarr[dimno-1]->myfirst;
    int lst = mydimarr[dimno-1]->mylast;
    int str = mydimarr[dimno-1]->mystride;
    return (((lst - fst) / str) + 1);
  }
  else
    return 0;
}

Reference* MxNArrayRep::getReference()
{
  return (&remoteRef);
}

void MxNArrayRep::setRank(int rank)
{
  d_rank = rank;
}

int MxNArrayRep::getRank()
{
  return d_rank;
}

bool MxNArrayRep::isIntersect(MxNArrayRep* arep)
{
  bool intersect = true;

  if (arep != NULL) {
    //Call Intersect()
    MxNArrayRep* result = this->Intersect(arep);
    //Loop through the result to see if there is an intersection
    for(int j=1; j <= mydimno ; j++) {
      if (result->getFirst(j) > result->getLast(j)) {
	intersect = false;
	break;
      }
      if (result->getStride(j) < 1) {
	intersect = false;
	break;
      }
    }

    return intersect;
  }
  return false;
}

Index* MxNArrayRep::Intersect(MxNArrayRep* arep, int dimno)
{
  Index* intersectionRep = new Index(0,0,0);
  if (arep != NULL) {
    if (dimno <= mydimno) {

      int myfst,mylst,mystr;  //The data from this representation
      int fst,lst,str;        //The data from the intersecting representation

      //Get the representations to be interested:
      myfst = mydimarr[dimno-1]->myfirst;
      mylst = mydimarr[dimno-1]->mylast;
      mystr = mydimarr[dimno-1]->mystride;
      fst = arep->getFirst(dimno);
      lst = arep->getLast(dimno);
      str = arep->getStride(dimno);
      
      //Intersecting to find the first,last and stride of the intersection:

      //Calculate lcm of the strides:
      int lcm_str = lcm(str,mystr);
      intersectionRep->mystride = lcm_str;

      //Calculate the crt:
      int d_crt; 
      int d_gcd = gcd(str,mystr);
      if (d_gcd == 1)
        d_crt = crt(fst,str,myfst,mystr);
      else {
        if ((fst % d_gcd) == (myfst % d_gcd)) {
          d_crt = fst % d_gcd;
        } 
	else {
          //No Intersection
          intersectionRep->myfirst = 1;	
          intersectionRep->mylast = 0;

          std::cerr << "INTERSECTION: ";
          intersectionRep->print(std::cerr);

          return intersectionRep; 
        } 
      }
      while(d_crt < 0)  d_crt += lcm_str;
      std::cout << "CRT = " << d_crt << "\n";
      //Using the crt as the starting intersect, find the first intersect 
      //within the first and last pairs:
      int max_fst = std::max(myfst,fst);
      int t1 = max_fst - d_crt;
      if ((t1 % lcm_str) == 0)
	intersectionRep->myfirst = max_fst;
      else {
	t1 = (max_fst - d_crt);
	if ((t1 % lcm_str) == 0) 
	  t1 = t1 / lcm_str;
	else
	  t1 = (t1 / lcm_str) + 1;
	intersectionRep->myfirst = lcm_str * t1 + d_crt;
      }
      
      //Using the first intersect, find the last
      int min_lst = std::min(mylst,lst);
      int dif = min_lst - intersectionRep->myfirst;
      if (dif > 0) {
	if ((dif % lcm_str) == 0)
	  intersectionRep->mylast = min_lst;
	else
	  intersectionRep->mylast = min_lst - (dif % lcm_str);
      }
      else {
	//No Intersection, leave results as they will signify that also
	intersectionRep->mylast = min_lst;
      }
      std::cerr << "INTERSECTION: ";
      intersectionRep->print(std::cerr);
    }
  }
  return intersectionRep;
}

MxNArrayRep* MxNArrayRep::Intersect(MxNArrayRep* arep)
{
  Index** intersectionArr = new Index* [mydimno];
  for(int i=1; i<=mydimno; i++) {
    intersectionArr[i-1] = Intersect(arep, i);
  }
  return (new MxNArrayRep(mydimno,intersectionArr));
}

void MxNArrayRep::print(std::ostream& dbg)
{
  for(int i=0; i < mydimno ; i++) {
    mydimarr[i]->print(dbg);
  }  
}


//******************  Index ********************************************

Index::Index(unsigned int first, unsigned int last, unsigned int stride)
  : mystride(stride)
{
  if (first > last) {
    myfirst = last;
    mylast = first;
  }
  else {
    myfirst = first;
    mylast = last;
  }
}

void Index::print(std::ostream& dbg)
{
  dbg << myfirst << ", " << mylast << ", " << mystride << "\n";
}    

    











