//static char *id="@(#) $Id$";

/*
 *  Persistent.h: Base class for persistent objects...
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   April 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <SCICore/Persistent/Persistent.h>
#include <SCICore/Persistent/Pstreams.h>
#include <SCICore/Containers/HashTable.h>
#include <SCICore/Containers/String.h>
#include <SCICore/Malloc/Allocator.h>
#include <SCICore/Multitask/Task.h>

namespace SCICore {
namespace PersistentSpace {

using SCICore::Containers::HashTable;
using SCICore::Multitask::Task;

static HashTable<clString, PersistentTypeID*>* table=0;

PersistentTypeID::PersistentTypeID(char* type, char* parent,
				   Persistent* (*maker)())
: type(type), parent(parent), maker(maker)
{
    if(!table){
	table=scinew HashTable<clString, PersistentTypeID*>;
    }
    clString typestring(type);
    PersistentTypeID* dummy;
    if(table->lookup(typestring, dummy)){
	if(dummy->maker == maker && clString(dummy->parent) == clString(parent)){
	    //cerr << "WARNING: duplicate type in Persistent Object Type Database: " << typestring << endl;
	} else {
	    cerr << "WARNING: duplicate type in Persistent Object Type Database: " << typestring << endl;
	    //cerr << "Makers: " << (void*)dummy->maker << ", " << (void*)maker << "\n";
	    //cerr << "Parents: " << dummy->parent << ", " << parent << "\n";
	    //exit(1);
	}
    }
    dummy=this;
    table->insert(typestring, dummy);
}

Persistent::~Persistent()
{
}

Piostream::Piostream(Direction dir, int version)
: dir(dir), version(version), err(0), outpointers(0), inpointers(0),
  current_pointer_id(1), timer_id(0)
{
}

Piostream::~Piostream()
{
    cancel_timers();
}

void Piostream::cancel_timers()
{
    if(timer_id){
	Task::self()->cancel_itimer(timer_id);
	timer_id=0;
    }
}

static void handle_itimer(void* cbdata)
{
    Piostream* stream=(Piostream*)cbdata;
    stream->do_itimer();
}

void Piostream::do_itimer()
{
    double pd=get_percent_done();
    (*timer_func)(pd, timer_data);
}

void Piostream::watch_progress(void (*tf)(double, void*), void* td)
{
    timer_func=tf;
    timer_data=td;
    // Start an interrupt timer which will monitor the progress of the
    // stream...
    timer_id=Task::self()->start_itimer(0.2, 0.2, handle_itimer, (void*)this);
}

int Piostream::reading()
{
    return dir==Read;
}

int Piostream::writing()
{
    return dir==Write;
}

int Piostream::error()
{
    return err;
}

static PersistentTypeID* find_derived(const clString& classname,
				      const clString& basename)
{
    if(!table)
	return 0;
    PersistentTypeID* pid;
    if(!table->lookup(classname, pid))
	return 0;
    if(!pid->parent)
	return 0;
    if(basename == pid->parent)
	return pid;
    if(find_derived(pid->parent, basename))
	return pid;
    return 0;
}

void Piostream::io(Persistent*& data, const PersistentTypeID& pid)
{
    if(err)
	return;
    if(dir==Read){
	int have_data;
	int pointer_id;
	data=0; // In case anything goes wrong...
	emit_pointer(have_data, pointer_id);
	if(have_data){
	    // See what type comes next in the stream.   If it
	    // Is a type derived from pid->type, then read it in
	    // Otherwise, it is an error...
	    clString in_name(peek_class());
	    clString want_name(pid.type);
	    Persistent* (*maker)()=0;
	    if(in_name == want_name){
		maker=pid.maker;
	    } else {
		PersistentTypeID* found_pid=find_derived(in_name, want_name);
		if(found_pid)
		    maker=found_pid->maker;
	    }
	    if(!maker){
		cerr << "Maker not found? (class=" << in_name << ")\n";
		err=1;
		return;
	    }

	    // Make it..
	    data=(*maker)();
	    // Read it in...
	    data->io(*this);

	    // Insert this pointer in the database
	    if(!inpointers)
		inpointers=scinew HashTable<int, Persistent*>;
	    inpointers->insert(pointer_id, data);
	} else {
	    // Look it up...
	    if(pointer_id==0){
		data=0;
	    } else {
		if(!inpointers || !inpointers->lookup(pointer_id, data)){
		    cerr << "Error - pointer not in file, but should be!\n";
		    err=1;
		    return;
		}
	    }
	}
    } else {
	int have_data;
	int pointer_id;
	if(data==0){
	    have_data=0;
	    pointer_id=0;
	} else if(outpointers && outpointers->lookup(data, pointer_id)){
	    // Already emitted, pointer id fetched from hashtable
	    have_data=0;
	} else {
	    // Emit it..
	    have_data=1;
	    pointer_id=current_pointer_id++;
	    if(!outpointers)
		outpointers=new HashTable<Persistent*, int>;
	    outpointers->insert(data, pointer_id);
	}
	emit_pointer(have_data, pointer_id);
	if(have_data)
	    data->io(*this);
    }
}

Piostream* auto_istream(const clString& filename)
{
    ifstream* inp=scinew ifstream(filename());
    return auto_istream(inp, filename());
}

Piostream* auto_istream(int fd)
{
  // Dd:
  printf("Persistent.cc: auto_istream using fd taken out for now.\n");
  //    ifstream* inp=scinew ifstream(fd);
  //    return auto_istream(inp);
  return NULL;
}

Piostream* auto_istream(ifstream* inp, const char *name)
{
  // Dd:
  printf("Persistent.cc: auto_istream(inp,name) using fd taken out for now.\n");
  //    ifstream* inp=scinew ifstream(fd);
  //    return auto_istream(inp);
  return NULL;
#if 0

    GzipPiostream *gzp=scinew GzipPiostream(name, 1);
    if (gzp->fileOpen()) return gzp;
    delete gzp;

    ifstream& in=(*inp);
    if(!in){
	cerr << "file not found: " << inp->rdbuf()->fd() << endl;
	return 0;
    }
    char m1, m2, m3, m4;
    // >> Won't work here - it eats white space...
    in.get(m1); in.get(m2); in.get(m3); in.get(m4);
    if(!in || m1 != 'S' || m2 != 'C' || m3 != 'I' || m4 != '\n'){
	cerr << inp->rdbuf()->fd() << " is not a valid SCI file! (magic=" << m1 << m2 << m3 << m4 << ")\n";
	return 0;
    }
    in.get(m1); in.get(m2); in.get(m3); in.get(m4);
    if(!in){
	cerr << "Error reading file: " << inp->rdbuf()->fd() << " (while readint type)" << endl;
	return 0;
    }
    int version;
    in >> version;
    if(!in){
	cerr << "Error reading file: " << inp->rdbuf()->fd() << " (while reading version)" << endl;
	return 0;
    }
    char m;
    do {
	in.get(m);
	if(!in){
	    cerr << "Error reading file: " << inp->rdbuf()->fd() << " (while reading newline)" << endl;
	    return 0;
	}
    } while(m != '\n');
    if(m1 == 'B' && m2 == 'I' && m3 == 'N'){
	return scinew BinaryPiostream(inp, version);
    } else if(m1 == 'A' && m2 == 'S' && m3 == 'C'){
	return scinew TextPiostream(inp, version);
    } else if(m1 == 'G' && m2 == 'Z' && m3 == 'P'){
	return scinew GunzipPiostream(inp, version);
    } else {
        cerr << name << " is an unknown type!\n";
        return 0;
    }
#endif
}

} // End namespace PersistentSpace
} // End namespace SCICore

//
// $log$
//
