# Test babel with Quest
# The fixtures are babel-tool's. Only the format and IFID are checked: this
# babel splits Quest into the quest4 and quest5 modules and does not build
# iFiction records for them.

include ../Makefile.boilerplate

.PHONY: $(CURDIR)
$(CURDIR):
	echo legacy: > output
	../../babel -format legacy.quest >> output
	../../babel -ifid legacy.quest >> output
	echo modern: >> output
	../../babel -format modern.quest >> output
	../../babel -ifid modern.quest >> output
	echo raw: >> output
	../../babel -format raw.aslx >> output
	../../babel -ifid raw.aslx >> output
	echo asl: >> output
	../../babel -format sample.asl >> output
	../../babel -ifid sample.asl >> output
	echo cas: >> output
	../../babel -format sample.cas >> output
	../../babel -ifid sample.cas >> output
	$(VERIFY)

clean:
	-rm -f output
