####################################################################
##                                                                ##
##            SWI-Prolog Virtual Machine Generator                ##
##                                                                ##
####################################################################

#
#  To run this you will need SWI-Prolog.
#  http://www.swi-prolog.org/
#

VMWRITER = vm.pl vmwriter.pl utils.pl ccode.pl detcheck.pl mode_combine.pl
GENERATED = insns-switch.i insns-threaded.i insns-igen.i insns-igen-h.i \
            insns-table.py
#           insns-threaded-rel.i 


all: $(GENERATED)

clean:
	-rm $(GENERATED)

default: $(addsuffix .default,$(GENERATED))


%-switch.i: %.pl $(VMWRITER)
	pl -f $< -g "main_emit(modes, switch)" -t halt > $@

%-threaded-rel.i: %.pl $(VMWRITER)
	pl -f $< -g "main_emit(modes, threaded(relative))" -t halt > $@

%-threaded.i: %.pl $(VMWRITER)
	pl -f $< -g "main_emit(modes, threaded(absolute))" -t halt > $@

%-immed.i: %.pl $(VMWRITER)
	pl -f $< -g "main_emit(modes, single([immed(int)]))" -t halt > $@

%-igen.i: %.pl $(VMWRITER)
	pl -f $< -g "main_emit(insns, functions('DEFINEFN '))" -t halt > $@

%-igen-h.i: %.pl $(VMWRITER)
	pl -f $< -g "main_emit(insns, headers('EXTERNFN '))" -t halt > $@

%-table.py: %.pl $(VMWRITER)
	pl -f $< -g "main_emit(pytable)" -t halt > $@

%.default: %
	cp $< $@
