# Test babel with Twine

include ../Makefile.boilerplate

.PHONY: $(CURDIR)
$(CURDIR):
	../../babel -format Test-Game.html > output
	../../babel -ifid Test-Game.html >> output
	$(VERIFY)

clean:
	-rm -f output
