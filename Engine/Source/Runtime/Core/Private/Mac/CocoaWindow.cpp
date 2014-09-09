// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "CocoaWindow.h"
#include "MacApplication.h"
#include "CocoaTextView.h"
#include "MacEvent.h"
#include "CocoaThread.h"

TArray< FCocoaWindow* > GRunningModalWindows;

NSString* NSWindowRedrawContents = @"NSWindowRedrawContents";
NSString* NSDraggingExited = @"NSDraggingExited";
NSString* NSDraggingUpdated = @"NSDraggingUpdated";
NSString* NSPrepareForDragOperation = @"NSPrepareForDragOperation";
NSString* NSPerformDragOperation = @"NSPerformDragOperation";

/**
 * Custom window class used for input handling
 */
@implementation FCocoaWindow

@synthesize bForwardEvents;
@synthesize TargetWindowMode;
@synthesize PreFullScreenRect;

- (id)initWithContentRect:(NSRect)ContentRect styleMask:(NSUInteger)Style backing:(NSBackingStoreType)BufferingType defer:(BOOL)Flag
{
	WindowMode = EWindowMode::Windowed;
	bAcceptsInput = false;
	bRoundedCorners = false;
	bDisplayReconfiguring = false;
	bDeferOrderFront = false;
	DeferOpacity = 0.0f;
	bRenderInitialised = false;
	bDeferSetFrame = false;
	bDeferSetOrigin = false;

	id NewSelf = [super initWithContentRect:ContentRect styleMask:Style backing:BufferingType defer:Flag];
	if(NewSelf)
	{
		bZoomed = [super isZoomed];
		self.bForwardEvents = true;
		self.TargetWindowMode = EWindowMode::Windowed;
		[super setAlphaValue:DeferOpacity];
		DeferFrame = [super frame];
		self.PreFullScreenRect = DeferFrame;
	}
	return NewSelf;
}

- (NSRect)openGLFrame
{
	if(self.TargetWindowMode == EWindowMode::Fullscreen || WindowMode == EWindowMode::Fullscreen)
	{
		return self.PreFullScreenRect;
	}
	else if([self styleMask] & (NSTexturedBackgroundWindowMask))
	{
		return (!bDeferSetFrame ? [self frame] : DeferFrame);
	}
	else
	{
		return (!bDeferSetFrame ? [[self contentView] frame] : [self contentRectForFrameRect:DeferFrame]);
	}
}


- (NSView*)openGLView
{
	if([self styleMask] & (NSTexturedBackgroundWindowMask))
	{
		NSView* SuperView = [[self contentView] superview];
		for(NSView* View in [SuperView subviews])
		{
			if([View isKindOfClass:[FCocoaTextView class]])
			{
				return View;
			}
		}
		return nil;
	}
	else
	{
		return [self contentView];
	}
}

- (void)performDeferredOrderFront
{
	if(!bRenderInitialised)
	{
		bRenderInitialised = true;
	}
	
	if(bDeferOrderFront)
	{
		if(!(bDeferSetFrame || bDeferSetOrigin))
		{
			bDeferOrderFront = false;
			[super setAlphaValue:DeferOpacity];
		}
		else
		{
			[self performDeferredSetFrame];
		}
	}
}

- (void)performDeferredSetFrame
{
	if(bRenderInitialised && (bDeferSetFrame || bDeferSetOrigin))
	{
		dispatch_block_t Block = ^{
			if(!bDeferSetFrame && bDeferSetOrigin)
			{
				DeferFrame.size = [self frame].size;
			}
			
			[super setFrame:DeferFrame display:YES];
		};
		
		if([NSThread isMainThread])
		{
			Block();
		}
		else
		{
			dispatch_async(dispatch_get_main_queue(), Block);
		}
		
		bDeferSetFrame = false;
		bDeferSetOrigin = false;
	}
}

- (void)orderWindow:(NSWindowOrderingMode)OrderingMode relativeTo:(NSInteger)OtherWindowNumber
{
	bool bModal = FMacWindow::CurrentModalWindow() == nil || FMacWindow::CurrentModalWindow() == self || [self styleMask] == NSBorderlessWindowMask;
	if(OrderingMode == NSWindowOut || bModal)
	{
		if([self alphaValue] > 0.0f)
		{
			[self performDeferredSetFrame];
		}
		[super orderWindow:OrderingMode relativeTo:OtherWindowNumber];
	}
}

- (bool)roundedCorners
{
    return bRoundedCorners;
}

- (void)setRoundedCorners:(bool)bUseRoundedCorners
{
	bRoundedCorners = bUseRoundedCorners;
}

- (void)setAcceptsInput:(bool)InAcceptsInput
{
	bAcceptsInput = InAcceptsInput;
}

- (void)redrawContents
{
	if(bNeedsRedraw && bForwardEvents && ([self isVisible] && [super alphaValue] > 0.0f))
	{
		NSNotification* Notification = [NSNotification notificationWithName:NSWindowRedrawContents object:self];
		FMacEvent::SendToGameRunLoop(Notification, self, EMacEventSendMethod::Sync, InGameRunLoopMode(@[ UE4NilEventMode, UE4ShowEventMode, UE4ResizeEventMode, UE4FullscreenEventMode, UE4CloseEventMode, UE4IMEEventMode ]));
	}
	bNeedsRedraw = false;
}

- (void)setWindowMode:(EWindowMode::Type)NewWindowMode
{
	WindowMode = NewWindowMode;
}

- (EWindowMode::Type)windowMode
{
	return WindowMode;
}

- (void)setDisplayReconfiguring:(bool)bIsDisplayReconfiguring
{
	bDisplayReconfiguring = bIsDisplayReconfiguring;
}

- (void)orderFrontAndMakeMain:(bool)bMain andKey:(bool)bKey
{
	if ([NSApp isHidden] == NO)
	{
		bool bBringToFront = FMacWindow::CurrentModalWindow() == nil || FMacWindow::CurrentModalWindow() == self || [self styleMask] == NSBorderlessWindowMask;
		if (bBringToFront)
		{
			[self orderFront:nil];
		}
		
		if (bMain && [self canBecomeMainWindow] && self != [NSApp mainWindow])
		{
			[self makeMainWindow];
		}
		if (bKey && [self canBecomeKeyWindow] && self != [NSApp keyWindow])
		{
			[self makeKeyWindow];
		}
	}
}

// Following few methods overload NSWindow's methods from Cocoa API, so have to use Cocoa's BOOL (signed char), not bool (unsigned int)
- (BOOL)canBecomeMainWindow
{
	bool bNoModalOrCurrent = FMacWindow::CurrentModalWindow() == nil || FMacWindow::CurrentModalWindow() == self;
	return bAcceptsInput && ([self styleMask] != NSBorderlessWindowMask) && bNoModalOrCurrent;
}

- (BOOL)canBecomeKeyWindow
{
	return bAcceptsInput && ![self ignoresMouseEvents];
}

- (BOOL)validateMenuItem:(NSMenuItem *)MenuItem
{
	// Borderless windows we use do not automatically handle first responder's actions, so we force it here
	return ([MenuItem action] == @selector(performClose:) || [MenuItem action] == @selector(performMiniaturize:) || [MenuItem action] == @selector(performZoom:)) ? YES : [super validateMenuItem:MenuItem];
}

- (void)setAlphaValue:(CGFloat)WindowAlpha
{
	if(!bRenderInitialised)
	{
		DeferOpacity = WindowAlpha;
		bDeferOrderFront = true;
	}
	else
	{
		if([self isVisible] && WindowAlpha > 0.0f)
		{
			[self performDeferredSetFrame];
		}
		[super setAlphaValue:WindowAlpha];
	}
}

- (void)orderOut:(id)Sender
{
	bDeferOrderFront = false;
	
	[super orderOut:Sender];
}

- (void)performClose:(id)Sender
{
	bDeferOrderFront = false;
	
	[self close];
}

- (void)performMiniaturize:(id)Sender
{
	[self miniaturize: self];
}

- (void)performZoom:(id)Sender
{
	bZoomed = !bZoomed;
	[self zoom: self];
}

- (void)setFrame:(NSRect)FrameRect display:(BOOL)Flag
{
	NSSize Size = [self frame].size;
	NSSize NewSize = FrameRect.size;
	if(!bRenderInitialised || ([self isVisible] && [super alphaValue] > 0.0f && (Size.width > 1 || Size.height > 1 || NewSize.width > 1 || NewSize.height > 1)))
	{
		[super setFrame:FrameRect display:Flag];
		bDeferSetFrame = false;
	}
	else
	{
		bDeferSetFrame = true;
		DeferFrame = FrameRect;
		if(self.bForwardEvents)
		{
			NSNotification* Notification = [NSNotification notificationWithName:NSWindowDidResizeNotification object:self];
			FMacEvent::SendToGameRunLoop(Notification, self, EMacEventSendMethod::Async, InGameRunLoopMode(@[ UE4ResizeEventMode, UE4ShowEventMode ]));
		}
	}
}

- (void)setFrameOrigin:(NSPoint)Point
{
	NSSize Size = [self frame].size;
	if(!bRenderInitialised || ([self isVisible] && [super alphaValue] > 0.0f && (Size.width > 1 || Size.height > 1)))
	{
		MainThreadCall(^{
			[super setFrameOrigin:Point];
		});
		bDeferSetOrigin = false;
	}
	else
	{
		bDeferSetOrigin = true;
		DeferFrame.origin = Point;
		NSNotification* Notification = [NSNotification notificationWithName:NSWindowDidMoveNotification object:self];
		FMacEvent::SendToGameRunLoop(Notification, self, EMacEventSendMethod::Async, InGameRunLoopMode(@[ UE4ResizeEventMode, UE4ShowEventMode ]));
	}
}

- (void)keyDown:(NSEvent *)Event
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (void)keyUp:(NSEvent *)Event
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
	// Handle clicking on the titlebar fullscreen item
	if(self.TargetWindowMode == EWindowMode::Windowed)
	{
		// @todo: Fix fullscreen mode mouse coordinate handling - for now default to windowed fullscreen
		self.TargetWindowMode = EWindowMode::WindowedFullscreen;
	}
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
	WindowMode = self.TargetWindowMode;
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(notification, self, EMacEventSendMethod::Async, InGameRunLoopMode(@[ UE4FullscreenEventMode ]));
	}
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
	WindowMode = EWindowMode::Windowed;
	self.TargetWindowMode = EWindowMode::Windowed;
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(notification, self, EMacEventSendMethod::Async, InGameRunLoopMode(@[ UE4FullscreenEventMode ]));
	}
}

- (void)windowDidBecomeKey:(NSNotification *)Notification
{
	if([NSApp isHidden] == NO)
	{
		if(FMacWindow::CurrentModalWindow() == nil || FMacWindow::CurrentModalWindow() == self || [self styleMask] == NSBorderlessWindowMask)
		{
			[self orderFrontAndMakeMain:false andKey:false];
		}
		else
		{
			[FMacWindow::CurrentModalWindow() orderFrontAndMakeMain:true andKey:true];
		}
	}
	
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Notification, self, EMacEventSendMethod::Async, InGameRunLoopMode(@[ UE4ShowEventMode, UE4CloseEventMode, UE4FullscreenEventMode ]));
	}
}

- (void)windowDidResignKey:(NSNotification *)Notification
{
	[self setMovable: YES];
	[self setMovableByWindowBackground: NO];
	
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Notification, self, EMacEventSendMethod::Async, InGameRunLoopMode(@[ UE4ShowEventMode, UE4CloseEventMode, UE4FullscreenEventMode ]));
	}
}

- (void)windowWillMove:(NSNotification *)Notification
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Notification, self, EMacEventSendMethod::Async, InGameRunLoopMode(@[ UE4ResizeEventMode, UE4ShowEventMode, UE4FullscreenEventMode ]));
	}
}

- (void)windowDidMove:(NSNotification *)Notification
{
	bZoomed = [self isZoomed];
	
	NSView* OpenGLView = [self openGLView];
	[[NSNotificationCenter defaultCenter] postNotificationName:NSViewGlobalFrameDidChangeNotification object:OpenGLView];
	
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Notification, self, EMacEventSendMethod::Async, InGameRunLoopMode(@[ UE4ResizeEventMode, UE4ShowEventMode, UE4FullscreenEventMode ]));
	}
}

- (void)windowDidChangeScreen:(NSNotification *)notification
{
	// You'd think this would be a good place to handle un/parenting...
	// However, do that here and your dragged window will disappear when you drag it onto another screen!
	// The windowdidChangeScreen notification only comes after you finish dragging.
	// It does however, work fine for handling display arrangement changes that cause a window to go offscreen.
	if(bDisplayReconfiguring)
	{
		NSScreen* Screen = [self screen];
		NSRect Frame = [self frame];
		NSRect VisibleFrame = [Screen visibleFrame];
		if(NSContainsRect(VisibleFrame, Frame) == NO)
		{
			// May need to scale the window to fit if it is larger than the new display.
			if (Frame.size.width > VisibleFrame.size.width || Frame.size.height > VisibleFrame.size.height)
			{
				NSRect NewFrame;
				NewFrame.size.width = Frame.size.width > VisibleFrame.size.width ? VisibleFrame.size.width : Frame.size.width;
				NewFrame.size.height = Frame.size.height > VisibleFrame.size.height ? VisibleFrame.size.height : Frame.size.height;
				NewFrame.origin = VisibleFrame.origin;
				
				[self setFrame:NewFrame display:NO];
			}
			else
			{
				NSRect Intersection = NSIntersectionRect(VisibleFrame, Frame);
				NSPoint Origin = Frame.origin;
				
				// If there's at least something on screen, try shifting it entirely on screen.
				if(Intersection.size.width > 0 && Intersection.size.height > 0)
				{
					CGFloat X = Frame.size.width - Intersection.size.width;
					CGFloat Y = Frame.size.height - Intersection.size.height;
					
					if(Intersection.size.width+Intersection.origin.x >= VisibleFrame.size.width+VisibleFrame.origin.x)
					{
						Origin.x -= X;
					}
					else if(Origin.x < VisibleFrame.origin.x)
					{
						Origin.x += X;
					}
					
					if(Intersection.size.height+Intersection.origin.y >= VisibleFrame.size.height+VisibleFrame.origin.y)
					{
						Origin.y -= Y;
					}
					else if(Origin.y < VisibleFrame.origin.y)
					{
						Origin.y += Y;
					}
				}
				else
				{
					Origin = VisibleFrame.origin;
				}
				
				[self setFrameOrigin:Origin];
			}
		}
	}
}

- (void)windowDidResize:(NSNotification *)Notification
{
	bZoomed = [self isZoomed];
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Notification, self, EMacEventSendMethod::Async, InGameRunLoopMode(@[ UE4ResizeEventMode, UE4ShowEventMode, UE4FullscreenEventMode ]));
	}
	bNeedsRedraw = true;
}

- (void)windowWillClose:(NSNotification *)notification
{
	if(self.bForwardEvents && MacApplication)
	{
		FMacEvent::SendToGameRunLoop(notification, self, EMacEventSendMethod::Async, InGameRunLoopMode(@[ UE4CloseEventMode ]));
	}
	self.bForwardEvents = false;
	[self setDelegate:nil];
}

- (void)mouseDown:(NSEvent*)Event
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (void)rightMouseDown:(NSEvent*)Event
{
	// Really we shouldn't be doing this - on OS X only left-click changes focus,
	// but for the moment it is easier than changing Slate.
	if([self canBecomeKeyWindow])
	{
		[self makeKeyWindow];
	}
	
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (void)otherMouseDown:(NSEvent*)Event
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (void)mouseUp:(NSEvent*)Event
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (void)rightMouseUp:(NSEvent*)Event
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (void)otherMouseUp:(NSEvent*)Event
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (NSDragOperation)draggingEntered:(id < NSDraggingInfo >)Sender
{
	return NSDragOperationGeneric;
}

- (void)draggingExited:(id < NSDraggingInfo >)Sender
{
	if(self.bForwardEvents)
	{
		NSNotification* Notification = [NSNotification notificationWithName:NSDraggingExited object:Sender];
		FMacEvent::SendToGameRunLoop(Notification, self, EMacEventSendMethod::Async);
	}
}

- (NSDragOperation)draggingUpdated:(id < NSDraggingInfo >)Sender
{
	if(self.bForwardEvents)
	{
		NSNotification* Notification = [NSNotification notificationWithName:NSDraggingUpdated object:Sender];
		FMacEvent::SendToGameRunLoop(Notification, self, EMacEventSendMethod::Async);
	}
	return NSDragOperationGeneric;
}

- (BOOL)prepareForDragOperation:(id < NSDraggingInfo >)Sender
{
	if(self.bForwardEvents)
	{
		NSNotification* Notification = [NSNotification notificationWithName:NSPrepareForDragOperation object:Sender];
		FMacEvent::SendToGameRunLoop(Notification, self, EMacEventSendMethod::Async);
	}
	return YES;
}

- (BOOL)performDragOperation:(id < NSDraggingInfo >)Sender
{
	if(self.bForwardEvents)
	{
		NSNotification* Notification = [NSNotification notificationWithName:NSPerformDragOperation object:Sender];
		FMacEvent::SendToGameRunLoop(Notification, self, EMacEventSendMethod::Async);
	}
	return YES;
}

- (BOOL)isMovable
{
	BOOL Movable = [super isMovable];
	if(Movable && bRenderInitialised && MacApplication)
	{
		Movable &= (BOOL)(GameThreadReturn(^{ return MacApplication->IsWindowMovable(self, NULL); }, InGameRunLoopMode(@[ UE4NilEventMode, UE4ShowEventMode, UE4ResizeEventMode, UE4FullscreenEventMode, UE4CloseEventMode, UE4IMEEventMode ])));
	}
	return Movable;
}

@end

/**
 * Custom window class used for mouse capture
 */
@implementation FMouseCaptureWindow

- (id)initWithTargetWindow: (FCocoaWindow*)Window
{
	self = [super initWithContentRect: [[Window screen] frame] styleMask: NSBorderlessWindowMask backing: NSBackingStoreBuffered defer: NO];
	[self setBackgroundColor: [NSColor clearColor]];
	[self setOpaque: NO];
	[self setLevel: NSMainMenuWindowLevel + 1];
	[self setIgnoresMouseEvents: NO];
	[self setAcceptsMouseMovedEvents: YES];
	[self setHidesOnDeactivate: YES];
	
	TargetWindow = Window;
	
	return self;
}

- (FCocoaWindow*)targetWindow
{
	return TargetWindow;
}

- (void)setTargetWindow: (FCocoaWindow*)Window
{
	TargetWindow = Window;
}

- (void)mouseDown:(NSEvent*)Event
{
	FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
}

- (void)rightMouseDown:(NSEvent*)Event
{
	FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
}

- (void)otherMouseDown:(NSEvent*)Event
{
	FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
}

- (void)mouseUp:(NSEvent*)Event
{
	FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
}

- (void)rightMouseUp:(NSEvent*)Event
{
	FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
}

- (void)otherMouseUp:(NSEvent*)Event
{
	FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
}

@end