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

# *** NOTE ***
# Do not remove or modify the comment line:
# #[INSERT NEW ?????? HERE]
# It is required by the Core/CCA/Component Wizard to properly edit this file.
# if you want to edit this file by hand, see the "Create A New Core/CCA/Component"
# documentation on how to do it correctly.

include $(SRCTOP_ABS)/scripts/smallso_prologue.mk

SRCDIR   := Packages/Teem/Dataflow/Modules/Filters


SRCS     += \
	$(SRCDIR)/NrrdConvert.cc\
	$(SRCDIR)/NrrdPermute.cc\
	$(SRCDIR)/NrrdQuantize.cc\
	$(SRCDIR)/NrrdResample.cc\
	$(SRCDIR)/NrrdSubvolume.cc\
#[INSERT NEW CODE FILE HERE]

PSELIBS := Packages/Teem/Core/Datatypes Core/Datatypes \
	Dataflow/Network Dataflow/Ports \
        Core/Persistent Core/Containers Core/Util \
        Core/Exceptions Core/Thread Core/GuiInterface \
        Core/Geom Core/Datatypes Core/Geometry \
        Core/TkExtensions Packages/Teem/Core/Datatypes \
	Packages/Teem/Dataflow/Ports

LIBS := $(TEEM_LIBRARY) -lnrrd -lbiff -lair

include $(SRCTOP_ABS)/scripts/smallso_epilogue.mk
