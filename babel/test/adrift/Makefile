# Test babel with Adrift
# The fixtures are babel-tool's. Only the format and IFID are checked: this
# babel does not build iFiction records for Adrift files.

include ../Makefile.boilerplate

.PHONY: $(CURDIR)
$(CURDIR):
	echo Adrift-5: > output
	../../babel -format test5.taf >> output
	../../babel -ifid test5.taf >> output
	echo Adrift-4: >> output
	../../babel -format test4.taf >> output
	../../babel -ifid test4.taf >> output
	$(VERIFY)

clean:
	-rm -f output
