
/*
 *  Connection.cc: A Connection between two modules
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <iostream>
#include <stdio.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

using std::cerr;
using std::endl;

#include <Dataflow/Network/Connection.h>
#include <Dataflow/Network/Module.h>
#include <Dataflow/Network/Port.h>
#include <Core/Math/MinMax.h>
#include <Core/Math/MiscMath.h>

//#define DEBUG 1

namespace SCIRun {

Connection::Connection(Module* m1, int p1, Module* m2, int p2)
{
    // mm- hack to get remote connections to work, only have one ptr
    if (m1 != 0)
    	oport = m1->oport(p1);
    else
	oport = 0;
    if (m2 != 0)
    	iport=m2->iport(p2);
    else 
	iport = 0;
    local=1;
    connected=1;
    socketPort = 0;
    remSocket = 0;
    remote = false;
    handle = 0;
#if 0
    demand=0;
#endif
}

// mm- can't attach to a port ptr that is null
void Connection::remoteConnect()
{
    if (iport)
    	iport->attach(this);
    if (oport)
    	oport->attach(this);
    connected=1;
}

void Connection::connect()
{
    iport->attach(this);
    oport->attach(this);
    connected=1;
}

Connection::~Connection()
{
    if (connected) {
	iport->detach(this);
	oport->detach(this);
    }
   
    if (remSocket != 0) {
	close (remSocket);
#ifdef DEBUG
        cerr << "Connection::~Connection() just closed remote socket\n";
#endif
    }
}

Demand_Message::Demand_Message(Connection* conn)
  : MessageBase(MessageTypes::Demand), conn(conn)
{
}


Demand_Message::~Demand_Message()
{
}

} // End namespace SCIRun

