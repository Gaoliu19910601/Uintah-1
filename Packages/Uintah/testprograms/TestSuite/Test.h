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


/**************************************
 * Test.h
 * Wayne Witzel
 * SCI Institute,
 * University of Utah
 *
 * Class Test:
 * Contains the name and results of a
 * regression test for reporting in a Suite
 * (see Suite.h).
 *
 ****************************************/

#include <sgi_stl_warnings_off.h>
#include <string>
#include <sgi_stl_warnings_on.h>
#include <Packages/Uintah/testprograms/TestSuite/uintahshare.h>
using namespace std;

class UINTAHSHARE Test
{
public:
  Test(const string name);

  ~Test(); // delete space allocated for name

  const string getName()
  { return myName; }

  // set the results of the test:
  // hasBeenRun() will return true,
  // hasPassed() will return true only if setResults
  // has been given a true argument for the test each
  // time it as been called.
  void setResults(bool passed);

  // true iff this test has been run (setResults called).
  bool hasBeenRun()
  { return myHasBeenRun; }

  // true iff setResults has been called and it has been
  // given a true passed value each time it has been called.
  bool hasPassed()
  { return myHasPassed; }

private:
  string myName;

  // true iff this test has been run 
  // (that is, setResults has ever been called for this object)
  bool myHasBeenRun; 

  // true iff setResults has been called and it has been
  // given a true passed value each time it has been called.
  bool myHasPassed;  
};
