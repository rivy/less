# Makefile (pass-through)
# Cross-platform (bash/sh + CMD/PowerShell)
# GNU make (gmake) compatible; ref: <https://www.gnu.org/software/make/manual>

# ref: https://stackoverflow.com/a/14061796/43774

# spell-checker:ignore (jargon) autoset deps depfile depfiles delims executables maint multilib
# spell-checker:ignore (make) BASEPATH CURDIR MAKECMDGOALS MAKEFLAGS SHELLSTATUS TERMERR TERMOUT abspath addprefix addsuffix endef eval findstring firstword gmake ifeq ifneq lastword notdir patsubst prepend undefine wordlist
# spell-checker:ignore (vars) CFLAGS CPPFLAGS CXXFLAGS DEFINETYPE EXEEXT LDFLAGS LIBPATH LIBs MAKEDIR OBJ_deps OBJs OSID PAREN devnull falsey fileset filesets globset globsets punct truthy

OSID := $(or $(and $(filter .exe,$(patsubst %.exe,.exe,$(subst $() $(),_,${SHELL}))),$(filter win,${OS:Windows_NT=win})),nix)## OSID == [nix,win]

# gather all goals/targets as arguments; creating an empty rule for each word (which avoids multiple calls to sub-make)
ARGS := $(wordlist 1,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
$(eval $(ARGS):_undefined_;@:)

_default: .DEFAULT
.DEFAULT:
	$(if $(filter win,${OSID}),${MAKE} -f Makefile.win -- ${ARGS}, ./configure && ${MAKE} -- ${ARGS})
