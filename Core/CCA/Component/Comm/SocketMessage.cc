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
 *  SocketMessage.cc: Socket implemenation of Message
 *
 *  Written by:
 *   Kosta Damevski and Keming Zhang
 *   Department of Computer Science
 *   University of Utah
 *   Jun 2003
 *
 *  Copyright (C) 1999 SCI Group
 */



#include <stdlib.h>
#include <string>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


#include <iostream>
#include <Core/CCA/Component/Comm/CommError.h>
#include <Core/CCA/Component/Comm/SocketMessage.h>
#include <Core/CCA/Component/Comm/SocketEpChannel.h>
#include <Core/CCA/Component/Comm/SocketSpChannel.h>
#include <Core/CCA/Component/Comm/DT/DTMessage.h>
#include <Core/CCA/Component/Comm/DT/DataTransmitter.h>
#include <Core/CCA/Component/PIDL/PIDL.h>
#include <Core/CCA/Component/PIDL/URL.h>

using namespace std;
using namespace SCIRun;


SocketMessage::SocketMessage(SocketEpChannel *epchan, DTMessage *dtmsg)
{
  this->msg=dtmsg->buf;
  this->dtmsg=dtmsg;
  this->object=NULL;
  msg_size=sizeof(int); //skip handler_id
  this->epchan=epchan;
  spchan=NULL;
}

SocketMessage::SocketMessage(SocketSpChannel *spchan)
{
  this->dtmsg=NULL;
  this->msg=NULL;
  this->object=NULL;
  msg_size=sizeof(int); //skip handler_id
  this->spchan=spchan;
  epchan=NULL;
}

SocketMessage::~SocketMessage() {
  if(msg!=NULL) free(msg);
}

void SocketMessage::setLocalObject(void *obj){
  object=obj;
}

void 
SocketMessage::createMessage()  { 
  msg=realloc(msg, INIT_SIZE);
  capacity=INIT_SIZE;
  msg_size=sizeof(int); //reserve for handler_id
  object=NULL;
}

void 
SocketMessage::marshalInt(const int *buf, int size){
  marshalBuf(buf, size*sizeof(int));
}

void 
SocketMessage::marshalByte(const char *buf, int size){
  marshalBuf(buf, size*sizeof(char));
}

void 
SocketMessage::marshalChar(const char *buf, int size){
  marshalBuf(buf, size*sizeof(char));
}

void 
SocketMessage::marshalFloat(const float *buf, int size){
  marshalBuf(buf, size*sizeof(float));
}
void 
SocketMessage::marshalDouble(const double *buf, int size){
  marshalBuf(buf, size*sizeof(double));
}

void 
SocketMessage::marshalLong(const long *buf, int size){
  marshalBuf(buf, size*sizeof(long));
}

void 
SocketMessage::marshalSpChannel(SpChannel* channel){
  SocketSpChannel * chan = dynamic_cast<SocketSpChannel*>(channel);
  marshalBuf(&(chan->ep_addr), sizeof(DTAddress));
  marshalBuf(&(chan->ep), sizeof(void *));
  marshalBuf(&(chan->object), sizeof(void *));
}

void 
SocketMessage::sendMessage(int handler){
  //cerr<<"SocketMessage::sendMessage handler id="<<handler<<endl;
  memcpy(msg, &handler, sizeof(int));
  DTMessage *wmsg=new DTMessage;
  wmsg->buf=(char*)msg;
  wmsg->length=msg_size;
  wmsg->autofree=true;

  if(epchan!=NULL){
    wmsg->recver=dtmsg->sender;
    wmsg->to_addr=dtmsg->fr_addr;
    epchan->ep->putMessage(wmsg);
  }
  else{
    wmsg->recver=spchan->ep;
    wmsg->to_addr= spchan->ep_addr;
    spchan->sp->putMessage(wmsg);
  }
}

void 
SocketMessage::waitReply(){
  DTMessage *wmsg=spchan->sp->getMessage();
  wmsg->autofree=false;
  msg=wmsg->buf;
  delete wmsg;
  msg_size=sizeof(int);//skip handler_id
}

void 
SocketMessage::unmarshalReply(){
  //do nothing
}

void 
SocketMessage::unmarshalInt(int *buf, int size){
  unmarshalBuf(buf, size*sizeof(int));
}

void 
SocketMessage::unmarshalByte(char *buf, int size){
  unmarshalBuf(buf, size*sizeof(char));
}
void 
SocketMessage::unmarshalChar(char *buf, int size){
  unmarshalBuf(buf, size*sizeof(char));
}

void 
SocketMessage::unmarshalFloat(float *buf, int size){
  unmarshalBuf(buf, size*sizeof(float));
}

void 
SocketMessage::unmarshalDouble(double *buf, int size){
  unmarshalBuf(buf, size*sizeof(double));
}

void 
SocketMessage::unmarshalLong(long *buf, int size){
  unmarshalBuf(buf, size*sizeof(long));
}

void 
SocketMessage::unmarshalSpChannel(SpChannel* channel){
  SocketSpChannel * chan = dynamic_cast<SocketSpChannel*>(channel);

  marshalBuf(&(chan->ep_addr), sizeof(DTAddress));
  marshalBuf(&(chan->ep), sizeof(void *));
  marshalBuf(&(chan->object), sizeof(void *));

  if(!PIDL::getDT()->isLocal(chan->ep_addr)){
    chan->object=NULL;
  }
}

void* 
SocketMessage::getLocalObj(){
  return object;
}

void SocketMessage::destroyMessage() {
  if(msg!=NULL) free(msg);
  msg=NULL;
  delete this;
}


//private methods
void 
SocketMessage::marshalBuf(const void *buf, int fullsize){
  msg_size+=fullsize;
  if(msg_size>capacity){
    capacity=msg_size+INIT_SIZE;
    msg=realloc(msg, capacity);
  }
  memcpy((char*)msg+msg_size-fullsize, buf, fullsize); 
}

void 
SocketMessage::unmarshalBuf(void *buf, int fullsize){
  memcpy(buf, (char*)msg+msg_size, fullsize); 
  msg_size+=fullsize;
}

