#
# Makefile fragment for this subdirectory
# $Id$
#

include $(SRCTOP)/scripts/smallso_prologue.mk

SRCDIR   := Uintah/Components/Schedulers

SRCS += $(SRCDIR)/templates.cc \
	$(SRCDIR)/MixedScheduler.cc \
	$(SRCDIR)/MPIScheduler.cc $(SRCDIR)/MessageLog.cc \
	$(SRCDIR)/OnDemandDataWarehouse.cc \
	$(SRCDIR)/RoundRobinLoadBalancer.cc \
	$(SRCDIR)/SendState.cc \
	$(SRCDIR)/SimpleLoadBalancer.cc \
	$(SRCDIR)/SingleProcessorScheduler.cc \
	$(SRCDIR)/SingleProcessorLoadBalancer.cc \
	$(SRCDIR)/TaskGraph.cc \
	$(SRCDIR)/ThreadPool.cc

PSELIBS := Uintah/Grid Uintah/Interface SCICore/Thread Uintah/Parallel \
	Uintah/Exceptions SCICore/Exceptions SCICore/Util PSECore/XMLUtil
LIBS := $(XML_LIBRARY) $(MPI_LIBRARY) $(VAMPIR_LIBRARY)

include $(SRCTOP)/scripts/smallso_epilogue.mk

#
# $Log$
# Revision 1.12.4.2  2000/10/06 23:57:54  witzel
# Added vampir support.
#
# Revision 1.12.4.1  2000/10/02 15:02:45  sparker
# Send only boundary particles
#
# Revision 1.12  2000/09/26 18:50:54  dav
# added MixedScheduler.cc
#
# Revision 1.11  2000/09/26 18:27:33  dav
# added templates.cc
#
# Revision 1.10  2000/09/20 16:00:29  sparker
# Added external interface to LoadBalancer (for per-processor tasks)
# Added message logging functionality. Put the tag <MessageLog/> in
#    the ups file to enable
#
# Revision 1.9  2000/07/27 22:39:47  sparker
# Implemented MPIScheduler
# Added associated support
#
# Revision 1.8  2000/06/17 07:04:56  sparker
# Implemented initial load balancer modules
# Use ProcessorGroup
# Implemented TaskGraph - to contain the common scheduling stuff
#
# Revision 1.7  2000/06/15 23:14:08  sparker
# Cleaned up scheduler code
# Renamed BrainDamagedScheduler to SingleProcessorScheduler
# Created MPIScheduler to (eventually) do the MPI work
#
# Revision 1.6  2000/06/15 21:57:12  sparker
# Added multi-patch support (bugzilla #107)
# Changed interface to datawarehouse for particle data
# Particles now move from patch to patch
#
# Revision 1.5  2000/05/21 08:19:07  sparker
# Implement NCVariable read
# Do not fail if variable type is not known
# Added misc stuff to makefiles to remove warnings
#
# Revision 1.4  2000/05/05 06:42:43  dav
# Added some _hopefully_ good code mods as I work to get the MPI stuff to work.
#
# Revision 1.3  2000/04/12 22:59:56  sparker
# Link with xerces
#
# Revision 1.2  2000/03/20 19:38:26  sparker
# Added VPATH support
#
# Revision 1.1  2000/03/17 09:29:45  sparker
# New makefile scheme: sub.mk instead of Makefile.in
# Use XML-based files for module repository
# Plus many other changes to make these two things work
#
