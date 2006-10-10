#
#  For more information, please see: http://software.sci.utah.edu
# 
#  The MIT License
# 
#  Copyright (c) 2004 Scientific Computing and Imaging Institute,
#  University of Utah.
# 
#  License for the specific language governing rights and limitations under
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
# 
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
# 
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
#


# Makefile fragment for this subdirectory

include $(SCIRUN_SCRIPTS)/smallso_prologue.mk

SRCDIR   := Core/Algorithms/Visualization

SRCS     += \
	$(SRCDIR)/EdgeMC.cc             \
	$(SRCDIR)/FastLatMC.cc          \
        $(SRCDIR)/HexMC.cc		\
	$(SRCDIR)/Isosurface.cc		\
	$(SRCDIR)/MarchingCubes.cc	\
	$(SRCDIR)/mcube2.cc		\
	$(SRCDIR)/Noise.cc		\
	$(SRCDIR)/NrrdTextureBuilderAlgo.cc	\
	$(SRCDIR)/PrismMC.cc		\
	$(SRCDIR)/QuadMC.cc		\
	$(SRCDIR)/RenderField.cc	\
	$(SRCDIR)/Sage.cc		\
	$(SRCDIR)/TetMC.cc		\
	$(SRCDIR)/TextureBuilderAlgo.cc	\
	$(SRCDIR)/TriMC.cc		\
	$(SRCDIR)/UHexMC.cc		\

PSELIBS := \
	Core/Basis      \
	Core/Containers \
	Core/Datatypes  \
	Core/Exceptions \
	Core/Geom       \
	Core/Geometry   \
	Core/Persistent \
	Core/Thread     \
	Core/Util       \
	Core/Volume

LIBS := $(TEEM_LIBRARY) $(PNG_LIBRARY) $(Z_LIBRARY) $(GL_LIBRARY) $(M_LIBRARY)

include $(SCIRUN_SCRIPTS)/smallso_epilogue.mk
