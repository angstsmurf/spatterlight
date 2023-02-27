# Test babel with Hugo

include ../Makefile.boilerplate

.PHONY: $(CURDIR)
$(CURDIR):
	echo Valid: > output
	../../babel -ifid minimal.hex >> output
	$(VERIFY)

clean:
	-rm -f output
