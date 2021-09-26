# Makefile (pass-through)
# Cross-platform (bash/sh + CMD/PowerShell)
# GNU make (gmake) compatible; ref: <https://www.gnu.org/software/make/manual>

# ref: https://stackoverflow.com/a/14061796/43774

OSID := $(or $(and $(filter .exe,$(patsubst %.exe,.exe,$(subst $() $(),_,${SHELL}))),$(filter win,${OS:Windows_NT=win})),nix)## OSID == [nix,win]

# gather all goals/targets as arguments; creating an empty rule for each word (which avoids multiple calls to sub-make)
ARGS := $(wordlist 1,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
$(eval $(ARGS):_undefined_;@:)

_default: .DEFAULT
.DEFAULT:
	$(if $(filter win,${OSID}),${MAKE} -f Makefile.win -- ${ARGS}, ./configure && ${MAKE} -- ${ARGS})
