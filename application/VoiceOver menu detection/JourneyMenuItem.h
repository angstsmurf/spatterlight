//
//  JourneyMenuItem.h
//  Spatterlight
//
//  Created by Administrator on 2024-01-28.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// One entry of a Journey menu, as sent by the interpreter: the text and
// the grid position a click on it should go to.
@interface JourneyMenuItem : NSObject <NSSecureCoding>

@property NSString *title;
@property NSUInteger column;
@property NSUInteger line;
@property NSInteger tag;

// For verbs: the title of the party member the verb belongs to.
@property (nullable) NSString *actor;

- (instancetype)initWithTitle:(NSString *)title column:(NSUInteger)column line:(NSUInteger)line tag:(NSInteger)tag;

// The grid cell a click on this item goes to, in a grid window of gridSize
// characters. bordered is YES when the grid has a border (the Amiga look),
// which moves the menus up a line. Returns NO if the cell is outside the
// grid, which no menu the interpreter sends reaches unless the grid is
// smaller than 15 × 5.
- (BOOL)getClickPoint:(NSPoint *)point inGridOfSize:(NSSize)gridSize bordered:(BOOL)bordered;

@end

NS_ASSUME_NONNULL_END
