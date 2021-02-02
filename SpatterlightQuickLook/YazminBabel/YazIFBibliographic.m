//
//  IFBibliographic.m
//  Yazmin
//
//  Created by David Schweinsberg on 25/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import "YazIFBibliographic.h"

@interface IFBibliographic ()

- (NSString *)renderDescriptionElement:(NSXMLElement *)element;
- (NSString *)encodeString:(NSString *)original;

@end

@implementation IFBibliographic

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
  self = [super init];
  if (self) {
    NSEnumerator *enumChildren = [element.children objectEnumerator];
    NSXMLNode *node;
    while ((node = [enumChildren nextObject])) {
      if ([node.name compare:@"title"] == 0) {
        _title = node.stringValue;
      } else if ([node.name compare:@"author"] == 0) {
        _author = node.stringValue;
      } else if ([node.name compare:@"language"] == 0) {
        _language = node.stringValue;
      } else if ([node.name compare:@"headline"] == 0) {
        _headline = node.stringValue;
      } else if ([node.name compare:@"firstpublished"] == 0) {
        _firstPublished = node.stringValue;
      } else if ([node.name compare:@"genre"] == 0) {
        _genre = node.stringValue;
      } else if ([node.name compare:@"group"] == 0) {
        _group = node.stringValue;
      } else if ([node.name compare:@"description"] == 0) {
        _storyDescription =
            [self renderDescriptionElement:(NSXMLElement *)node];
      } else if ([node.name compare:@"series"] == 0) {
        _series = node.stringValue;
      } else if ([node.name compare:@"seriesnumber"] == 0) {
        _seriesNumber = node.stringValue.intValue;
      } else if ([node.name compare:@"forgiveness"] == 0) {
        _forgiveness = node.stringValue;
      }
    }
  }
  return self;
}

- (instancetype)init {
  self = [super init];
  if (self) {
  }
  return self;
}

- (NSString *)renderDescriptionElement:(NSXMLElement *)element {
  NSMutableString *string = [NSMutableString string];
  NSEnumerator *enumChildren = [element.children objectEnumerator];
  NSXMLNode *node;
  NSUInteger count = 0;
  while ((node = [enumChildren nextObject])) {
    if (node.kind == NSXMLTextKind) {
      if (count > 0)
        [string appendString:@"\n"];
      [string appendString:node.stringValue];
      ++count;
    }
  }
  return string;
}

- (NSString *)encodeString:(NSString *)original {
  NSMutableString *string = [NSMutableString string];
  for (NSUInteger i = 0; i < original.length; ++i) {
    unichar c = [original characterAtIndex:i];
    if (c == '&')
      [string appendString:@"&amp;"];
    else if (c == '<')
      [string appendString:@"&lt;"];
    else if (c == '>')
      [string appendString:@"&gt;"];
    else if (c == '\n')
      [string appendString:@"<br/>"];
    else
      [string appendFormat:@"%C", c];
  }
  return string;
}

- (NSString *)xmlString {
  NSMutableString *string = [NSMutableString string];
  [string appendString:@"<bibliographic>\n"];
  [string appendFormat:@"<title>%@</title>\n", [self encodeString:_title]];
  [string appendFormat:@"<author>%@</author>\n", [self encodeString:_author]];
  if (_language)
    [string appendFormat:@"<language>%@</language>\n",
                         [self encodeString:_language]];
  if (_headline)
    [string appendFormat:@"<headline>%@</headline>\n",
                         [self encodeString:_headline]];
  if (_firstPublished)
    [string appendFormat:@"<firstpublished>%@</firstpublished>\n",
                         [self encodeString:_firstPublished]];
  if (_genre)
    [string appendFormat:@"<genre>%@</genre>\n", [self encodeString:_genre]];
  if (_group)
    [string appendFormat:@"<group>%@</group>\n", [self encodeString:_group]];
  if (_storyDescription) {
    [string appendFormat:@"<description>%@</description>\n",
                         [self encodeString:_storyDescription]];
  }
  if (_series)
    [string appendFormat:@"<series>%@</series>\n", [self encodeString:_series]];
  if (_seriesNumber)
    [string appendFormat:@"<seriesnumber>%d</seriesnumber>\n", _seriesNumber];
  if (_forgiveness)
    [string appendFormat:@"<forgiveness>%@</forgiveness>\n",
                         [self encodeString:_forgiveness]];
  [string appendString:@"</bibliographic>\n"];
  return string;
}

@end
