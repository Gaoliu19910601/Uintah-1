
/*
 *  VoiceRemover.cc:
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   May 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <VoiceRemover/VoiceRemover.h>
#include <ModuleList.h>
#include <MUI.h>
#include <NotFinished.h>
#include <SoundPort.h>
#include <iostream.h>

static Module* make_VoiceRemover()
{
    return new VoiceRemover;
}

static RegisterModule db1("Sound", "VoiceRemover", make_VoiceRemover);

VoiceRemover::VoiceRemover()
: UserModule("VoiceRemover", Sink)
{
    // Create the input port
    isound=new SoundIPort(this, "Input",
			  SoundIPort::Atomic|SoundIPort::Stream);
    add_iport(isound);

    // Create the output port
    osound=new SoundOPort(this, "Output",
			  SoundIPort::Atomic|SoundIPort::Stream);
    add_oport(osound);
}

VoiceRemover::VoiceRemover(const VoiceRemover& copy, int deep)
: UserModule(copy, deep)
{
    NOT_FINISHED("VoiceRemover::VoiceRemover");
}

VoiceRemover::~VoiceRemover()
{
}

Module* VoiceRemover::clone(int deep)
{
    return new VoiceRemover(*this, deep);
}

void VoiceRemover::execute()
{
    if(!isound->is_stereo()){
	error("VoiceRemover only works on stereo sound");
	return;
    }
    int nsamples=isound->nsamples();
    double rate=isound->sample_rate();
    if(nsamples==-1){
	nsamples=(int)(rate*10);
    } else {
	osound->set_nsamples(nsamples);
    }
    osound->set_stereo(0);
    osound->set_sample_rate(rate);

    int sample=0;
    while(!isound->end_of_stream()){
	// Read a sound sample
	double l=isound->next_sample();
	double r=isound->next_sample();
	double s=l-r;
	osound->put_sample(s);

	// Tell everyone how we are doing...
	update_progress(sample++, nsamples);
	if(sample >= nsamples)
	    sample=0;
    }
}
 
