
/*
 *  Network.cc: The core of dataflow...
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Distributed modifications by:
 *   Michelle Miller
 *   Dec. 1997
 *  Copyright (C) 1994 SCI Group
 */

#include <Dataflow/Network.h>

#include <Classlib/NotFinished.h>
#include <Dataflow/Connection.h>
#include <Dataflow/Module.h>
#include <Dataflow/ModuleList.h>
#include <Malloc/Allocator.h>
#include <TCL/Remote.h>

#include <stdio.h>
#include <iostream.h>

//#define DEBUG 1

extern int setupConnect (int port);
extern int acceptConnect (int socket);

Network::Network(int first):
netedit(0), first(first), slave_socket(0), nextHandle(0)
{
}

Network::~Network()
{
    if (slave_socket != 0)
	close (slave_socket);
}

int Network::read_file(const clString&)
{
    NOT_FINISHED("Network::read_file");
    return 1;
}

// For now, we just use a simple mutex for both reading and writing
void Network::read_lock()
{
    the_lock.lock();
}

void Network::read_unlock()
{
    the_lock.unlock();
}

void Network::write_lock()
{
    the_lock.lock();
}

void Network::write_unlock()
{
    the_lock.unlock();
}

int Network::nmodules()
{
    return modules.size();
}

Module* Network::module(int i)
{
    return modules[i];
}

int Network::nconnections()
{
    return connections.size();
}

Connection* Network::connection(int i)
{
    return connections[i];
}

clString Network::connect(Module* m1, int p1, Module* m2, int p2)
{
    Connection* conn=scinew Connection(m1, p1, m2, p2);
    clString id(m1->id+"_p"+to_string(p1)+"_to_"+m2->id+"_p"+to_string(p2));
    conn->id=id;
    conn->connect();
    connections.add(conn);
    // Reschedule next time we can...
    reschedule=1;

    // socket already set up
    Message msg;

    if (!m1->isSkeleton() && !m2->isSkeleton()) {
    }  	// do nothing - normal case
    else if (m1->isSkeleton() && m2->isSkeleton()) {

	// format CreateLocalConnectionMsg control message
        msg.type        = CREATE_LOC_CONN;
	msg.u.clc.outModHandle = m1->handle;
	msg.u.clc.oport = p1;
	msg.u.clc.inModHandle = m2->handle;
	msg.u.clc.iport = p2;
        strcpy (msg.u.clc.connID, id());
        msg.u.clc.connHandle = conn->handle = getNextHandle();
        conn_handles.insert(conn->handle, conn);

	// send control message
        char buf[BUFSIZE];
        bzero (buf, sizeof (buf));
        bcopy ((char *) &msg, buf, sizeof (msg));
        write (slave_socket, buf, sizeof(buf));
    } else {
	conn->setRemote();

	// format CreateRemoteConnectionMsg control message
        msg.type        = CREATE_REM_CONN;
	if (m1->isSkeleton()) {
	    msg.u.crc.fromRemote = true; 	// connection from slave
	    m2->handle = getNextHandle();	// assign handle to local mod
	    mod_handles.insert(m2->handle, m2); // take this out - don't need
	} else {
	    msg.u.crc.fromRemote = false; 	// connection from master
	    m1->handle = getNextHandle();	// assign handle to local mod
	    mod_handles.insert(m1->handle, m1); // take this out - don't need
	}
	    
        msg.u.crc.outModHandle = m1->handle;
        msg.u.crc.oport = p1;
        msg.u.crc.inModHandle = m2->handle;
        msg.u.crc.iport = p2;
        strcpy (msg.u.crc.connID, id());
        msg.u.crc.connHandle = conn->handle = getNextHandle();
        conn_handles.insert(conn->handle, conn);
	msg.u.crc.socketPort = conn->socketPort = BASE_PORT + conn->handle;

	// send control message
        char buf[BUFSIZE];
        bzero (buf, sizeof (buf));
        bcopy ((char *) &msg, buf, sizeof (msg));
        write (slave_socket, buf, sizeof(buf));

	// setup data connection
	int listen_socket = setupConnect (conn->socketPort);
	conn->remSocket = acceptConnect (listen_socket);
	close (listen_socket);
    }

    return id;
}

int Network::disconnect(const clString& connId)
{
    Message msg;
    for (int i = 0; i < connections.size(); i++)
        if (connections[i]->id == connId)
		break;
    if (i == connections.size()) {
        return 0;
    }
 
    // endpoints on two different machines - format DeleteRemConnMsg
    if (connections[i]->isRemote()) {
        msg.type        = DELETE_REM_CONN;
        msg.u.drc.connHandle = connections[i]->handle;

    	// send message to slave
	char buf[BUFSIZE];
    	bzero (buf, sizeof (buf));
    	bcopy ((char *) &msg, buf, sizeof (msg));
    	write (slave_socket, buf, sizeof(buf));
	
    	// remove from handle hashtable
    	conn_handles.remove (connections[i]->handle);

    // only distrib connections will have a handle - format DeleteLocConnMsg
    } else if (connections[i]->handle) {
	msg.type 	= DELETE_LOC_CONN;
	msg.u.dlc.connHandle = connections[i]->handle;

    	// send message to slave
    	char buf[BUFSIZE];
    	bzero (buf, sizeof (buf));
    	bcopy ((char *) &msg, buf, sizeof (msg));
    	write (slave_socket, buf, sizeof(buf));

    	// remove from handle hashtable
    	conn_handles.remove (connections[i]->handle);
    } 
    // remove connection ref from iport and oport

    delete connections[i];	//connection destructor,tears down data channel
    connections.remove(i);
    return 1;
}

void Network::initialize(NetworkEditor* _netedit)
{
    netedit=_netedit;
    NOT_FINISHED("Network::initialize"); // Should read a file???
}

Module* Network::add_module(const clString& name)
{ 
    Message msg;

    makeModule maker=ModuleList::lookup(name);
    if(!maker){
	cerr << "Module: " << name << " not found!\n";
	return 0;
    }

    // Create the ID. Append counter to the name.
    int i=0;
    clString id;
    while(1){
	id=clString(name+"_"+to_string(i));
	if(get_module_by_id(id)){
	    i++;
	} else {
	    break;
	}
    }

#ifdef DEBUG
    cerr << "Network::add_module created ID\n";
#endif

    if (name(0) == 'r') {

#ifdef DEBUG
    	cerr << "Network::add_module remote module\n";
#endif
	// open listen socket and startup slave if not done yet
	if (slave_socket == 0) {
	    int listen_socket = setupConnect (BASE_PORT);

/* Thread "TCLTask"(pid 8291) caught signal SIGSEGV at address 6146200 (segmentation violation - Unknown code!)
	    // rsh to startup sr, passing master port number 
	    system ("rsh burn /a/home/sci/data12/mmiller/o_SCIRun/sr -slave burn 8888");
 */ 	    slave_socket = acceptConnect (listen_socket);
            close (listen_socket);
	}

	// send message to addModule (pass ID to link 2 instances);
	msg.type 	= CREATE_MOD;
	strcpy (msg.u.cm.name, name());
	strcpy (msg.u.cm.id, id());
	msg.u.cm.handle = getNextHandle();

	char buf[BUFSIZE];
	bzero (buf, sizeof (buf));
	bcopy ((char *) &msg, buf, sizeof (msg));
   	write (slave_socket, buf, sizeof(buf));

	// pass through to normal code - skeleton module? yes, need to do
	// this so the NetworkEditor calls for remote will execute.  if I
	// don't bind a NetworkEditor, how will the calls happen?  the other
	// side (daemon) should repeat code below also.
    }

    // calls the module constructor (name,id,SchedClass)
    Module* mod=(*maker)(id);	

    if (name(0) == 'r') {		// is this a remote module?
	mod->skeleton = true;		// tag as skeleton
    	mod->handle = msg.u.cm.handle;	// skeleton & remote same handle
    }
    modules.add(mod); 		// add to array of Module ptrs in Network

    // Binds NetworkEditor and Network instances to module instance.  
    // Instantiates ModuleHelper and starts event loop.
    mod->set_context(netedit, this);   

    // add Module id and ptr to Module to hashtable of modules in network
    module_ids.insert(mod->id, mod);

    // add to hashtable of handles and module ptrs
    if (mod->handle > 0)
    	mod_handles.insert(mod->handle, mod);
    return mod;
}

Module* Network::get_module_by_id(const clString& id)
{
    Module* mod;
    if(module_ids.lookup(id, mod)){
	return mod;
    } else {
	return 0;
    }
}

Module* Network::get_module_by_handle (int handle)
{
    Module* mod;
    if (mod_handles.lookup (handle, mod)) {
	return mod;
    } else {
	return 0;
    }
}

Connection* Network::get_connect_by_handle (int handle)
{
    Connection *conn;
    if (conn_handles.lookup (handle, conn)) {
  	return conn;
    } else {
	return 0;
    }
}

int Network::delete_module(const clString& id)
{
    Module* mod = get_module_by_id(id);
    if (!mod)
	return 0;
    
    if (mod->isSkeleton()) {

	// format deletemodule message
    	Message msg;
        msg.type        = DELETE_MOD;
        msg.u.dm.modHandle = mod->handle;

        // send msg to slave
        char buf[BUFSIZE];
        bzero (buf, sizeof (buf));
        bcopy ((char *) &msg, buf, sizeof (msg));
        write (slave_socket, buf, sizeof(buf));
	
	// remove from handle hashtable
  	mod_handles.remove (mod->handle);
    }

    // traverse array of ptrs to Modules in Network to find this module
    for (int i = 0; i < modules.size(); i++)
        if (modules[i] == mod)
	    break;
    if (i == modules.size())
	return 0;

    // remove array element corresponding to module, remove from hashtable
    modules.remove(i);
    module_ids.remove(id);
    delete mod;			
    return 1;
}

#ifdef __GNUG__
// Template instantiations
#include <Classlib/Array1.cc>
template class Array1<Connection*>;
template class Array1<Module*>;

#include <Classlib/HashTable.cc>
template class HashTable<clString, Module*>;
template class HashTable<int, Module*>;
template class HashTable<clString, Connection*>;
template class HashKey<clString, Module*>;
template class HashKey<int, Module*>;
template class HashKey<clString, Connection*>;
template int Hash(const clString& k, int hash_size);

#endif
