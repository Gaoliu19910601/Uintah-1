# Makefile fragment for this subdirectory
include $(SCIRUN_SCRIPTS)/smallso_prologue.mk

SRCDIR   := Packages/Uintah/CCA/Components/ICE

SRCS	+= \
	$(SRCDIR)/ICE.cc         \
	$(SRCDIR)/ICEDebug.cc \
	$(SRCDIR)/ICELabel.cc    \
	$(SRCDIR)/ICEMaterial.cc \
	$(SRCDIR)/GeometryObject2.cc

SUBDIRS := $(SRCDIR)/EOS 
 
include $(SCIRUN_SCRIPTS)/recurse.mk          

PSELIBS := \
	Packages/Uintah/CCA/Components/HETransformation \
	Packages/Uintah/CCA/Components/MPM \
	Packages/Uintah/CCA/Ports \
	Packages/Uintah/Core/Grid \
	Packages/Uintah/Core/ProblemSpec \
	Packages/Uintah/Core/Parallel \
	Packages/Uintah/Core/Exceptions \
	Packages/Uintah/Core/Math \
	Core/Exceptions Core/Thread \
	Core/Geometry Dataflow/XMLUtil \
	Core/Datatypes

LIBS	:= $(XML_LIBRARY) $(MPI_LIBRARY)

include $(SCIRUN_SCRIPTS)/smallso_epilogue.mk



