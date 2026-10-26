# ``BlorbFramework``

Read-only parsing of Blorb resource files and Quetzal save headers, shared by
Spatterlight, its QuickLook preview and its thumbnail extension.

## Overview

``Blorb`` takes the contents of a Blorb file (`FORM IFRS`) and indexes its
resources: the executable, pictures and sounds listed in the `RIdx` chunk, plus
the metadata (`IFmd`), frontispiece (`Fspc`), Z-code identifier (`IFhd`),
resource descriptions (`RDes`) and text chunks (`AUTH`, `ANNO`, `(c) `,
`SNam`). It never modifies the data and is written to cope with truncated or
corrupt files: a length that runs past the end of the buffer means the chunk
is skipped, not read.

``Blorb/coverImageData`` picks the cover: the `Fspc` picture if there is one,
otherwise the first picture that is not on a list of known placeholder images.

The C functions in `iff.h` are the bounds-checked IFF primitives the parser is
built on. `parseQuetzalHeader` reads the game identifier of a Quetzal save
with the same primitives.

## Topics

### Blorb files

- ``Blorb``
- ``BlorbResource``
