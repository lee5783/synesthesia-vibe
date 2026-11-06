//
//  DebugOverlayView.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-06.
//

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface DebugOverlayView : NSView

@property (nonatomic, copy, nullable) NSString *fpsText;
@property (nonatomic, assign) BOOL fpsIsHealthy;
@property (nonatomic, copy) NSString *capText;
@property (nonatomic, copy) NSString *frameMillisecondsText;
@property (nonatomic, copy) NSString *presetText;
@property (nonatomic, copy) NSString *sizeText;
@property (nonatomic, copy) NSString *backendText;
@property (nonatomic, copy) NSString *audioText;
@property (nonatomic, copy) NSString *rateText;
@property (nonatomic, copy) NSString *autoText;
@property (nonatomic, copy, nullable) NSString *lastErrorText;
@property (nonatomic, assign) float bass;
@property (nonatomic, assign) float mid;
@property (nonatomic, assign) float treble;

@end

NS_ASSUME_NONNULL_END
