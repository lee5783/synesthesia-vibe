//
//  SidebarViewController.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-06.
//

#import "SidebarViewController.h"

#import "SVColorTokens.h"

static const CGFloat SidebarRowHeight = 28.0;
static const CGFloat SidebarHeaderHeight = 28.0;
static const CGFloat SidebarFooterHeight = 34.0;
static NSUserInterfaceItemIdentifier const SidebarPresetColumn = @"preset";

@interface SidebarViewController () <NSTableViewDataSource, NSTableViewDelegate>

@property (nonatomic, strong) NSTextField *headerLabel;
@property (nonatomic, strong) NSTableView *tableView;
@property (nonatomic, strong) NSTextField *footerLabel;
@property (nonatomic, copy) NSString *audioHint;

@end

@implementation SidebarViewController

- (instancetype)init
{
    self = [super initWithNibName:nil bundle:nil];
    if (self)
        _presetURLs = @[];
    return self;
}

- (void)loadView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 260.0, 600.0)];
    view.wantsLayer = YES;
    view.layer.backgroundColor = SVColorTokens.backgroundSecondary.CGColor;

    self.headerLabel = [NSTextField labelWithString:@""];
    self.headerLabel.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightSemibold];
    self.headerLabel.textColor = SVColorTokens.textMuted;
    self.headerLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    self.headerLabel.translatesAutoresizingMaskIntoConstraints = NO;

    NSButton *folderButton = [NSButton buttonWithImage:[NSImage imageWithSystemSymbolName:@"folder.badge.plus" accessibilityDescription:@"Choose presets folder"]
                                                target:self
                                                action:@selector(chooseFolder:)];
    folderButton.bezelStyle = NSBezelStyleAccessoryBarAction;
    folderButton.bordered = NO;
    folderButton.contentTintColor = SVColorTokens.textSecondary;
    folderButton.toolTip = @"Choose presets folder";
    folderButton.translatesAutoresizingMaskIntoConstraints = NO;

    NSView *header = [[NSView alloc] initWithFrame:NSZeroRect];
    header.translatesAutoresizingMaskIntoConstraints = NO;
    [header addSubview:self.headerLabel];
    [header addSubview:folderButton];

    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:SidebarPresetColumn];
    column.resizingMask = NSTableColumnAutoresizingMask;

    self.tableView = [[NSTableView alloc] initWithFrame:NSZeroRect];
    [self.tableView addTableColumn:column];
    self.tableView.style = NSTableViewStyleSourceList;
    self.tableView.rowHeight = SidebarRowHeight;
    self.tableView.headerView = nil;
    self.tableView.backgroundColor = NSColor.clearColor;
    self.tableView.dataSource = self;
    self.tableView.delegate = self;
    self.tableView.allowsEmptySelection = NO;

    NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    scrollView.documentView = self.tableView;
    scrollView.drawsBackground = NO;
    scrollView.hasVerticalScroller = YES;
    scrollView.translatesAutoresizingMaskIntoConstraints = NO;

    NSImageView *folderIcon = [[NSImageView alloc] initWithFrame:NSZeroRect];
    folderIcon.image = [NSImage imageWithSystemSymbolName:@"folder" accessibilityDescription:nil];
    folderIcon.contentTintColor = SVColorTokens.info;
    folderIcon.translatesAutoresizingMaskIntoConstraints = NO;

    self.footerLabel = [NSTextField labelWithString:@""];
    self.footerLabel.font = [NSFont systemFontOfSize:11.0];
    self.footerLabel.textColor = SVColorTokens.textMuted;
    self.footerLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
    self.footerLabel.translatesAutoresizingMaskIntoConstraints = NO;

    NSView *footer = [[NSView alloc] initWithFrame:NSZeroRect];
    footer.translatesAutoresizingMaskIntoConstraints = NO;
    [footer addSubview:folderIcon];
    [footer addSubview:self.footerLabel];

    [view addSubview:header];
    [view addSubview:scrollView];
    [view addSubview:footer];

    [NSLayoutConstraint activateConstraints:@[
        [header.topAnchor constraintEqualToAnchor:view.topAnchor constant:4.0],
        [header.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
        [header.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
        [header.heightAnchor constraintEqualToConstant:SidebarHeaderHeight],
        [self.headerLabel.leadingAnchor constraintEqualToAnchor:header.leadingAnchor constant:18.0],
        [self.headerLabel.centerYAnchor constraintEqualToAnchor:header.centerYAnchor],
        [folderButton.leadingAnchor constraintEqualToAnchor:self.headerLabel.trailingAnchor constant:6.0],
        [folderButton.trailingAnchor constraintEqualToAnchor:header.trailingAnchor constant:-10.0],
        [folderButton.centerYAnchor constraintEqualToAnchor:header.centerYAnchor],

        [scrollView.topAnchor constraintEqualToAnchor:header.bottomAnchor],
        [scrollView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
        [scrollView.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
        [scrollView.bottomAnchor constraintEqualToAnchor:footer.topAnchor],

        [footer.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
        [footer.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
        [footer.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
        [footer.heightAnchor constraintEqualToConstant:SidebarFooterHeight],
        [folderIcon.leadingAnchor constraintEqualToAnchor:footer.leadingAnchor constant:16.0],
        [folderIcon.centerYAnchor constraintEqualToAnchor:footer.centerYAnchor],
        [self.footerLabel.leadingAnchor constraintEqualToAnchor:folderIcon.trailingAnchor constant:6.0],
        [self.footerLabel.trailingAnchor constraintEqualToAnchor:footer.trailingAnchor constant:-12.0],
        [self.footerLabel.centerYAnchor constraintEqualToAnchor:footer.centerYAnchor]
    ]];

    self.view = view;
}

- (void)setPresetURLs:(NSArray<NSURL *> *)presetURLs
{
    _presetURLs = [presetURLs copy];
    [self.tableView reloadData];
    [self updateHeader];
}

- (void)setFolderURL:(nullable NSURL *)folderURL
{
    _folderURL = [folderURL copy];
    [self updateHeader];
}

- (void)setSelectedPresetURL:(nullable NSURL *)selectedPresetURL
{
    _selectedPresetURL = [selectedPresetURL copy];
    [self syncSelection];
}

- (void)selectPresetAtURL:(NSURL *)url
{
    self.selectedPresetURL = url;
    [self syncSelection];
}

- (void)setAudioHint:(nullable NSString *)hint
{
    _audioHint = [hint copy];
}

- (void)chooseFolder:(id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseDirectories = YES;
    panel.canChooseFiles = NO;
    panel.allowsMultipleSelection = NO;
    panel.prompt = @"Choose";
    [panel beginSheetModalForWindow:self.view.window completionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK && panel.URL != nil)
            [self.delegate sidebarViewController:self didChooseFolderAtURL:panel.URL];
    }];
}

- (void)updateHeader
{
    NSString *name = self.folderURL != nil ? self.folderURL.lastPathComponent : self.folderURL.path;
    if (name.length == 0)
        name = @"presets";
    self.headerLabel.stringValue = [NSString stringWithFormat:@"%@ · %lu", name, (unsigned long)self.presetURLs.count];
    self.footerLabel.stringValue = [self shortenedPath];
}

- (NSString *)shortenedPath
{
    NSString *path = self.folderURL.path;
    if (path.length == 0)
        return @"";
    NSString *home = NSHomeDirectory();
    if ([path hasPrefix:home])
        return [@"~" stringByAppendingString:[path substringFromIndex:home.length]];
    return path;
}

- (void)syncSelection
{
    NSUInteger index = self.selectedPresetURL != nil ? [self.presetURLs indexOfObject:self.selectedPresetURL] : NSNotFound;
    if (index == NSNotFound || index >= self.presetURLs.count)
        return;
    if (self.tableView.selectedRow == (NSInteger)index)
        return;
    [self.tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:index] byExtendingSelection:NO];
    [self.tableView scrollRowToVisible:(NSInteger)index];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return (NSInteger)self.presetURLs.count;
}

- (nullable NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(nullable NSTableColumn *)tableColumn row:(NSInteger)row
{
    NSTableCellView *cell = [tableView makeViewWithIdentifier:SidebarPresetColumn owner:self];
    if (cell == nil) {
        NSImageView *icon = [[NSImageView alloc] initWithFrame:NSZeroRect];
        icon.image = [NSImage imageWithSystemSymbolName:@"waveform" accessibilityDescription:nil];
        icon.contentTintColor = SVColorTokens.textMuted;
        icon.translatesAutoresizingMaskIntoConstraints = NO;

        NSTextField *textField = [NSTextField labelWithString:@""];
        textField.lineBreakMode = NSLineBreakByTruncatingTail;
        textField.translatesAutoresizingMaskIntoConstraints = NO;

        cell = [[NSTableCellView alloc] initWithFrame:NSZeroRect];
        cell.identifier = SidebarPresetColumn;
        cell.textField = textField;
        [cell addSubview:icon];
        [cell addSubview:textField];
        [NSLayoutConstraint activateConstraints:@[
            [icon.leadingAnchor constraintEqualToAnchor:cell.leadingAnchor constant:2.0],
            [icon.centerYAnchor constraintEqualToAnchor:cell.centerYAnchor],
            [icon.widthAnchor constraintEqualToConstant:16.0],
            [textField.leadingAnchor constraintEqualToAnchor:icon.trailingAnchor constant:8.0],
            [textField.trailingAnchor constraintEqualToAnchor:cell.trailingAnchor constant:-4.0],
            [textField.centerYAnchor constraintEqualToAnchor:cell.centerYAnchor]
        ]];
    }

    NSURL *url = self.presetURLs[(NSUInteger)row];
    cell.textField.stringValue = url.URLByDeletingPathExtension.lastPathComponent;
    cell.accessibilityLabel = url.URLByDeletingPathExtension.lastPathComponent;
    return cell;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSInteger row = self.tableView.selectedRow;
    if (row < 0 || (NSUInteger)row >= self.presetURLs.count)
        return;
    [self.delegate sidebarViewController:self didSelectPresetAtURL:self.presetURLs[(NSUInteger)row]];
}

@end
