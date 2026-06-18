#include "macoshelper.h"

#import <AppKit/AppKit.h>

void SetMacApplicationVisibleInDock(bool visible)
{
    const NSApplicationActivationPolicy policy = visible
        ? NSApplicationActivationPolicyRegular
        : NSApplicationActivationPolicyAccessory;
    [NSApp setActivationPolicy:policy];
    if (visible) {
        [NSApp activateIgnoringOtherApps:YES];
    }
}

void InstallMacLiquidGlass(void *nativeView)
{
    NSView *qtView = (__bridge NSView *)nativeView;
    NSWindow *window = qtView.window;
    NSView *content = window.contentView;
    if (!window || !content) return;

    window.opaque = NO;
    window.backgroundColor = NSColor.clearColor;
    window.titlebarAppearsTransparent = YES;

    if (@available(macOS 26.0, *)) {
        if ([content isKindOfClass:NSGlassEffectView.class]) return;
        NSGlassEffectView *glass = [[NSGlassEffectView alloc] initWithFrame:content.bounds];
        glass.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        glass.style = NSGlassEffectViewStyleRegular;
        glass.cornerRadius = 12.0;
        glass.contentView = content;
        window.contentView = glass;
    } else {
        NSVisualEffectView *material = [[NSVisualEffectView alloc] initWithFrame:content.bounds];
        material.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        material.material = NSVisualEffectMaterialUnderWindowBackground;
        material.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        material.state = NSVisualEffectStateFollowsWindowActiveState;
        window.contentView = material;
        content.frame = material.bounds;
        content.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [material addSubview:content];
    }
}
