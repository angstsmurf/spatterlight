//
//  IFStory.m
//  Yazmin
//
//  Created by David Schweinsberg on 26/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import "YazIFStory.h"
#import "YazIFAnnotation.h"
#import "YazIFBibliographic.h"
#import "IFColophon.h"
#import "IFDB.h"
#import "YazIFIdentification.h"
#import "IFYazmin.h"

@implementation IFStory

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
  self = [super init];
  if (self) {
    NSXMLElement *idElement = [element elementsForName:@"identification"][0];
    _identification = [[IFIdentification alloc] initWithXMLElement:idElement];

    NSXMLElement *biblioElement = [element elementsForName:@"bibliographic"][0];
    _bibliographic = [[IFBibliographic alloc] initWithXMLElement:biblioElement];

    NSArray<NSXMLElement *> *elements = [element elementsForName:@"colophon"];
    if (elements.count > 0) {
      NSXMLElement *colophonElement = elements[0];
      _colophon = [[IFColophon alloc] initWithXMLElement:colophonElement];
    }

    elements = [element elementsForName:@"annotation"];
    if (elements.count > 0)
      _annotation =
          [[IFAnnotation alloc] initWithXMLElement:elements.firstObject];

    elements = [element elementsForLocalName:@"ifdb"
                                         URI:@"http://ifdb.tads.org/api/xmlns"];
    if (elements.count > 0)
      _ifdb = [[IFDB alloc] initWithXMLElement:elements.firstObject];
  }
  return self;
}

- (instancetype)initWithIFID:(NSString *)ifid storyURL:(NSURL *)storyURL {
  self = [super init];
  if (self) {
    _identification = [[IFIdentification alloc] initWithIFID:ifid];
    _bibliographic = [[IFBibliographic alloc] init];
    _colophon = [[IFColophon alloc] init];
    _annotation = [[IFAnnotation alloc] init];
    _annotation.yazmin.storyURL = storyURL;
  }
  return self;
}

- (void)updateFromStory:(IFStory *)story {
  // Don't update identifications

  // Update bibliographic fields that aren't nil
  // (except group, for which we'll keep the user-specified group if not nil)
  IFBibliographic *bib = story.bibliographic;
  if (bib.title)
    _bibliographic.title = bib.title;
  if (bib.author)
    _bibliographic.author = bib.author;
  if (bib.language)
    _bibliographic.language = bib.language;
  if (bib.headline)
    _bibliographic.headline = bib.headline;
  if (bib.firstPublished)
    _bibliographic.firstPublished = bib.firstPublished;
  if (bib.genre)
    _bibliographic.genre = bib.genre;
  if (!_bibliographic.group && bib.group)
    _bibliographic.group = bib.group;
  if (bib.storyDescription)
    _bibliographic.storyDescription = bib.storyDescription;
  if (bib.series) {
    _bibliographic.series = bib.series;
    _bibliographic.seriesNumber = bib.seriesNumber;
  }
  if (bib.forgiveness)
    _bibliographic.forgiveness = bib.forgiveness;

  _colophon = story.colophon;
  // Don't update annotations
}

- (NSString *)xmlString {
  NSMutableString *string = [NSMutableString string];
  [string appendString:@"<story>\n"];
  [string appendString:_identification.xmlString];
  [string appendString:_bibliographic.xmlString];
  if (_colophon)
    [string appendString:_colophon.xmlString];
  if (_annotation)
    [string appendString:_annotation.xmlString];
  [string appendString:@"</story>\n"];
  return string;
}

@end
