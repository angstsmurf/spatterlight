//  Extracted from NSDate-Extensions
// by Erica Sadun, http://ericasadun.com
// https://github.com/erica/NSDate-Extensions

#import <Foundation/Foundation.h>

@interface NSDate (relative)

+ (NSCalendar *) currentCalendar; // avoid bottlenecks

@property (readonly, copy) NSString *formattedRelativeString;

@property (NS_NONATOMIC_IOSONLY, getter=isToday, readonly) BOOL today;
@property (NS_NONATOMIC_IOSONLY, getter=isYesterday, readonly) BOOL yesterday;

- (BOOL) isSameWeekAsDate: (NSDate *) aDate;
@property (NS_NONATOMIC_IOSONLY, getter=isThisWeek, readonly) BOOL thisWeek;

- (NSDate *) dateByAddingDays: (NSInteger) dDays;
- (NSDate *) dateBySubtractingDays: (NSInteger) dDays;

@end
