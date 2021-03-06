
/* babel.h
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2009-2017 Christoph Ender.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef babel_h_INCLUDED
#define babel_h_INCLUDED

#ifndef DISABLE_BABEL
#include <libxml/tree.h>
#endif

#include "../tools/types.h"
#include "../tools/unused.h"

struct babel_doc_entry
{
#ifndef DISABLE_BABEL
  xmlDocPtr babel_doc;
#else
  void *babel_doc;
#endif
  char *filename;
  long timestamp;
  bool uses_if_namespace;
};

struct babel_info
{
  struct babel_doc_entry **entries;
  int entries_allocated;
  int nof_entries;
};

struct babel_story_info
{
  uint16_t release_number;
  char *serial;
  int length;
  char *title;
  char *author;
  char *description;
  char *language;
};

void free_babel_info(struct babel_info *babel);
void free_babel_story_info(struct babel_story_info *b_info);
struct babel_info *load_babel_info();
struct babel_story_info *get_babel_story_info(uint16_t release, char *serial,
    uint16_t checksum, struct babel_info *babel, bool babel_from_blorb);
void store_babel_info_timestamps(struct babel_info *babel);
bool babel_files_have_changed(struct babel_info *babel);
bool babel_available();

#ifndef DISABLE_BABEL
struct babel_info *load_babel_info_from_blorb(z_file *infile, int length,
    char *filename, time_t last_mod_timestamp);
#else
struct babel_info *load_babel_info_from_blorb(z_file *UNUSED(infile),
    int UNUSED(length), char *UNUSED(filename),
    time_t UNUSED(last_mod_timestamp));
#endif

#endif /* babel_h_INCLUDED */

