//
//  LibHelperTableView.m
//  Spatterlight
//
//  Extracted from TableViewController.m
//

#import "LibHelperTableView.h"
#import "TableViewController.h"
#import "TableViewController+InfoWindows.h"
#import "TableViewController+TableDelegate.h"

@interface LibHelperTableView () {
    NSTrackingArea *trackingArea;
    BOOL mouseOverView;
    NSInteger lastOverRow;
}
@end

@implementation LibHelperTableView

-(BOOL)becomeFirstResponder {
    BOOL flag = [super becomeFirstResponder];
    if (flag) {
        [(TableViewController *)self.delegate enableClickToRenameAfterDelay];
    }
    return flag;
}

- (void)keyDown:(NSEvent *)event {
    NSString *pressed = event.characters;
    if ([pressed isEqualToString:@" "])
        [(TableViewController *)self.delegate showGameInfo:nil];
    else
        [super keyDown:event];
}

- (void)awakeFromNib {
    [super awakeFromNib];
    trackingArea = [[NSTrackingArea alloc] initWithRect:self.frame options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways) owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
    mouseOverView = NO;
    _mouseOverRow = -1;
    lastOverRow = -1;
}

- (void)mouseEntered:(NSEvent*)theEvent {
    mouseOverView = YES;
}

- (void)mouseMoved:(NSEvent*)theEvent {
    if (mouseOverView) {
        _mouseOverRow = [self rowAtPoint:[self convertPoint:theEvent.locationInWindow fromView:nil]];
        if (lastOverRow == _mouseOverRow)
            return;
        else {
            [(TableViewController *)self.delegate mouseOverChangedFromRow:lastOverRow toRow:_mouseOverRow];
            lastOverRow = _mouseOverRow;
        }
    }
}

- (void)mouseExited:(NSEvent *)theEvent {
    mouseOverView = NO;
    [(TableViewController *)self.delegate mouseOverChangedFromRow:lastOverRow toRow:-1];
    _mouseOverRow = -1;
    lastOverRow = -1;
}

- (void)updateTrackingAreas {
    [self removeTrackingArea:trackingArea];
    trackingArea = [[NSTrackingArea alloc] initWithRect:self.frame options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways) owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
}

@end

@implementation CellViewWithConstraint
@end

@implementation MyIndicator
- (void)mouseDown:(NSEvent *)event {
    if (_tableView) {
        [_tableView mouseDown:event];
    } else {
        [super mouseDown:event];
    }
}

@end

@implementation RatingsCellView
@end

@implementation ForgivenessCellView
@end

@implementation LikeCellView
@end
