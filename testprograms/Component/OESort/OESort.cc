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
 *  OESort.cc
 *
 *  Written by:
 *   Kostadin Damevski
 *   Department of Computer Science
 *   University of Utah
 *   October, 2002
 *
 *  Copyright (C) 2002 U of U
 */

#include <iostream>
#include <stdlib.h>
#include <algo.h>
#include <mpi.h>
#include <Core/CCA/Component/PIDL/PIDL.h>
#include <Core/CCA/Component/PIDL/MxNArrayRep.h>
#include <Core/CCA/Component/PIDL/MalformedURL.h>

#include <testprograms/Component/OESort/OESort_impl.h>
#include <testprograms/Component/OESort/OESort_sidl.h>
#include <Core/Thread/Time.h>

#define ARRSIZE 1000

using std::cerr;
using std::cout;

using namespace SCIRun;

void usage(char* progname)
{
    cerr << "usage: " << progname << " [options]\n";
    cerr << "valid options are:\n";
    cerr << "  -server  - server process\n";
    cerr << "  -client URL  - client process\n";
    cerr << "\n";
    exit(1);
}

void init(CIA::array1<int>& a, int s)
{
  a.resize(s);
  a[0] = 1;
  for(int i=1;i<s;i++) {
    a[i] = a[i-1] + (rand() % 10);
  }
}


int main(int argc, char* argv[])
{
    using std::string;

    using OESort_ns::OESort_impl;
    using OESort_ns::OESort;

    CIA::array1<int> arr1;
    CIA::array1<int> arr2;
    CIA::array1<int> result_arr;

    int myrank = 0;
    int mysize = 0;

    try {
        PIDL::initialize(argc,argv);
        
        MPI_Init(&argc,&argv);

	bool client=false;
	bool server=false;
	vector <URL> server_urls;
	int reps=1;

        MPI_Comm_size(MPI_COMM_WORLD,&mysize);
        MPI_Comm_rank(MPI_COMM_WORLD,&myrank);

	for(int i=1;i<argc;i++){
	    string arg(argv[i]);
	    if(arg == "-server"){
		if(client)
		    usage(argv[0]);
		server=true;
	    } else if(arg == "-client"){
		if(server)
		    usage(argv[0]);
                i++;
                for(;i<argc;i++){		
                  string url_arg(argv[i]);
		  server_urls.push_back(url_arg);
                }
		client=true;
                break;
	    } else if(arg == "-reps"){
		if(++i>=argc)
		    usage(argv[0]);
		reps=atoi(argv[i]);
	    } else {
		usage(argv[0]);
	    }
	}
	if(!client && !server)
	    usage(argv[0]);

	if(server) {
	  OESort_impl* pp=new OESort_impl;

          //Set up server's requirement of the distribution array 
          Index** dr0 = new Index* [1];
          dr0[0] = new Index(myrank,ARRSIZE,2);
          MxNArrayRep* arrr0 = new MxNArrayRep(1,dr0);
	  pp->setCalleeDistribution("O",arrr0);

          Index** dr1 = new Index* [1];
          dr1[0] = new Index(abs(myrank-1),ARRSIZE,2);
          MxNArrayRep* arrr1 = new MxNArrayRep(1,dr1);
	  pp->setCalleeDistribution("E",arrr1);
 
	  Index** dr2 = new Index* [1];
          dr2[0] = new Index(myrank,((ARRSIZE*2)-1),2);
          MxNArrayRep* arrr2 = new MxNArrayRep(1,dr2);
	  pp->setCalleeDistribution("X",arrr2);
          std::cerr << "setCalleeDistribution completed\n";

	  cerr << "Waiting for OESort connections...\n";
	  cerr << pp->getURL().getString() << '\n';

	} else {

          //Creating a multiplexing proxy from all the URLs
	  Object::pointer obj=PIDL::objectFrom(server_urls,mysize,myrank);
	  OESort::pointer pp=pidl_cast<OESort::pointer>(obj);
	  if(pp.isNull()){
	    cerr << "pp_isnull\n";
	    abort();
	  }

	  //Set up the array and the timer  
          init(arr1,ARRSIZE);
	  init(arr2,ARRSIZE);

	  //Inform everyone else of my distribution
          //(this sends a message to all the callee objects)
          Index** dr0 = new Index* [1];
          dr0[0] = new Index(0,ARRSIZE,1);
          MxNArrayRep* arrr0 = new MxNArrayRep(1,dr0);
	  pp->setCallerDistribution("O",arrr0);

          Index** dr1 = new Index* [1];
          dr1[0] = new Index(0,ARRSIZE,1);
          MxNArrayRep* arrr1 = new MxNArrayRep(1,dr1);
	  pp->setCallerDistribution("E",arrr1);
 
	  Index** dr2 = new Index* [1];
          dr2[0] = new Index(0,((ARRSIZE*2)-1),1);
          MxNArrayRep* arrr2 = new MxNArrayRep(1,dr2);
	  pp->setCallerDistribution("X",arrr2);
	  std::cerr << "setCallerDistribution completed\n";
	  
          double stime=Time::currentSeconds();

	  /*Odd-Even merge together two sorted arrays*/
	  pp->sort(arr1,arr2,result_arr);
	  
	  /*Pairwise check of merged array:*/
	  for(unsigned int l=0; l+1 < result_arr.size() ;l+=2)
	    if (result_arr[l] > result_arr[l+1]) {
	      int t = result_arr[l];
	      result_arr[l] = result_arr[l+1];
	      result_arr[l+1] = t;
	  }

	  for(unsigned int i=0;i<result_arr.size();i++){
	    cerr << result_arr[i] << "  ";
	  }
	  double dt=Time::currentSeconds()-stime;
	  cerr << reps << " reps in " << dt << " seconds\n";
	  double us=dt/reps*1000*1000;
	  cerr << us << " us/rep\n";
	}
    } catch(const MalformedURL& e) {
	cerr << "OESort.cc: Caught MalformedURL exception:\n";
	cerr << e.message() << '\n';
    } catch(const Exception& e) {
	cerr << "OESort.cc: Caught exception:\n";
	cerr << e.message() << '\n';
	abort();
    } catch(...) {
	cerr << "Caught unexpected exception!\n";
	abort();
    }
    PIDL::serveObjects();

    MPI_Finalize();

    return 0;
}

