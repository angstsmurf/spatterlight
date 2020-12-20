//
//  ScalingScrollView.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-12-20.
//
//

#import <Foundation/Foundation.h>

@interface ScalingScrollView : NSScrollView

@property CGFloat scaleFactor;

- (IBAction)zoomToActualSize:(id)sender;
- (IBAction)zoomIn:(id)sender;
- (IBAction)zoomOut:(id)sender;

@end

