
/*
 *  ModuleHelper.cc:  Thread to execute modules..
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   April 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Dataflow/Network/ModuleHelper.h>

#include <Dataflow/Comm/MessageBase.h>
#include <Dataflow/Comm/MessageTypes.h>
#include <Dataflow/Network/Connection.h>
#include <Dataflow/Network/Module.h>
#include <Dataflow/Network/NetworkEditor.h>
#include <Dataflow/Network/Port.h>

#include <iostream>
#include <unistd.h>
using std::cerr;
using std::endl;

#define DEFAULT_MODULE_PRIORITY 90

namespace SCIRun {

ModuleHelper::ModuleHelper(Module* module)
: module(module)
{
}

ModuleHelper::~ModuleHelper()
{
}

void ModuleHelper::run()
{

    if(module->have_own_dispatch){
	module->do_execute();
    } else {
        module->pid_.set(getpid());
	for(;;){
	    MessageBase* msg=module->mailbox.receive();
	    switch(msg->type){
	    case MessageTypes::ExecuteModule:
		module->do_execute();
		break;
	    case MessageTypes::TriggerPort:
		{
		    Scheduler_Module_Message* smsg=(Scheduler_Module_Message*)msg;
		    smsg->conn->oport->resend(smsg->conn);
		}
		break;
	    case MessageTypes::Demand:
  	        {
#if 0
		    Demand_Message* dmsg=(Demand_Message*)msg;
		    if(dmsg->conn->oport->have_data()){
		        dmsg->conn->oport->resend(dmsg->conn);
		    } else {
		        dmsg->conn->demand++;
			while(dmsg->conn->demand)
			    module->do_execute();
		    }
#endif
		}
		break;
	    default:
		cerr << "Illegal Message type: " << msg->type << endl;
		break;
	    }
	    delete msg;
	}
    }
}

} // End namespace SCIRun

