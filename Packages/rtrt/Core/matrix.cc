/*

The MIT License

Copyright (c) 1997-2009 Center for the Simulation of Accidental Fires and 
Explosions (CSAFE), and  Scientific Computing and Imaging Institute (SCI), 
University of Utah.

License for the specific language governing rights and limitations under
Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

*/


// -*- c++ -*-
//
// $COPYRIGHT$
//
//===========================================================================
// thanks to Valient Gough for this example program!

#include <mtl/matrix.h>
#include <mtl/mtl.h>
#include <mtl/utils.h>
#include <mtl/lu.h>

using namespace mtl;
using namespace std;

// don't print out the matrices once they get to this size...
#define MAX_PRINT_SIZE 5

typedef matrix<double, rectangle<>, dense<>, row_major>::type Matrix;
typedef dense1D<double> Vector;

double testMatrixError(const Matrix &A, const Matrix &AInv)
{
  int size = A.nrows();

  // test it
  Matrix AInvA(size,size);

  // AInvA = AInv * A
  mult(AInv, A, AInvA);

  // I = identity
  typedef matrix<double, diagonal<>, packed<>, row_major>::type IdentMat;
  IdentMat I(size, size, 0, 0);
  mtl::set(I, 1.0);

  // AInvA += -I
  add(scaled(I, -1.0), AInvA);

  if (size < MAX_PRINT_SIZE) {
    cout << "Ainv * A - I = " << endl;
    print_all_matrix(AInvA);
  }

  // find max error
  double max_error = 0.0;
  for(Matrix::iterator i = AInvA.begin(); i != AInvA.end(); ++i)
    for(Matrix::Row::iterator j = (*i).begin(); j != (*i).end(); ++j)
      if(fabs(*j) > fabs(max_error))
        max_error = *j;
        
  cout << "max error = " << max_error << endl;

  return max_error;
}


void testLUSoln(const Matrix &A, const Vector &b, Vector &x)
{
  // create LU decomposition
  Matrix LU(A.nrows(), A.ncols());
  dense1D<int> pvector(A.nrows());

  copy(A, LU);
  lu_factorize(LU, pvector);
        
  // solve
  lu_solve(LU, pvector, b, x);
}

void testLUInv(const Matrix &A, int size)
{
  // invert it
  Matrix AInv(size,size);
        
  // create LU decomposition
  Matrix LU(A.nrows(), A.ncols());
  dense1D<int> pvector(A.nrows());

  copy(A, LU);
  lu_factor(LU, pvector);
        
  // solve
  lu_inverse(LU, pvector, AInv);
        

  if(size < MAX_PRINT_SIZE) {
    cout << "A = " << endl;
    print_all_matrix(A);
    cout << "Ainv = " << endl;
    print_all_matrix(AInv);
  }

  // test it
  testMatrixError(A, AInv);

}

int main(int argc, char **argv)
{
  typedef Matrix::size_type sizeT;

  const sizeT size = (argc > 1 ? atoi(argv[1]) : 5);

  cout << "inverting matrix of size " << size << endl;

  // create a random matrix and invert it.  Then see how close it comes to
  // identity.

  Matrix A(size,size);
  Vector b(size);
  Vector x(size);

  // initialize
  for (sizeT i=0; i<A.nrows(); i++) {
    for (sizeT j=0; j<A.nrows(); j++)
      A(i,j) = (double)(rand() % 200 - 100) / 50.0;
    b[i] = (double)(rand() % 200 - 100) / 50.0;
  }

  if (size < MAX_PRINT_SIZE) {
    cout << "A = " << endl;
    print_all_matrix(A);
  }

        
  // time LU inv
  cout << endl 
       << " ----------- testing inversion using LU decomposition" 
       << endl;
  testLUInv(A, size);

  if (size < MAX_PRINT_SIZE) {
    cout << "solution = ";
    print_vector(x);
  }
        
  // test LU solution
  mtl::set(x, 0.0);
  testLUSoln(A, b, x);

  if(size < MAX_PRINT_SIZE) {
    cout << "solution = ";
    print_vector(x);
  }

  if(argc == 1)
    cout << endl 
         << "pass size argument to program to time larger matrices." 
         << endl;

  return 0;
}
