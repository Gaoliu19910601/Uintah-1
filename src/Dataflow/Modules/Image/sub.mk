#
# Makefile fragment for this subdirectory
# $Id$
#

# *** NOTE ***
# 
# Do not remove or modify the comment line:
#
# #[INSERT NEW ?????? HERE]
#
# It is required by the module maker to properly edit this file.
# if you want to edit this file by hand, see the "Create A New Module"
# documentation on how to do it correctly.

include $(SRCTOP)/scripts/smallso_prologue.mk

SRCDIR   := SCIRun/Modules/Image

SRCS     += \
	$(SRCDIR)/Binop.cc\
	$(SRCDIR)/Edge.cc\
	$(SRCDIR)/FFT.cc\
	$(SRCDIR)/FFTImage.cc\
	$(SRCDIR)/FilterImage.cc\
	$(SRCDIR)/Gauss.cc\
	$(SRCDIR)/Hist.cc\
	$(SRCDIR)/HistEq.cc\
	$(SRCDIR)/IFFT.cc\
	$(SRCDIR)/IFFTImage.cc\
	$(SRCDIR)/ImageConvolve.cc\
	$(SRCDIR)/ImageGen.cc\
	$(SRCDIR)/ImageSel.cc\
	$(SRCDIR)/ImageToGeom.cc\
	$(SRCDIR)/Noise.cc\
	$(SRCDIR)/PMFilterImage.cc\
	$(SRCDIR)/Radon.cc\
	$(SRCDIR)/Segment.cc\
	$(SRCDIR)/Sharpen.cc\
	$(SRCDIR)/Snakes.cc\
	$(SRCDIR)/Subsample.cc\
	$(SRCDIR)/Ted.cc\
	$(SRCDIR)/Threshold.cc\
	$(SRCDIR)/Transforms.cc\
	$(SRCDIR)/Turk.cc\
	$(SRCDIR)/Unop.cc\
	$(SRCDIR)/ViewHist.cc\
	$(SRCDIR)/WhiteNoiseImage.cc\
#[INSERT NEW MODULE HERE]

PSELIBS := SCIRun/Datatypes/Image PSECore/Dataflow PSECore/Datatypes \
	SCICore/Datatypes SCICore/Persistent SCICore/Exceptions \
	SCICore/TclInterface SCICore/Containers SCICore/Thread \
	SCICore/Math SCICore/TkExtensions
LIBS := $(TK_LIBRARY) $(GL_LIBS) -lm

include $(SRCTOP)/scripts/smallso_epilogue.mk

#
# $Log$
# Revision 1.2.2.1  2000/09/28 02:55:00  mcole
# merge trunk into FIELD_REDESIGN branch
#
# Revision 1.4  2000/06/08 22:46:35  moulding
# Added a comment note about not messing with the module maker comment lines,
# and how to edit this file by hand.
#
# Revision 1.3  2000/06/07 17:32:58  moulding
# made changes to allow the module maker to add to and edit this file
#
# Revision 1.2  2000/03/20 19:38:12  sparker
# Added VPATH support
#
# Revision 1.1  2000/03/17 09:29:08  sparker
# New makefile scheme: sub.mk instead of Makefile.in
# Use XML-based files for module repository
# Plus many other changes to make these two things work
#
#
