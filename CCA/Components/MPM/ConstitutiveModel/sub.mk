# Makefile fragment for this subdirectory

SRCDIR   := Packages/Uintah/CCA/Components/MPM/ConstitutiveModel

SRCS     += \
	$(SRCDIR)/CompMooneyRivlin.cc        	\
	$(SRCDIR)/ConstitutiveModelFactory.cc 	\
	$(SRCDIR)/ConstitutiveModel.cc        	\
	$(SRCDIR)/MPMMaterial.cc              	\
	$(SRCDIR)/CompNeoHook.cc              	\
	$(SRCDIR)/CompNeoHookImplicit.cc 	\
	$(SRCDIR)/CompNeoHookPlas.cc          	\
	$(SRCDIR)/ViscoScram.cc               	\
	$(SRCDIR)/HypoElastic.cc              	\
	$(SRCDIR)/MWViscoElastic.cc           	\
	$(SRCDIR)/IdealGasMP.cc               	\
	$(SRCDIR)/Membrane.cc 			\
	$(SRCDIR)/JohnsonCook.cc 		\
	$(SRCDIR)/DamageModel.cc \
	$(SRCDIR)/DamageModelFactory.cc \
	$(SRCDIR)/HypoElasticPlastic.cc \
	$(SRCDIR)/JohnsonCookDamage.cc \
	$(SRCDIR)/JohnsonCookPlastic.cc \
	$(SRCDIR)/PlasticityModel.cc \
	$(SRCDIR)/PlasticityModelFactory.cc 

PSELIBS := Packages/Uintah/Core/Grid \
	Packages/Uintah/CCA/Components/ICE \
	Packages/Uintah/CCA/Components/HETransformation \
	Core/Datatypes \
	Core/Util

