
/*
 *  cDMatrix.cc : ?
 *
 *  Written by:
 *   Author: ?
 *   Department of Computer Science
 *   University of Utah
 *   Date: ?
 *
 *  Copyright (C) 199? SCI Group
 */

#include <Core/Datatypes/cDMatrix.h>
#include <iostream>
using std::cerr;
using std::endl;
using std::ostream;
#include <fstream>
using std::complex;
using std::ifstream;

namespace SCIRun {

//-----------------------------------------------------------------

cDMatrix::cDMatrix(int N){
 int i,j;
 Size = N;

 a = new Complex*[N];

 Complex* p=new Complex[N*N];
  for(i=0;i<N;i++){
    a[i]=p;
    p+=N;
  }

 for(i=0;i<N;i++){
 for(j=0;j<N;j++)
  a[i][j] = 0;
 }
}

//----------------------------------------------------------------
cDMatrix::~cDMatrix(){

 delete[] a[0];

 delete [] a;
}

//----------------------------------------------------------------
cDMatrix::cDMatrix(const cDMatrix &B){
 int i,j;
 int N = B.Size;

 a = new Complex*[N];

  Complex* p=new Complex[N*N];
  for(i=0;i<N;i++){
    a[i]=p;
    p+=N;
  }

 Size = B.Size;

 for(i=0;i<N;i++){
 for(j=0;j<N;j++)
   a[i][j] = B.a[i][j];
 }
}

//----------------------------------------------------------------

cDMatrix& cDMatrix::operator=(const cDMatrix &B){
 int N = B.Size;
 int i,j;
  
 delete[] a[0];
 delete [] a;

 a = new Complex*[N];

  Complex* p=new Complex[N*N];
  for(i=0;i<N;i++){
    a[i]=p;
    p+=N;
  }


 Size = B.Size;

 for(i=0;i<N;i++){
 for(j=0;j<N;j++)
  a[i][j] = B.a[i][j];
 }

 return *this;
}

//-----------------------------------------------------------------
cDMatrix::Complex& cDMatrix::operator()(int i, int j){

if((i>=0)&&(i<Size)&&(j>=0)&&(j<Size)){
  return(a[i][j]);
 }

else 
    cerr <<"Error: Array index is out of range!"<<endl;
 return(a[0][0]);  
}

cDMatrix::Complex& cDMatrix::get(int i, int j) {
    return (*this)(i,j);
}

//-----------------------------------------------------------------
cDMatrix cDMatrix:: operator+(const cDMatrix& B) const{
 cDMatrix C(Size);

 for(int i = 0;i < Size;i++){
 for(int j = 0;j < Size;j++){
    
    C.a[i][j] = a[i][j] + B.a[i][j];
}}
 
return (C);
}

//-----------------------------------------------------------------
cDMatrix cDMatrix:: operator-(const cDMatrix& B) const{
 cDMatrix C(Size);

 for(int i = 0;i < Size;i++){
 for(int j = 0;j < Size;j++){
        
    C.a[i][j] = a[i][j] - B.a[i][j];
}}
 
return (C);
}

//-----------------------------------------------------------------
cDMatrix cDMatrix:: operator*(const cDMatrix& B) const{
 cDMatrix C(Size);

 for(int i=0;i<Size;i++){
 for(int k=0;k<Size;k++){
 for(int j=0;j<Size;j++){
    C.a[i][j] = C.a[i][j] + a[i][k]*B.a[k][j];
 }}}

 return (C);
}


//-----------------------------------------------------------------
int cDMatrix::load(char* filename){

std::ifstream file_in(filename);

 if(!file_in){
      cerr<<"Error:Cannot open input file:" <<filename<<endl;
  return(-1);
}

 for(int i = 0;i < Size;i++){
 for(int j = 0;j < Size;j++){
 
      file_in>>a[i][j];
 }}

 file_in.close();
 return (0);
 
}


//-----------------------------------------------------------------
ostream &operator<< (ostream &output, cDMatrix &A){

 for(int i=0 ;i < A.size();i++){
 output<<"[";
 for(int j=0 ;j < A.size();j++)
                output<<A(i,j)<<" ";
 output<<"]";
 output<<endl;
 }
 return(output);
}

//-----------------------------------------------------------------
cDMatrix operator*(cDMatrix::Complex x,const cDMatrix &B){
cDMatrix C(B.Size);

 for(int i=0;i<B.Size;i++){
 for(int j=0;j<B.Size;j++){
  C.a[i][j] = x*B.a[i][j];
 }}

 return (C);
}

//-----------------------------------------------------------------

cDMatrix operator*(const cDMatrix &B,cDMatrix::Complex x){
 cDMatrix C(B.Size);

 for(int i=0;i<B.Size;i++){
 for(int j=0;j<B.Size;j++){ 
  C.a[i][j]=x*B.a[i][j];
 }}

 return (C);
}

//---------------------------------------------------------------
cDMatrix operator*(double x,const cDMatrix &B){
cDMatrix C(B.Size);

 for(int i=0;i<B.Size;i++){
 for(int j=0;j<B.Size;j++){
  C.a[i][j] = x*B.a[i][j];
 }}

 return (C);
}

//-----------------------------------------------------------------

cDMatrix operator*(const cDMatrix &B,double x){
 cDMatrix C(B.Size);

 for(int i=0;i<B.Size;i++){
 for(int j=0;j<B.Size;j++){ 
  C.a[i][j]=x*B.a[i][j];
 }}

 return (C);
}
//-----------------------------------------------------------------

cVector cDMatrix::operator*(cVector &V){
 cVector V1(V.Size);
 int i,j;

 for(i=0;i<V.Size;i++){
   for (j=0;j<V.Size;j++)
       V1.a[i] = V1.a[i] + a[i][j]*V.a[j];
 }

 return V1;
}

//-----------------------------------------------------------------

void cDMatrix::mult(cVector& V,cVector& tmp){
  int i,j;

  for(i=0;i<V.Size;i++)
    tmp.a[i] = 0;
  
 for(i=0;i<Size;i++){
   for (j=0;j<Size;j++)
       tmp.a[i] = tmp.a[i] + a[i][j]*V.a[j];
 }

  for(i=0;i<V.Size;i++)
    V.a[i] = tmp.a[i];
  

}
//-----------------------------------------------------------------

} // End namespace SCIRun


