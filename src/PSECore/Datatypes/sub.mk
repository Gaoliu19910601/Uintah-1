#
# Makefile fragment for this subdirectory
# $Id$
#

include $(SRCTOP)/scripts/smallso_prologue.mk

SRCDIR   := PSECore/Datatypes

SRCS     += $(SRCDIR)/BooleanPort.cc $(SRCDIR)/ColorMapPort.cc \
	$(SRCDIR)/ColumnMatrixPort.cc $(SRCDIR)/GeometryPort.cc \
	$(SRCDIR)/HexMeshPort.cc $(SRCDIR)/IntervalPort.cc \
	$(SRCDIR)/MatrixPort.cc $(SRCDIR)/MeshPort.cc \
	$(SRCDIR)/ScalarFieldPort.cc $(SRCDIR)/ScaledBoxWidgetData.cc \
	$(SRCDIR)/ScaledBoxWidgetDataPort.cc $(SRCDIR)/SoundPort.cc \
	$(SRCDIR)/SurfacePort.cc $(SRCDIR)/VectorFieldPort.cc \
	$(SRCDIR)/VoidStarPort.cc $(SRCDIR)/cMatrixPort.cc \
	$(SRCDIR)/cVectorPort.cc \
	$(SRCDIR)/SpanSpace.cc $(SRCDIR)/SpanPort.cc \
	$(SRCDIR)/templates.cc \
	$(SRCDIR)/PathPort.cc $(SRCDIR)/CameraViewPort.cc 

PSELIBS := PSECore/Dataflow PSECore/Comm SCICore/Containers \
	SCICore/Thread SCICore/Geom SCICore/Geometry SCICore/Exceptions \
	SCICore/Persistent SCICore/Datatypes
LIBS := 

include $(SRCTOP)/scripts/smallso_epilogue.mk

#
# $Log$
# Revision 1.3.2.5  2000/10/26 10:04:01  moulding
# merge HEAD into FIELD_REDESIGN
#
# Revision 1.3.2.4  2000/10/10 23:02:15  michaelc
# move from {SVT}Field ports to Field ports
#
# Revision 1.3.2.3  2000/10/02 23:12:47  michaelc
# add SpanSpace.cc, unresolved symbols
#
# Revision 1.3.2.2  2000/09/28 03:14:20  mcole
# merge trunk into FIELD_REDESIGN branch
#
# Revision 1.3.2.1  2000/06/07 17:34:14  kuehne
# added variuos ports used by the new fields
#
# Revision 1.7  2000/08/20 04:23:22  samsonov
# *** empty log message ***
#
# Revision 1.6  2000/07/27 17:13:22  yarden
# add SpanPort
#
# Revision 1.5  2000/07/24 20:58:47  yarden
# New datastructure to hold a SpanSpace (part of Noise)
#
# Revision 1.4  2000/07/19 06:35:00  samsonov
# PathPort datatype moved from DaveW
#
# Revision 1.3  2000/03/21 03:01:24  sparker
# Partially fixed special_get method in SimplePort
# Pre-instantiated a few key template types, in an attempt to reduce
#   initial compile time and reduce code bloat.
# Manually instantiated templates are in */*/templates.cc
#
# Revision 1.2  2000/03/20 19:37:20  sparker
# Added VPATH support
#
# Revision 1.1  2000/03/17 09:27:58  sparker
# New makefile scheme: sub.mk instead of Makefile.in
# Use XML-based files for module repository
# Plus many other changes to make these two things work
#
#
