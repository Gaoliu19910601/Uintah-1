
/*
 *  EditMatrix.cc:  Visual matrix editor
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   November 1995
 *
 *  Copyright (C) 1995 SCI Group
 */

#include <Core/Containers/String.h>
#include <Dataflow/Network/Module.h>
#include <Core/Datatypes/DenseMatrix.h>
#include <Core/Datatypes/Matrix.h>
#include <Dataflow/Ports/MatrixPort.h>
#include <Core/Datatypes/SymSparseRowMatrix.h>
#include <Core/Malloc/Allocator.h>
#include <Core/TclInterface/TCLvar.h>
#include <Core/TclInterface/TCL.h>
#include <tcl.h>
#include <iostream>
using std::cerr;
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace SCIRun {


class EditMatrix : public Module {
    TCLstring Matrixtype;
    TCLstring TclMat;
    TCLint nrow;
    TCLint ncol;
    MatrixIPort* iport;
    MatrixOPort* oport;
    char *word;
    int loadFlag;
    int sendFlag;
    MatrixHandle omat;
    MatrixHandle imat;
public:
    EditMatrix(const clString& id);
    virtual ~EditMatrix();
    virtual void execute();
    void tcl_command( TCLArgs&, void * );
};

extern "C" Module* make_EditMatrix(const clString& id) {
  return new EditMatrix(id);
}

EditMatrix::EditMatrix(const clString& id)
: Module("EditMatrix", id, Source), Matrixtype("Matrixtype", id, this),
  TclMat("TclMat", id, this), nrow("nrow", id, this), ncol("ncol", id, this)
{
    // Create the input port
    iport=scinew MatrixIPort(this, "Matrix", MatrixIPort::Atomic);
    add_iport(iport);
    // Create the output port
    oport=scinew MatrixOPort(this, "Matrix", MatrixIPort::Atomic);
    add_oport(oport);
    loadFlag=sendFlag=1;
    word=0;
}

EditMatrix::~EditMatrix()
{
}

void EditMatrix::execute()
{
    int have_it=iport->get(imat);
    if (loadFlag) {
//	cerr << "Caught load flag\n";
	loadFlag=0;
	if (have_it && imat.get_rep()) {
	    int r,c;
	    Array1<clString> a(imat->nrows()*imat->ncols());
	    for (r=0; r<imat->nrows(); r++) {
		for (c=0; c<imat->ncols(); c++) {
		    a[r*imat->ncols()+c] = to_string(imat->get(r,c));
		}
	    }
	    int space=0;
	    for (r=0; r<a.size(); r++) {
		space += a[r].len() + 1;
	    }
	    if (word) free(word);
//	    cerr << "Allocating " << space+1 << " chars.\n";
	    word = (char *) malloc (sizeof(char)*(space+2));
	    int curr=0;
	    for (r=0; r<a.size(); r++) {
		strncpy(&(word[curr]), (a[r])(), a[r].len());
		curr+=a[r].len();
		word[curr]=' ';
		curr++;
	    }
	    word[curr]='\0';
//	    cerr << "Here's the outgoing string: " << word << "\n";
	    TclMat.set(word);
	    nrow.set(imat->nrows());
	    ncol.set(imat->ncols());
	    cerr << "Loaded type = "<<imat->getType()<<"\n";
	    DenseMatrix *dm=imat->getDense();
	    if (dm){
		cerr << "Loaded matrix is dense\n";
		Matrixtype.set("dense");
	    } else {
		cerr << "Loaded matrix isn't dense\n";
		Matrixtype.set("symsparse");
	    }
cerr << "Done with c++, calling tcl_load...\n";
	    TCL::execute(id+" tcl_load");
	}
    }
    if (sendFlag) {
//	cerr << "Caught send flag\n";
	sendFlag=0;
	const char *m=(TclMat.get())();
	int nr = nrow.get();
	int nc = ncol.get();
	if (strlen(m)<nr*nc) return;
	clString st(Matrixtype.get());
	cerr << "st=" << st << "\n";
//	cerr << "m=" << m << "\n";
//	cerr << "Here's TclMat: "<<TclMat.get()<<"\n";
	if (word) free(word);
	char *word = (char *) malloc (sizeof(char) * strlen(m+1));
	// Dd: Using "word" instead of "m" below... should work...
	strcpy(word, m);
//	cerr << "r=" <<r<<"  c="<<c << "\n";
	char *next;
	if (st == "dense") {
	    DenseMatrix* dm = scinew DenseMatrix(nr, nc);
	    int r,c;
	    for (r=0; r<nr; r++) {
		for (c=0; c<nc; c++) {
		    if (r==0 && c==0) {
			next=strtok(word, " ");
		    } else {
			next=strtok((char *)0, " ");
		    }
//		    cerr << "Read matrix element("<<r<<","<<c<<"): "<<atof(next)<<"\n";
		    dm->put(r,c,atof(next));
		}
	    }
	    cerr << "Sending a dense matrix.\n";
	    omat = dm;
	} else {
	    Array1<double> val;
	    Array1<int> ridx;
	    Array1<int> cidx;
	    int r,c;
	    for (r=0; r<nr; r++) {
		for (c=0; c<nc; c++) {
		    if (r==0 && c==0) {
			next=strtok(word, " ");
		    } else {
			next=strtok((char *)0, " ");
		    }
		    double v=atof(next);
		    if (v!=0) {
			val.add(v);
			ridx.add(r);
			cidx.add(c);
		    }
		}
	    }
	    Array1<int> in_rows;
	    int z;
	    if (ridx.size()!=0) {
		int curr_r=-1;
		for (z=0; z<ridx.size(); z++) {
		    if (curr_r != ridx[z]) {
			curr_r++;
			in_rows.add(z);
			z--;
		    }
		}
		for (z=in_rows.size(); z<=nrow.get(); z++) {
		    in_rows.add(ridx.size());
		}
	    }
//	    cerr << "SSRM: nrow="<<nrow.get()<<" ncol="<<ncol.get()<<"\n";
//	    cerr << "  in_rows("<<in_rows.size()<<"): ";
//	    int i;
//	    for (i=0; i<in_rows.size(); i++) {
//		cerr <<in_rows[i]<<" ";
//	    }
//	    cerr << "\n  in_cols("<<cidx.size()<<"): ";
//	    for (i=0; i<cidx.size(); i++) {
//		cerr <<cidx[i]<<" ";
//	    }
//	    cerr << "\n\n";
	    SymSparseRowMatrix* ssm = 
		scinew SymSparseRowMatrix(nrow.get(), ncol.get(), in_rows, cidx);
	    for (z=0; z<val.size(); z++)
		ssm->put(ridx[z], cidx[z], val[z]);
	    cerr << "Sending a sparse matrix.\n";
	    omat = ssm;
	}
    }
    oport->send(omat);
}

void EditMatrix::tcl_command(TCLArgs& args, void* userdata) {
    if (args[1] == "load") {
	loadFlag=1;
	want_to_execute();
    } else if (args[1] == "send") {
	sendFlag=1;
	want_to_execute();
    } else {
        Module::tcl_command(args, userdata);
    }
}

} // End namespace SCIRun

