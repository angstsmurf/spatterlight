// Code adapted from avf et al.
// See replies to https://stackoverflow.com/questions/21241690/time-date-from-nsdate-to-show-just-like-message-and-mail-app-in-ios

@implementation NSDate (relative)


- (NSString *)formattedRelativeString {

    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    dateFormatter.doesRelativeDateFormatting = YES;
    [dateFormatter setDoesRelativeDateFormatting:YES];

    if ([[NSCalendar currentCalendar] isDateInToday:self]) {
        dateFormatter.timeStyle = NSDateFormatterShortStyle;
        dateFormatter.dateStyle = NSDateFormatterNoStyle;
    }
    else if ([[NSCalendar currentCalendar] isDateInYesterday:self]) {
        dateFormatter.timeStyle = NSDateFormatterNoStyle;
        dateFormatter.dateStyle = NSDateFormatterMediumStyle;
    }
    else if (isThisWeek(self)) {
        NSInteger weekday = [[NSCalendar currentCalendar] component:NSCalendarUnitWeekday
                                                           fromDate:self];
        return dateFormatter.weekdaySymbols[weekday-1];
    } else {
        dateFormatter.timeStyle = NSDateFormatterNoStyle;
        dateFormatter.dateStyle = NSDateFormatterShortStyle;
    }

    return [dateFormatter stringFromDate:self];
}




BOOL isThisWeek(NSDate *self)
{

    NSDateComponents *offsetComponents = [[NSDateComponents alloc] init];
    offsetComponents.day = -6;  // 6 days back from today
    NSDate *weekAgo = [[NSCalendar currentCalendar] dateByAddingComponents:offsetComponents toDate:[NSDate date] options:0];

    //Not future or more than a week ago
    return ([[NSDate date] compare:self] == NSOrderedDescending) && (([weekAgo compare:self] == NSOrderedAscending));

}


@end
