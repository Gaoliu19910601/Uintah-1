#
# Makefile fragment for this subdirectory
# $Id$
#

include $(OBJTOP_ABS)/scripts/smallso_prologue.mk

SRCDIR   := DaveW/Modules/CS684

SRCS     += $(SRCDIR)/BldBRDF.cc $(SRCDIR)/BldScene.cc \
	$(SRCDIR)/RTrace.cc $(SRCDIR)/Radiosity.cc $(SRCDIR)/RayMatrix.cc \
	$(SRCDIR)/RayTest.cc $(SRCDIR)/XYZtoRGB.cc

PSELIBS := DaveW/Datatypes/CS684 PSECore/Widgets PSECore/Dataflow \
	PSECore/Datatypes SCICore/Containers SCICore/Exceptions \
	SCICore/TclInterface SCICore/Thread SCICore/Persistent \
	SCICore/Geom SCICore/Geometry SCICore/Datatypes SCICore/Util \
	SCICore/TkExtensions
LIBS := $(TK_LIBRARY) $(GL_LIBS)

include $(OBJTOP_ABS)/scripts/smallso_epilogue.mk

#
# $Log$
# Revision 1.1  2000/03/17 09:25:31  sparker
# New makefile scheme: sub.mk instead of Makefile.in
# Use XML-based files for module repository
# Plus many other changes to make these two things work
#
#
