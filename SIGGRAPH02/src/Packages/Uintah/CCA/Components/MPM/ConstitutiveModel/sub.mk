# Makefile fragment for this subdirectory

SRCDIR   := Packages/Uintah/CCA/Components/MPM/ConstitutiveModel

SRCS     += \
	$(SRCDIR)/CompMooneyRivlin.cc         \
	$(SRCDIR)/ConstitutiveModelFactory.cc \
	$(SRCDIR)/ConstitutiveModel.cc        \
	$(SRCDIR)/MPMMaterial.cc              \
	$(SRCDIR)/CompNeoHook.cc              \
	$(SRCDIR)/CompNeoHookPlas.cc          \
	$(SRCDIR)/ViscoScram.cc               \
	$(SRCDIR)/HypoElastic.cc              \
	$(SRCDIR)/MWViscoElastic.cc           \
	$(SRCDIR)/IdealGasMP.cc               \
	$(SRCDIR)/Membrane.cc 

PSELIBS := Packages/Uintah/Core/Grid \
	Packages/Uintah/CCA/Components/ICE \
	Packages/Uintah/CCA/Components/HETransformation \
	Core/Datatypes \
	Core/Util

