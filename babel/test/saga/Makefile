# Test the ScottFree and SAGA Plus modules on the game files the scott and
# plus test suites keep in the repository. SAGA Plus text databases are known
# by their title: a database we have no checksum for still gets the IFID of
# the game its title names (the plus synthetic games are titled X-MEN).

include ../Makefile.boilerplate

SCOTT = ../../../terps/scott/test
PLUS = ../../../terps/plus/test/synthetic/games

.PHONY: $(CURDIR)
$(CURDIR):
	echo ScottFree: > output
	../../babel -format $(SCOTT)/regression.dat >> output
	../../babel -ifid $(SCOTT)/regression.dat >> output
	../../babel -ifid $(SCOTT)/parser/parser.dat >> output
	echo SAGA Plus: >> output
	../../babel -format $(PLUS)/rare-opcodes/SYNTH.DAT >> output
	../../babel -ifid $(PLUS)/rare-opcodes/SYNTH.DAT >> output
	../../babel -ifid $(PLUS)/loader-edges/EDGES.DAT >> output
	echo Standard input: >> output
	../../babel -ifid - < $(SCOTT)/regression.dat >> output
	$(VERIFY)

clean:
	-rm -f output
