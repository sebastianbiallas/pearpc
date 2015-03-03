#include <Cocoa/Cocoa.h>

void openDisplay()
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];


    NSRect frame = NSMakeRect(100, 100, 300, 500);
    NSWindow* window  = 
    [
        [
            [NSWindow alloc] initWithContentRect:frame
            styleMask:NSResizableWindowMask
            // NSBorderlessWindowMask
            backing:NSBackingStoreBuffered
            defer:NO
        ]
        autorelease
    ];
    [window setBackgroundColor:[NSColor blueColor]];
    [window makeKeyAndOrderFront:NSApp];

    NSAlert *alert = [[NSAlert alloc] init];

    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setMessageText:@"Delete the record?"];
    [alert setInformativeText:@"Deleted records cannot be restored."];
    [alert setAlertStyle:NSWarningAlertStyle];
    if ([alert runModal] == NSAlertFirstButtonReturn) {
    }
    [alert release];
    [pool release];
    //return 0;
}