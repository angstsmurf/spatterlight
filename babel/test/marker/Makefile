# Test the UUID://...// marker scan in Z-code and Glulx files
# An IFID in a marker is digits, letters and hyphens, closed by "//", and
# small letters are capitalised. Anything else falls back to the IFID built
# from the header.

include ../Makefile.boilerplate

.PHONY: $(CURDIR)
$(CURDIR):
	echo Valid: > output
	../../babel -ifid valid.z5 >> output
	../../babel -ifid valid.ulx >> output
	echo At end of file: >> output
	../../babel -ifid at-end.z5 >> output
	echo After a malformed marker: >> output
	../../babel -ifid second.z5 >> output
	echo Lowercase: >> output
	../../babel -ifid lowercase.z5 >> output
	../../babel -ifid lowercase.ulx >> output
	echo Padded: >> output
	../../babel -ifid padded.z5 >> output
	echo Unclosed: >> output
	../../babel -ifid unclosed.z5 >> output
	../../babel -ifid cut-short.z5 >> output
	$(VERIFY)

clean:
	-rm -f output
