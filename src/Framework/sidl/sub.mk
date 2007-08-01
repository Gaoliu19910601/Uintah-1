#
#  For more information, please see: http://software.sci.utah.edu
#
#  The MIT License
#
#  Copyright (c) 2007 Scientific Computing and Imaging Institute,
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

SRCDIR := Framework/sidl

SRCDIR_ABS :=  $(SRCTOP_ABS)/$(SRCDIR)
IMPLDIR_ABS := $(SRCDIR_ABS)/Impl
IMPLDIR := $(SRCDIR)/Impl
GLUEDIR_ABS := $(IMPLDIR_ABS)/glue
GLUEDIR := $(IMPLDIR)/glue

SIDL := \
        $(SRCDIR_ABS)/sci-cca.sidl \
        $(SRCDIR_ABS)/scijump.sidl


OUTPUTDIR_ABS := $(OBJTOP_ABS)/$(SRCDIR)
OUTIMPLDIR_ABS := $(OUTPUTDIR_ABS)/Impl
OUTGLUEDIR_ABS := $(OUTIMPLDIR_ABS)/glue

$(IMPLDIR_ABS)/serverbabel.make: $(GLUEDIR_ABS)/serverbabel.make

#--generate-subdirs, target dep: $(FWK_IMPLSRCS)
$(GLUEDIR_ABS)/serverbabel.make: $(SIDL) Core/Babel/timestamp
	if ! test -d $(OUTGLUEDIR_ABS); then mkdir -p $(OUTGLUEDIR_ABS); fi
	$(BABEL) --server=C++ \
           --output-directory=$(IMPLDIR_ABS) \
           --make-prefix=server \
           --hide-glue \
           --repository-path=$(BABEL_REPOSITORY) \
           --vpath=$(IMPLDIR_ABS) $(filter %.sidl, $+)

$(GLUEDIR_ABS)/clientbabel.make: $(CCASIDL)
	$(BABEL) --client=C++ --hide-glue --make-prefix=client --output-directory=$(IMPLDIR_ABS) $<

include $(SCIRUN_SCRIPTS)/smallso_prologue.mk

serverIORSRCS :=
serverSTUBSRCS :=
serverSKELSRCS :=
include $(GLUEDIR_ABS)/serverbabel.make
SRCS += $(patsubst %,$(GLUEDIR)/%,$(serverIORSRCS) $(serverSTUBSRCS) $(serverSKELSRCS))

clientSTUBSRCS :=
include $(GLUEDIR_ABS)/clientbabel.make
SRCS += $(patsubst %,$(GLUEDIR)/%,$(clientSTUBSRCS))

serverIMPLSRCS :=
include $(IMPLDIR_ABS)/serverbabel.make
SRCS += $(patsubst %,$(IMPLDIR)/%,$(serverIMPLSRCS))

PSELIBS := Core/Thread Framework/Core
INCLUDES += -I$(IMPLDIR_ABS) -I$(GLUEDIR_ABS) $(BABEL_INCLUDE)
LIBS := $(BABEL_LIBRARY)

include $(SCIRUN_SCRIPTS)/smallso_epilogue.mk
