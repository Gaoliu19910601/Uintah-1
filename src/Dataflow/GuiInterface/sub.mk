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

SRCDIR   := Dataflow/GuiInterface

SRCS     += $(SRCDIR)/GuiCallback.cc \
	$(SRCDIR)/GuiContext.cc \
	$(SRCDIR)/GuiGeom.cc \
	$(SRCDIR)/GuiInterface.cc \
	$(SRCDIR)/GuiVar.cc \
	$(SRCDIR)/GuiView.cc \
	$(SRCDIR)/MemStats.cc \
	$(SRCDIR)/TCLInterface.cc \
	$(SRCDIR)/TclObj.cc \
	$(SRCDIR)/TCLTask.cc \
	$(SRCDIR)/UIvar.cc \
	$(SRCDIR)/TkOpenGLContext.cc	    		\
	$(SRCDIR)/TkOpenGLEventSpawner.cc    		\


PSELIBS := Core/Exceptions Core/Util Core/Thread \
	   Core/Containers Dataflow/TkExtensions \
	   Core/Math Core/Geom Core/Datatypes
LIBS := $(ITK_LIBRARY) $(ITCL_LIBRARY) $(TCL_LIBRARY) $(X_LIBRARY) $(TEEM_LIBRARY) $(GL_LIBRARY)

include $(SCIRUN_SCRIPTS)/smallso_epilogue.mk

