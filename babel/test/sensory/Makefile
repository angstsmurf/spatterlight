# Test babel basic functionality with sensory.blb

include ../Makefile.boilerplate

.PHONY: $(CURDIR)
$(CURDIR):
	../../babel -ifid sensory.blb > output
	../../babel -format sensory.blb >> output
	../../babel -identify sensory.blb >> output
	../../babel -story sensory.blb >> output
	cmp GLULX-4-000329-5C2240F3.blorb sensory.ulx >> output
	$(VERIFY)

clean:
	-rm -f output GLULX-4-*
