/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- Phase 0 harness.
 *
 * Reads an ADRIFT 5 Blorb (or bare .taf), locates the 'ADRI' Exec payload,
 * de-obfuscates and zlib-inflates it, and writes the resulting UTF-8 XML to
 * stdout (diagnostics go to stderr).  This proves the container + cipher +
 * inflate pipeline before any of the object model exists.
 *
 *   ./a5dump <game.blorb|game.taf>  > game.xml
 *
 * Concepts borrowed from Bocfel (blorb/iff) and frankendrift (FileIO).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a5blorb.h"
#include "a5deobf.h"

static uint8_t *
read_file (const char *path, uint32_t *out_len)
{
  FILE *fp;
  long size;
  uint8_t *buf;

  fp = fopen (path, "rb");
  if (fp == NULL)
    {
      fprintf (stderr, "a5dump: cannot open %s\n", path);
      return NULL;
    }
  fseek (fp, 0, SEEK_END);
  size = ftell (fp);
  fseek (fp, 0, SEEK_SET);
  if (size <= 0)
    {
      fclose (fp);
      fprintf (stderr, "a5dump: empty or unreadable file %s\n", path);
      return NULL;
    }
  buf = (uint8_t *) malloc ((size_t) size);
  if (buf == NULL || fread (buf, 1, (size_t) size, fp) != (size_t) size)
    {
      free (buf);
      fclose (fp);
      fprintf (stderr, "a5dump: read error on %s\n", path);
      return NULL;
    }
  fclose (fp);
  *out_len = (uint32_t) size;
  return buf;
}

static void
print_fourcc (FILE *fp, uint32_t cc)
{
  fprintf (fp, "%c%c%c%c",
           (cc >> 24) & 0xff, (cc >> 16) & 0xff, (cc >> 8) & 0xff, cc & 0xff);
}

int
main (int argc, char **argv)
{
  uint8_t *file_buf, *payload, *xml;
  uint32_t file_len, payload_len, header, trailer, region_len, xml_len;
  a5_blorb_chunk_t chunk;
  int is_v5;

  if (argc != 2)
    {
      fprintf (stderr, "usage: %s <game.blorb|game.taf>\n", argv[0]);
      return 2;
    }

  file_buf = read_file (argv[1], &file_len);
  if (file_buf == NULL)
    return 1;

  /* Locate the ADRIFT payload: the Exec chunk of a Blorb, else the whole file. */
  if (a5blorb_find_exec (file_buf, file_len, &chunk))
    {
      fprintf (stderr, "blorb: Exec chunk type '");
      print_fourcc (stderr, chunk.type);
      fprintf (stderr, "', %u bytes\n", chunk.size);
      payload = (uint8_t *) chunk.data; /* aliases into our own buffer */
      payload_len = chunk.size;
      if (chunk.type != A5_CHUNK_ADRI)
        fprintf (stderr, "blorb: warning: Exec chunk is not 'ADRI'\n");
    }
  else
    {
      fprintf (stderr, "no blorb wrapper; treating whole file as bare .taf\n");
      payload = file_buf;
      payload_len = file_len;
    }

  if (payload_len < 16)
    {
      fprintf (stderr, "a5dump: payload too short (%u bytes)\n", payload_len);
      free (file_buf);
      return 1;
    }

  /*
   * Version marker: an ADRIFT 5 payload carries the 12-byte signature then a
   * 4-byte hex size field at offset 12 (frankendrift FileIO.vb:800).  A size of
   * "0000" means an empty (absent) Babel <ifindex> block; a non-empty block is
   * detectable by the literal "<ifindex" tag at offset 16.  In either case the
   * obfuscated/deflated region starts at 16 + <hex size>; otherwise (older v5
   * layout) at 12.  Either way a 14-byte trailer follows the stream.  This must
   * mirror a5model.cpp's loader exactly (see a5_load_advmodel).
   */
  {
    int is_zero_size = (payload[12] == '0' && payload[13] == '0'
                         && payload[14] == '0' && payload[15] == '0');
    int has_ifindex = (payload_len >= 24
                        && memcmp (payload + 16, "<ifindex", 8) == 0);
    if (is_zero_size || has_ifindex)
      {
        char size_hex[5];
        memcpy (size_hex, payload + 12, 4);
        size_hex[4] = '\0';
        header = 16 + (uint32_t) strtoul (size_hex, NULL, 16);
        is_v5 = 1;
      }
    else
      {
        header = 12;
        is_v5 = 0;
      }
  }
  trailer = 14;

  fprintf (stderr, "signature:");
  {
    int i;
    for (i = 0; i < 16; i++)
      fprintf (stderr, " %02x", payload[i]);
  }
  fprintf (stderr, "\nversion marker: %s (header=%u, trailer=%u)\n",
           is_v5 ? "v5 (ifindex/size field)" : "non-v5", header, trailer);

  if (payload_len < header + trailer)
    {
      fprintf (stderr, "a5dump: payload too short for header+trailer\n");
      free (file_buf);
      return 1;
    }
  region_len = payload_len - header - trailer;

  /* De-obfuscate the zlib region in place, then inflate. */
  a5_deobfuscate (payload, header, region_len);
  fprintf (stderr, "zlib region: offset %u, length %u; first bytes %02x %02x\n",
           header, region_len, payload[header], payload[header + 1]);

  xml = a5_inflate (payload + header, region_len, &xml_len);
  if (xml == NULL)
    {
      fprintf (stderr, "a5dump: inflate failed\n");
      free (file_buf);
      return 1;
    }

  fprintf (stderr, "inflated XML: %u bytes\n", xml_len);
  fwrite (xml, 1, xml_len, stdout);

  free (xml);
  free (file_buf);
  return 0;
}
