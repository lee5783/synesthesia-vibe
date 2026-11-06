//
//  SidebarViewController.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-06.
//

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@class SidebarViewController;

@protocol SidebarViewControllerDelegate <NSObject>

- (void)sidebarViewController:(SidebarViewController *)controller didSelectPresetAtURL:(NSURL *)url;
- (void)sidebarViewController:(SidebarViewController *)controller didChooseFolderAtURL:(NSURL *)url;

@end

@interface SidebarViewController : NSViewController

@property (nonatomic, weak, nullable) id<SidebarViewControllerDelegate> delegate;
@property (nonatomic, copy) NSArray<NSURL *> *presetURLs;
@property (nonatomic, copy, nullable) NSURL *folderURL;
@property (nonatomic, copy, nullable) NSURL *selectedPresetURL;

- (void)selectPresetAtURL:(NSURL *)url;
- (void)setAudioHint:(nullable NSString *)hint;

@end

NS_ASSUME_NONNULL_END
