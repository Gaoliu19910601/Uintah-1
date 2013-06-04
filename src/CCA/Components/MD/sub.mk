#
#  The MIT License
#
#  Copyright (c) 1997-2013 The University of Utah
# 
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to
#  deal in the Software without restriction, including without limitation the
#  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
#  sell copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
# 
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
# 
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
#  IN THE SOFTWARE.
# 
# 
# 
# 
# 
# Makefile fragment for this subdirectory 


include $(SCIRUN_SCRIPTS)/smallso_prologue.mk

SRCDIR	:= CCA/Components/MD

SRCS += $(SRCDIR)/MD.cc                         \
        $(SRCDIR)/MDLabel.cc                    \
        $(SRCDIR)/MDSystem.cc                   \
        $(SRCDIR)/ElectrostaticsFactory.cc      \
        $(SRCDIR)/Electrostatics.cc             \
        $(SRCDIR)/SPME.cc                       \
        $(SRCDIR)/NonBondedFactory.cc           \
        $(SRCDIR)/NonBonded.cc                  \
        $(SRCDIR)/AnalyticNonBonded.cc          \
        $(SRCDIR)/SPMEMapPoint.cc               \
        $(SRCDIR)/SPMEPatch.cc                  \
        $(SRCDIR)/SimpleGrid.cc                 \
        $(SRCDIR)/CenteredCardinalBSpline.cc

SUBDIRS := \

include $(SCIRUN_SCRIPTS)/recurse.mk

PSELIBS := \
	CCA/Components/OnTheFlyAnalysis \
	CCA/Ports           \
	Core/Disclosure     \
	Core/Exceptions     \
	Core/Containers     \
	Core/Geometry       \
	Core/GeometryPiece  \
	Core/Grid           \
	Core/Labels         \
	Core/Math           \
	Core/Parallel       \
	Core/ProblemSpec    \
	Core/Thread         \
	Core/Util           

LIBS := $(XML2_LIBRARY) $(VT_LIBRARY) $(MPI_LIBRARY) $(M_LIBRARY) \
	      $(LAPACK_LIBRARY) $(BLAS_LIBRARY) $(THREAD_LIBRARY) $(FFTW_LIBRARY)


include $(SCIRUN_SCRIPTS)/smallso_epilogue.mk
