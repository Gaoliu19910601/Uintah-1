/****************************************************************
 *  Simple "SGI_Solve module"for the Dataflow                      *
 *                                                              *
 *  Written by:                                                 *
 *   Leonid Zhukov                                              *
 *   Department of Computer Science                             *
 *   University of Utah                                         *
 *   November 1997                                              *
 *                                                              *
 *  Copyright (C) 1997 SCI Group                                *
 *                                                              *
 *                                                              *
 ****************************************************************/


#include <Dataflow/Network/Module.h>
#include <Dataflow/Ports/ColumnMatrixPort.h>
#include <Dataflow/Ports/MatrixPort.h>
#include <Core/TclInterface/TCLvar.h>

#include <iostream>
using std::cerr;
using std::cout;
using std::endl;
#include <time.h>

namespace DaveW {
using namespace SCIRun;

/*
extern "C" {
void PSLDLT_Solve ( int token,double x[],double b[]);
 void PSLDLT_Destroy (int token);
}
*/

class SGI_Solve : public Module {

ColumnMatrixIPort* factor_port;
ColumnMatrixIPort* rhs_port;
ColumnMatrixOPort* solution_port;
  
public:
 
  TCLstring tcl_status;
  SGI_Solve(const clString& id);
  SGI_Solve(const SGI_Solve&, int deep);
  virtual ~SGI_Solve();
  virtual void execute();
  
}; //class


extern "C" Module* make_SGI_Solve(const clString& id) {
    return new SGI_Solve(id);
}

//---------------------------------------------------------------
SGI_Solve::SGI_Solve(const clString& id)
  : Module("SGI_Solve", id, Filter),
    tcl_status("tcl_status",id,this)

{

 factor_port = new ColumnMatrixIPort(this,"factor port",ColumnMatrixIPort::Atomic);
 add_iport(factor_port);

 rhs_port = new ColumnMatrixIPort(this,"Column Matrix",ColumnMatrixIPort::Atomic);
 add_iport(rhs_port);

 solution_port=new ColumnMatrixOPort(this,"Column Matrix",ColumnMatrixIPort::Atomic);
 add_oport(solution_port);
    
}

//------------------------------------------------------------
SGI_Solve::~SGI_Solve(){}

//--------------------------------------------------------------

void SGI_Solve::execute()
{
  tcl_status.set("Calling SGI_Solve!");
  
 
  
  ColumnMatrixHandle rhsHandle;
  if(!rhs_port->get(rhsHandle))
    return; 
  
   
  ColumnMatrixHandle factorHandle;
  if(!factor_port->get(factorHandle))
    return; 


  int token = (factorHandle->get_rhs())[0];
  int nrows = (factorHandle->get_rhs())[1];
  

  double start,end;

  
  double* rhs = new double[nrows];
 double* sol = new double[nrows];

 int i;
  
  for(i=0;i<nrows;i++)
    rhs[i] = (rhsHandle->get_rhs())[i];
  
  
 cerr<<"Solve ...";
 start = time(0); 
// PSLDLT_Solve(token,sol,rhs);
 end = time(0); 
 cerr<<"...Done!  Time = "<<(end-start)<<" s"<<endl;


 // PSLDLT_Destroy(token); // this line was commented out for real


 cerr<<endl;

 cerr <<"Solution:"<<endl;
 for(i=0;i<10;i++)
   cout << sol[i]<<endl;

 
 ColumnMatrix* solution = new ColumnMatrix(nrows);
 solution->put_lhs(sol);

 
 solution_port->send(solution);
 
 
} 
//---------------------------------------------------------------
} // End namespace DaveW



