
/*
 *  TensorFieldReader.cc: TensorField Reader class
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   July 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Packages/DaveW/Core/Datatypes/General/TensorFieldPort.h>
#include <Packages/DaveW/Core/Datatypes/General/TensorField.h>
#include <Dataflow/Network/Module.h>
#include <Core/Malloc/Allocator.h>
#include <Core/TclInterface/TCLTask.h>
#include <Core/TclInterface/TCLvar.h>

namespace DaveW {
using namespace DaveW;
using namespace SCIRun;

class TensorFieldReader : public Module {
    TensorFieldOPort* outport;
    TCLstring filename;
    TensorFieldHandle handle;
    clString old_filename;
public:
    TensorFieldReader(const clString& id);
    virtual ~TensorFieldReader();
    virtual void execute();
};

extern "C" Module* make_TensorFieldReader(const clString& id) {
  return new TensorFieldReader(id);
}

TensorFieldReader::TensorFieldReader(const clString& id)
: Module("TensorFieldReader", id, Source), filename("filename", id, this)
{
    // Create the output data handle and port
    outport=scinew TensorFieldOPort(this, "Output Data", TensorFieldIPort::Atomic);
    add_oport(outport);
}

TensorFieldReader::~TensorFieldReader()
{
}

#ifdef BROKEN
static void watcher(double pd, void* cbdata)
{
    TensorFieldReader* reader=(TensorFieldReader*)cbdata;
    if(TCLTask::try_lock()){
	// Try the malloc lock once before we call update_progress
	// If we can't get it, then back off, since our caller might
	// have it locked
	if(!Task::test_malloc_lock()){
	    TCLTask::unlock();
	    return;
	}
	reader->update_progress(pd);
	TCLTask::unlock();
    }
}
#endif

void TensorFieldReader::execute()
{

    clString fn(filename.get());
    if(!handle.get_rep() || fn != old_filename){
	old_filename=fn;
	Piostream* stream=auto_istream(fn);
	if(!stream){
	    error(clString("Error reading file: ")+filename.get());
	    return; // Can't open file...
	}
	// Read the file...
//	stream->watch_progress(watcher, (void*)this);
	Pio(*stream, handle);
	if(!handle.get_rep() || stream->error()){
	    error("Error reading TensorField from file");
	    delete stream;
	    return;
	}
	delete stream;
    }
    outport->send(handle);
}
} // End namespace DaveW


