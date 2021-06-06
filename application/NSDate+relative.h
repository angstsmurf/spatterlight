//  Extracted from NSDate-Extensions
// by Erica Sadun, http://ericasadun.com
// https://github.com/erica/NSDate-Extensions

#import <Foundation/Foundation.h>

@interface NSDate (relative)

+ (NSCalendar *) currentCalendar; // avoid bottlenecks

@property (readonly, copy) NSString *formattedRelativeString;

- (BOOL) isToday;
- (BOOL) isYesterday;

- (BOOL) isSameWeekAsDate: (NSDate *) aDate;
- (BOOL) isThisWeek;

- (NSDate *) dateByAddingDays: (NSInteger) dDays;
- (NSDate *) dateBySubtractingDays: (NSInteger) dDays;

@end
