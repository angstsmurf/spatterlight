//  Extracted from NSDate-Extensions
// by Erica Sadun, http://ericasadun.com
// https://github.com/erica/NSDate-Extensions

#define D_WEEK		604800

// Thanks, AshFurrow
static const unsigned componentFlags = (NSYearCalendarUnit| NSMonthCalendarUnit | NSDayCalendarUnit | NSWeekCalendarUnit |  NSHourCalendarUnit | NSMinuteCalendarUnit | NSSecondCalendarUnit | NSWeekdayCalendarUnit | NSWeekdayOrdinalCalendarUnit);

@implementation NSDate (relative)

// Courtesy of Lukasz Margielewski
// Updated via Holger Haenisch
+ (NSCalendar *) currentCalendar {
    static NSCalendar *sharedCalendar = nil;
    if (!sharedCalendar)
        sharedCalendar = [NSCalendar autoupdatingCurrentCalendar];
    return sharedCalendar;
}


- (NSString *)formattedRelativeString {

    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    dateFormatter.doesRelativeDateFormatting = YES;
    [dateFormatter setDoesRelativeDateFormatting:YES];

    if([self isToday]) {
        dateFormatter.timeStyle = NSDateFormatterShortStyle;
        dateFormatter.dateStyle = NSDateFormatterNoStyle;
    }
    else if ([self isYesterday]) {
        dateFormatter.timeStyle = NSDateFormatterNoStyle;
        dateFormatter.dateStyle = NSDateFormatterMediumStyle;
    }
    else if ([self isThisWeek]) {
        NSDateComponents *weekdayComponents = [[NSCalendar currentCalendar] components:kCFCalendarUnitWeekday fromDate:self];
        NSUInteger weekday = (NSUInteger)weekdayComponents.weekday;
        return [dateFormatter.weekdaySymbols objectAtIndex:weekday - 1];
    } else {
        dateFormatter.timeStyle = NSDateFormatterNoStyle;
        dateFormatter.dateStyle = NSDateFormatterShortStyle;
    }

    return [dateFormatter stringFromDate:self];
}



#pragma mark - Comparing Dates

- (BOOL) isEqualToDateIgnoringTime: (NSDate *) aDate {
	NSDateComponents *components1 = [[NSDate currentCalendar] components:componentFlags fromDate:self];
	NSDateComponents *components2 = [[NSDate currentCalendar] components:componentFlags fromDate:aDate];
	return ((components1.year == components2.year) &&
			(components1.month == components2.month) &&
			(components1.day == components2.day));
}

- (BOOL) isToday {
	return [self isEqualToDateIgnoringTime:[NSDate date]];
}

- (BOOL) isYesterday {
	return [self isEqualToDateIgnoringTime:[NSDate dateYesterday]];
}

// This hard codes the assumption that a week is 7 days
- (BOOL) isSameWeekAsDate: (NSDate *) aDate {
	NSDateComponents *components1 = [[NSDate currentCalendar] components:componentFlags fromDate:self];
	NSDateComponents *components2 = [[NSDate currentCalendar] components:componentFlags fromDate:aDate];

	// Must be same week. 12/31 and 1/1 will both be week "1" if they are in the same week
	if (components1.week != components2.week) return NO;

	// Must have a time interval under 1 week. Thanks @aclark
	return (abs((int)[self timeIntervalSinceDate:aDate]) < D_WEEK);
}

- (BOOL) isThisWeek {
	return [self isSameWeekAsDate:[NSDate date]];
}

#pragma mark - Adjusting Dates

// Courtesy of dedan who mentions issues with Daylight Savings
- (NSDate *) dateByAddingDays: (NSInteger) dDays {
    NSDateComponents *dateComponents = [[NSDateComponents alloc] init];
    [dateComponents setDay:dDays];
    NSDate *newDate = [[NSCalendar currentCalendar] dateByAddingComponents:dateComponents toDate:self options:0];
    return newDate;
}

#pragma mark - Relative Dates

+ (NSDate *) dateYesterday {
	return [[NSDate date] dateByAddingDays:-1];
}

@end
