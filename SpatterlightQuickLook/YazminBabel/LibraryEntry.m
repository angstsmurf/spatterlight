//
//  LibraryEntry.m
//  Yazmin
//
//  Created by David Schweinsberg on 27/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import "LibraryEntry.h"
#import "YazIFAnnotation.h"
#import "YazIFBibliographic.h"
#import "YazIFIdentification.h"
#import "YazIFStory.h"
#import "IFYazmin.h"

@interface LibraryEntry ()

+ (NSURL *)URLRelativeToLibrary:(NSURL *)storyURL;

@end

@implementation LibraryEntry

+ (NSURL *)URLRelativeToLibrary:(NSURL *)storyURL {
  NSUserDefaults *userDefaults =
      [[NSUserDefaults alloc] initWithSuiteName:NSArgumentDomain];
  NSString *libraryURLStr = [userDefaults stringForKey:@"LibraryURL"];
  NSURL *libraryURL = [NSURL URLWithString:libraryURLStr];
  NSArray<NSString *> *components = libraryURL.pathComponents;
  NSArray<NSString *> *rootComponents =
      [components subarrayWithRange:NSMakeRange(0, components.count - 1)];
  NSArray<NSString *> *fileComponents =
      [rootComponents arrayByAddingObjectsFromArray:storyURL.pathComponents];
  return [NSURL fileURLWithPathComponents:fileComponents];
}

- (instancetype)initWithStoryMetadata:(IFStory *)storyMetadata {
  self = [super init];
  if (self) {
    _storyMetadata = storyMetadata;
  }
  return self;
}

- (void)updateFromStory:(IFStory *)story {
  [_storyMetadata updateFromStory:story];
}

- (NSString *)ifid {
  return _storyMetadata.identification.ifids.firstObject;
}

- (NSURL *)fileURL {
  NSURL *storyURL = _storyMetadata.annotation.yazmin.storyURL;
  if (storyURL.scheme == nil) {
    storyURL = [LibraryEntry URLRelativeToLibrary:storyURL];
  }
  return storyURL;
}

- (NSString *)title {
  if (_storyMetadata && _storyMetadata.bibliographic.title &&
      ![_storyMetadata.bibliographic.title isEqualToString:@""])
    return _storyMetadata.bibliographic.title;
  else
    return self.fileURL.path.lastPathComponent;
}

- (NSString *)sortTitle {
  NSString *title = [self.title lowercaseString];
  if ([title hasPrefix:@"a "])
    return [self.title substringFromIndex:2];
  else if ([title hasPrefix:@"the "])
    return [self.title substringFromIndex:4];
  else
    return self.title;
}

- (NSString *)author {
  return _storyMetadata.bibliographic.author;
}

- (NSString *)genre {
  return _storyMetadata.bibliographic.genre;
}

- (NSString *)group {
  return _storyMetadata.bibliographic.group;
}

- (NSString *)firstPublished {
  return _storyMetadata.bibliographic.firstPublished;
}

@end
