/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * CLI: hard-coded map fixture → map_render_svg → .svg + NSImage → .png
 *
 *   ./test/mapsvg_harness [-o out.png] [--svg out.svg] [-colour]
 */

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <cstdio>
#include <cstring>

#include "mapsvg_fixture.h"
#include "../mapdraw.h"

static void
usage (const char *argv0)
{
  fprintf (stderr,
           "Usage: %s [-o out.png] [--svg out.svg] [-colour]\n"
           "  -colour  draw in the alternative scheme \"glk map colour\" selects\n"
           "  Default outputs: mapsvg_out.png / mapsvg_out.svg\n",
           argv0);
}

static int
write_file (const char *path, const void *data, size_t len)
{
  FILE *f = fopen (path, "wb");
  if (f == NULL)
    {
      perror (path);
      return 0;
    }
  if (fwrite (data, 1, len, f) != len)
    {
      perror (path);
      fclose (f);
      return 0;
    }
  fclose (f);
  return 1;
}

static int
svg_to_png (const char *svg, size_t svg_len, const char *png_path)
{
  @autoreleasepool
    {
      NSData *svgData = [NSData dataWithBytes:svg length:svg_len];
      NSImage *image = [[NSImage alloc] initWithData:svgData];
      if (image == nil || image.size.width < 1 || image.size.height < 1)
        {
          fprintf (stderr, "NSImage failed to load SVG (%zu bytes)\n", svg_len);
          return 0;
        }

      NSSize size = image.size;
      NSRect rect = NSMakeRect (0, 0, size.width, size.height);
      NSBitmapImageRep *rep =
        [[NSBitmapImageRep alloc]
          initWithBitmapDataPlanes:NULL
                        pixelsWide:(NSInteger) size.width
                        pixelsHigh:(NSInteger) size.height
                     bitsPerSample:8
                   samplesPerPixel:4
                          hasAlpha:YES
                          isPlanar:NO
                    colorSpaceName:NSCalibratedRGBColorSpace
                       bytesPerRow:0
                      bitsPerPixel:0];
      if (rep == nil)
        {
          fprintf (stderr, "failed to create bitmap\n");
          return 0;
        }

      [NSGraphicsContext saveGraphicsState];
      NSGraphicsContext *gc =
        [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
      [NSGraphicsContext setCurrentContext:gc];
      [[NSColor whiteColor] setFill];
      NSRectFill (rect);
      [image drawInRect:rect
               fromRect:NSZeroRect
              operation:NSCompositingOperationSourceOver
               fraction:1.0
         respectFlipped:YES
                  hints:@{ NSImageHintInterpolation:
                             @(NSImageInterpolationHigh) }];
      [NSGraphicsContext restoreGraphicsState];

      NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG
                                      properties:@{}];
      if (png == nil)
        {
          fprintf (stderr, "PNG encode failed\n");
          return 0;
        }
      if (![png writeToFile:@(png_path) atomically:YES])
        {
          fprintf (stderr, "failed to write %s\n", png_path);
          return 0;
        }
      fprintf (stderr, "wrote %s (%.0fx%.0f)\n", png_path, size.width,
               size.height);
      return 1;
    }
}

int
main (int argc, char **argv)
{
  const char *png_path = "mapsvg_out.png";
  const char *svg_path = "mapsvg_out.svg";
  map_t map;
  map_view_t view;
  map_svg_t *svg;
  int i;
  int colour = 0;

  for (i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "-o") == 0 && i + 1 < argc)
        png_path = argv[++i];
      else if (strcmp (argv[i], "--svg") == 0 && i + 1 < argc)
        svg_path = argv[++i];
      else if (strcmp (argv[i], "-colour") == 0
               || strcmp (argv[i], "-color") == 0)
        colour = 1;
      else if (strcmp (argv[i], "-h") == 0
               || strcmp (argv[i], "--help") == 0)
        {
          usage (argv[0]);
          return 0;
        }
      else
        {
          usage (argv[0]);
          return 1;
        }
    }

  /* AppKit needs a shared application for some drawing paths. */
  [NSApplication sharedApplication];

  memset (&map, 0, sizeof map);
  memset (&view, 0, sizeof view);
  mapsvg_fixture_build (&map, &view);

  map_set_colour_scheme (colour ? MAP_SCHEME_DERIVED : MAP_SCHEME_STANDARD);

  svg = map_render_svg (&map, &view, "LocationHub");
  if (svg == NULL || svg->svg == NULL)
    {
      fprintf (stderr, "map_render_svg returned nothing\n");
      return 1;
    }

  {
    size_t len = strlen (svg->svg);
    if (!write_file (svg_path, svg->svg, len))
      {
        map_svg_free (svg);
        return 1;
      }
    fprintf (stderr, "wrote %s (%zu bytes)\n", svg_path, len);

    if (!svg_to_png (svg->svg, len, png_path))
      {
        map_svg_free (svg);
        return 1;
      }
  }

  map_svg_free (svg);
  return 0;
}
