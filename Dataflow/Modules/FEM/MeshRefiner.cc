
/*
 *  MeshRefiner.cc: Evaluate the error in a finite element solution
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   May 1996
 *
 *  Copyright (C) 1996 SCI Group
 */

#include <Dataflow/Network/Module.h>
#include <Dataflow/Ports/MeshPort.h>
#include <Dataflow/Ports/BooleanPort.h>
#include <Dataflow/Ports/IntervalPort.h>
#include <Dataflow/Ports/ScalarFieldPort.h>
#include <Core/Datatypes/ScalarFieldUG.h>
#include <Core/Geometry/Point.h>
#include <Core/TclInterface/TCLvar.h>
#include <iostream>
using std::cerr;
using std::endl;

namespace SCIRun {


class MeshRefiner : public Module {
    ScalarFieldIPort* upbound_field;
    ScalarFieldIPort* lowbound_field;
    IntervalIPort* interval_iport;
    MeshOPort* mesh_oport;
    sciBooleanOPort* cond_oport;
public:
    MeshRefiner(const clString& id);
    virtual ~MeshRefiner();
    virtual void execute();
};

extern "C" Module* make_MeshRefiner(const clString& id) {
  return new MeshRefiner(id);
}

MeshRefiner::MeshRefiner(const clString& id)
: Module("MeshRefiner", id, Filter)
{
    lowbound_field=new ScalarFieldIPort(this, "Lower bound",
					ScalarFieldIPort::Atomic);
    add_iport(lowbound_field);
    upbound_field=new ScalarFieldIPort(this, "Upper bound",
				       ScalarFieldIPort::Atomic);
    add_iport(upbound_field);
    interval_iport=new IntervalIPort(this, "Error interval",
				     IntervalIPort::Atomic);
    add_iport(interval_iport);

    // Create the output port
    mesh_oport=new MeshOPort(this, "Refined mesh",
			     MeshIPort::Atomic);
    add_oport(mesh_oport);
    cond_oport=new sciBooleanOPort(this, "Stopping condition",
				   sciBooleanIPort::Atomic);
    add_oport(cond_oport);
}

MeshRefiner::~MeshRefiner()
{
}

void MeshRefiner::execute()
{
  cerr << "MeshRefiner executing...\n";
    ScalarFieldHandle lowf;
    if(!lowbound_field->get(lowf))
        return;
    ScalarFieldHandle upf;
    if(!upbound_field->get(upf))
        return;
    IntervalHandle interval;
    if(!interval_iport->get(interval))
        return;
    ScalarFieldUG* lowfug=lowf->getUG();
    if(!lowfug){
	error("ComposeError can't deal with this field");
	return;
    }
    ScalarFieldUG* upfug=upf->getUG();
    if(!upfug){
	error("ComposeError can't deal with this field");
	return;
    }
    if(upfug->mesh.get_rep() != lowfug->mesh.get_rep()){
        error("Two different meshes...\n");
	return;
    }

    MeshHandle mesh(upfug->mesh);
    //double* low=&lowfug->data[0];
    double* up=&upfug->data[0];
    double upper_bound=interval->high;
    Array1<Point> new_points;
    int nelems=mesh->elems.size();
    int i;
    for(i=0;i<nelems;i++){
        if(up[i] > upper_bound){
	    new_points.add(mesh->elems[i]->centroid());
	}
	if(i%10000 == 0)
	  update_progress(i, 2*nelems);
    }
    if(new_points.size() == 0){
        mesh_oport->send(mesh);
	cond_oport->send(new sciBoolean(1));
	return;
    }

    mesh.detach();
    mesh->detach_nodes();
    mesh->compute_face_neighbors();
    cond_oport->send(new sciBoolean(0));
    cerr << "There are " << new_points.size() << " new points\n";
    for(i=0;i<new_points.size();i++){
        if(!mesh->insert_delaunay(new_points[i]))
	    cerr << "Error inserting point: " << new_points[i] << endl;
	if(i%1000 == 0)
	  update_progress(new_points.size()+i, 2*new_points.size());
    }
    mesh->pack_all();
    cerr << "There are now " << mesh->nodes.size() << " nodes and " << mesh->elems.size() << " elements\n";
    mesh_oport->send(mesh);
}

} // End namespace SCIRun

