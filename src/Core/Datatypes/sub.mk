#
# Makefile fragment for this subdirectory
# $Id$
#

include $(SRCTOP)/scripts/smallso_prologue.mk

SRCDIR   := SCICore/Datatypes

GENSRCS := $(SRCDIR)/ScalarFieldRG.cc $(SRCDIR)/ScalarFieldRGchar.cc \
	$(SRCDIR)/ScalarFieldRGuchar.cc $(SRCDIR)/ScalarFieldRGshort.cc \
	$(SRCDIR)/ScalarFieldRGint.cc $(SRCDIR)/ScalarFieldRGfloat.cc \
	$(SRCDIR)/ScalarFieldRGdouble.cc

GENHDRS := $(patsubst %.cc,%.h,$(GENSRCS))

SRCS += $(GENSRCS) $(SRCDIR)/TriSurface.cc $(SRCDIR)/BasicSurfaces.cc \
	$(SRCDIR)/Boolean.cc $(SRCDIR)/ColorMap.cc \
	$(SRCDIR)/ColumnMatrix.cc $(SRCDIR)/Datatype.cc \
	$(SRCDIR)/DenseMatrix.cc $(SRCDIR)/HexMesh.cc \
	$(SRCDIR)/Image.cc $(SRCDIR)/Interval.cc $(SRCDIR)/Matrix.cc \
	$(SRCDIR)/Mesh.cc $(SRCDIR)/ScalarField.cc \
	$(SRCDIR)/ScalarFieldHUG.cc $(SRCDIR)/ScalarFieldRGBase.cc \
	$(SRCDIR)/ScalarFieldUG.cc $(SRCDIR)/ScalarFieldZone.cc \
	$(SRCDIR)/SparseRowMatrix.cc $(SRCDIR)/Surface.cc \
	$(SRCDIR)/SymSparseRowMatrix.cc $(SRCDIR)/TriDiagonalMatrix.cc \
	$(SRCDIR)/VectorField.cc $(SRCDIR)/VectorFieldHUG.cc \
	$(SRCDIR)/VectorFieldOcean.cc $(SRCDIR)/VectorFieldRG.cc \
	$(SRCDIR)/VectorFieldUG.cc $(SRCDIR)/VectorFieldZone.cc \
	$(SRCDIR)/VoidStar.cc $(SRCDIR)/cDMatrix.cc \
	$(SRCDIR)/cMatrix.cc $(SRCDIR)/cSMatrix.cc $(SRCDIR)/cVector.cc \
	$(SRCDIR)/SurfTree.cc $(SRCDIR)/ScalarFieldRGCC.cc \
	$(SRCDIR)/VectorFieldRGCC.cc $(SRCDIR)/templates.cc \
	$(SRCDIR)/Geom.cc $(SRCDIR)/Attrib.cc \
	$(SRCDIR)/Field.cc $(SRCDIR)/SField.cc \
	$(SRCDIR)/GenSField.cc \
	$(SRCDIR)/FieldWrapper.cc $(SRCDIR)/Domain.cc \
	$(SRCDIR)/SField.cc $(SRCDIR)/VField.cc \
	$(SRCDIR)/TField.cc $(SRCDIR)/LatticeGeom.cc \
	$(SRCDIR)/StructuredGeom.cc $(SRCDIR)/UnstructuredGeom.cc \
	$(SRCDIR)/MeshGeom.cc \

$(SRCDIR)/ScalarFieldRG.h: $(SRCDIR)/ScalarFieldRGTYPE.h
	sed -e 's/RGTYPE/RG/g' -e 's/TYPE/double/g' < $< > $@

$(SRCDIR)/ScalarFieldRGchar.h: $(SRCDIR)/ScalarFieldRGTYPE.h
	sed 's/TYPE/char/g' < $< > $@

$(SRCDIR)/ScalarFieldRGuchar.h: $(SRCDIR)/ScalarFieldRGTYPE.h
	sed 's/TYPE/uchar/g' < $< > $@

$(SRCDIR)/ScalarFieldRGshort.h: $(SRCDIR)/ScalarFieldRGTYPE.h
	sed 's/TYPE/short/g' < $< > $@

$(SRCDIR)/ScalarFieldRGint.h: $(SRCDIR)/ScalarFieldRGTYPE.h
	sed 's/TYPE/int/g' < $< > $@

$(SRCDIR)/ScalarFieldRGfloat.h: $(SRCDIR)/ScalarFieldRGTYPE.h
	sed 's/TYPE/float/g' < $< > $@

$(SRCDIR)/ScalarFieldRGdouble.h: $(SRCDIR)/ScalarFieldRGTYPE.h
	sed 's/TYPE/double/g' < $< > $@


# .CC

$(SRCDIR)/ScalarFieldRG.cc: $(SRCDIR)/ScalarFieldRGdouble.cc $(SRCDIR)/ScalarFieldRG.h
	sed 's/RGdouble/RG/g' < $< > $@

$(SRCDIR)/ScalarFieldRGchar.cc: $(SRCDIR)/ScalarFieldRGTYPE.cc $(SRCDIR)/ScalarFieldRGchar.h
	sed 's/TYPE/char/g' < $< > $@

$(SRCDIR)/ScalarFieldRGuchar.cc: $(SRCDIR)/ScalarFieldRGTYPE.cc $(SRCDIR)/ScalarFieldRGuchar.h
	sed 's/TYPE/uchar/g' < $< > $@

$(SRCDIR)/ScalarFieldRGshort.cc: $(SRCDIR)/ScalarFieldRGTYPE.cc $(SRCDIR)/ScalarFieldRGshort.h
	sed 's/TYPE/short/g' < $< >$@

$(SRCDIR)/ScalarFieldRGint.cc: $(SRCDIR)/ScalarFieldRGTYPE.cc $(SRCDIR)/ScalarFieldRGint.h
	sed 's/TYPE/int/g' < $< > $@

$(SRCDIR)/ScalarFieldRGfloat.cc: $(SRCDIR)/ScalarFieldRGTYPE.cc $(SRCDIR)/ScalarFieldRGfloat.h
	sed 's/TYPE/float/g' < $< > $@

$(SRCDIR)/ScalarFieldRGdouble.cc: $(SRCDIR)/ScalarFieldRGTYPE.cc $(SRCDIR)/ScalarFieldRGdouble.h
	sed 's/TYPE/double/g' < $< > $@

PSELIBS := SCICore/Persistent SCICore/Exceptions SCICore/Containers \
	SCICore/Thread SCICore/Geometry SCICore/Geom SCICore/TclInterface \
	SCICore/Math
LIBS := -lm

include $(SRCTOP)/scripts/smallso_epilogue.mk

clean::
	rm -f $(GENSRCS)
	rm -f $(patsubst %.cc,%.h,$(GENSRCS))

#
# $Log$
# Revision 1.3.2.3  2000/08/14 21:29:45  michaelc
# Attributes as general acceleration class
#
# Revision 1.3.2.2  2000/08/03 16:52:52  kuehne
# Initial commit
#
# Revision 1.3.2.1  2000/06/07 17:42:25  kuehne
# Added datastructures used by the new fields
#
# Revision 1.3  2000/03/21 03:01:28  sparker
# Partially fixed special_get method in SimplePort
# Pre-instantiated a few key template types, in an attempt to reduce
#   initial compile time and reduce code bloat.
# Manually instantiated templates are in */*/templates.cc
#
# Revision 1.2  2000/03/20 19:37:35  sparker
# Added VPATH support
#
# Revision 1.1  2000/03/17 09:28:19  sparker
# New makefile scheme: sub.mk instead of Makefile.in
# Use XML-based files for module repository
# Plus many other changes to make these two things work
#
#
