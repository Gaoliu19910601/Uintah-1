# Makefile fragment for this subdirectory

include $(SCIRUN_SCRIPTS)/smallso_prologue.mk

SRCDIR := Packages/Uintah/Core/Exceptions

SRCS += \
	$(SRCDIR)/ConvergenceFailure.cc     \
	$(SRCDIR)/InvalidCompressionMode.cc \
	$(SRCDIR)/InvalidGrid.cc            \
	$(SRCDIR)/InvalidValue.cc           \
	$(SRCDIR)/ParameterNotFound.cc      \
	$(SRCDIR)/ProblemSetupException.cc  \
	$(SRCDIR)/TypeMismatchException.cc  \
	$(SRCDIR)/UintahPetscError.cc       \
	$(SRCDIR)/VariableNotFoundInGrid.cc 

PSELIBS := \
	Core/Exceptions \
	Core/Geometry

LIBS := 

include $(SCIRUN_SCRIPTS)/smallso_epilogue.mk

