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


/*
 *  restart_merger.cc
 *
 *  Merges uda directories where one has been restarted from another
 *  with the "-nocopy" option (which is default).  As a result, it will
 *  create a uda that would be just as if the "-copy" option were used
 *  (however, there are file system reasons why it is better to use this
 *  restart_merger afterwards than to use "-copy" in the first place).
 *
 *  Written by:
 *   WayneWitzel
 *   Department of Computer Science
 *   University of Utah
 *   July 2001
 *
 *  Copyright (C) 2001 U of U
 */

#include <Packages/Uintah/Core/DataArchive/DataArchive.h>
#include <Packages/Uintah/CCA/Components/DataArchiver/DataArchiver.h>
#include <Packages/Uintah/CCA/Ports/ProblemSpecInterface.h>
#include <Packages/Uintah/CCA/Components/ProblemSpecification/ProblemSpecReader.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Packages/Uintah/Core/Parallel/Parallel.h>
#include <Core/OS/Dir.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>


using namespace SCIRun;
using namespace std;
using namespace Uintah;

void usage(const std::string& badarg, const std::string& progname)
{
    if(badarg != "")
	cerr << "Error parsing argument: " << badarg << '\n';
    cerr << "Usage: " << progname << " [options] <uda dir 1> <uda dir 2> [<uda dir 3> ...]\n\n";
    cerr << "There can be any number of udas on the command line.\n";
    cerr << "Options:\n";
    cerr << "\t-copy\t(Default) Copies timestep directories into the new uda directory\n"
	 << "\t\twithout affecting the source uda directories.\n";
    cerr << "\t-move\tMoves timestep directories from the source udas directories into\n"
	 << "\t\tthe new uda directory and removes the source udas.\n"
	 << "\t\tThis option can be faster if the source and destination are\n"
	 << "\t\ton the same file system, but there may be data loss where the\n"
	 << "\t\tudas overlap.\n\n";
    cerr << "\t\t\t-move or -copy must be the first argument if specified.\n";
    cerr << "\n\nAssuming <uda dir n> was created by restarting from <uda dir n-1> \n";
    cerr << "with the -nocopy option (which is default for sus), this will create a\n";
    cerr << "new uda directory that is a continuous version of these uda directories.\n\n"; 
    exit(1);
}

int main(int argc, char** argv)
{
  bool move = false;
  int i = 1;
  for (i = 1; i < argc; i++) {
    string s = argv[i];
    if (s == "-copy") {
      move = false; // default anyway
    }
    else if (s == "-move") {
      move = true;
    }
    else
      break;
  }

  int n_udafiles = argc - i;
  if (n_udafiles < 2) {
    usage("", argv[0]);
  }
  
  char** udafile = &argv[i];
  string ups_filename = string(udafile[0]) + "/input.xml";

  bool thrownException = false;
  
  Uintah::Parallel::determineIfRunningUnderMPI( argc, argv );

  string new_uda_dir;
  try {
    ProblemSpecInterface* reader = scinew ProblemSpecReader(ups_filename);
    ProblemSpecP ups = reader->readInputFile();
    Uintah::Parallel::initializeManager(argc, argv, "");
    const ProcessorGroup* world = Uintah::Parallel::getRootProcessorGroup();
    DataArchiver out_uda(world);
    out_uda.problemSetup(ups, NULL);
    out_uda.initializeOutput(ups);
    new_uda_dir = out_uda.getOutputLocation();
    
    int timestep = 0;
    int prevTimestep = 0;
    int i;
    for (i = 0; i < n_udafiles-1; i++) { // each file except last
      DataArchive in_uda(udafile[i+1]);
      int old_timestep = timestep;
      if (!in_uda.queryRestartTimestep(timestep)) {
	cerr << endl << udafile[i+1] << " is not a restarted uda -- no \"restart\" tag found.\n";
	exit(1);
      }

      if (old_timestep > timestep) {
	cerr << endl << udafile[i] << " does not preceed " << udafile[i+1] << " with respect to restart timestep order.\n\n";
	usage("", argv[0]);
      }
      
      Dir restartFromDir(udafile[i]);
      // the time argument doesn't matter.  Pass in 0 also to signify to not copy checkpoints
      out_uda.restartSetup(restartFromDir, prevTimestep, timestep, 0 /* this time doesn't matter for our purpose here */, false, move);
      prevTimestep = timestep;
    }
   
    // copy all of the last uda timesteps 
    Dir restartFromDir(udafile[i]);
    out_uda.copySection(restartFromDir, "globals");
    out_uda.copySection(restartFromDir, "variables");
    // pass in an arbitrary '1' for time, so it can copy the checkpoints.
    out_uda.restartSetup(restartFromDir, prevTimestep, -1, 1, false, move);

    //ups->releaseDocument();
  } catch (Exception& e) {
    cerr << "Caught exception: " << e.message() << '\n';
    if(e.stackTrace())
      cerr << "Stack trace: " << e.stackTrace() << '\n';
    // Dd: I believe that these cause error messages
    // to be lost when the program dies...
    //Uintah::Parallel::finalizeManager(Uintah::Parallel::Abort);
    //abort();
    thrownException = true;
  } catch (std::exception e){
    cerr << "Caught std exception: " << e.what() << '\n';
    //Uintah::Parallel::finalizeManager(Uintah::Parallel::Abort);
    //abort();
    thrownException = true;
  }
  /*
  catch(...){
    cerr << "Caught unknown exception\n";
    //Uintah::Parallel::finalizeManager(Uintah::Parallel::Abort);
    //abort();
    thrownException = true;
  }
  */
  
  if (thrownException) {
    usage("", argv[0]);
  }

  cout << "Successfully created " << new_uda_dir << endl;
  return 0;
}
