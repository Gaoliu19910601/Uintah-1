#
#  The contents of this file are subject to the University of Utah Public
#  License (the "License"); you may not use this file except in compliance
#  with the License.
#  
#  Software distributed under the License is distributed on an "AS IS"
#  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
#  License for the specific language governing rights and limitations under
#  the License.
#  
#  The Original Source Code is SCIRun, released March 12, 2001.
#  
#  The Original Source Code was developed by the University of Utah.
#  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
#  University of Utah. All Rights Reserved.
#


# Makefile fragment for this subdirectory

include $(SCIRUN_SCRIPTS)/smallso_prologue.mk

SRCDIR   := Core/CCA/Component/Comm

SRCS     += \
	$(SRCDIR)/connector.cc \
	$(SRCDIR)/listener.cc \
	$(SRCDIR)/SocketSpChannel.cc \
	$(SRCDIR)/SocketEpChannel.cc \
	$(SRCDIR)/NexusEpChannel.cc \
	$(SRCDIR)/NexusSpChannel.cc \
	$(SRCDIR)/CommError.cc \
	$(SRCDIR)/NexusSpMessage.cc \
	$(SRCDIR)/NexusEpMessage.cc \
	$(SRCDIR)/SocketMessage.cc \
	$(SRCDIR)/Communication.cc \
	$(SRCDIR)/sock.c \
	$(SRCDIR)/buf.c \
	$(SRCDIR)/ReplyEP.cc \
	$(SRCDIR)/NexusHandlerThread.cc \

PSELIBS := Core/Exceptions Core/Thread 
LIBS := $(GLOBUS_LIBS) -lglobus_nexus -lglobus_dc -lglobus_common

include $(SCIRUN_SCRIPTS)/smallso_epilogue.mk

