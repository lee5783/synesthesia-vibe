//
//  OverVideoButtonView.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-06.
//

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface OverVideoButtonView : NSControl

@property (nonatomic, copy) NSString *symbolName;
@property (nonatomic, assign, getter=isOn) BOOL on;

- (instancetype)initWithSymbolName:(NSString *)symbolName label:(NSString *)label toolTip:(nullable NSString *)toolTip;

@end

NS_ASSUME_NONNULL_END
