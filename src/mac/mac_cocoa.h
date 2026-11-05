// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef MAC_COCOA_H
#define MAC_COCOA_H

// rad: one place that pulls in Cocoa for the macOS backends (window_manager,
// shell, render). The `raid` unity build is compiled as Objective-C on macOS.
// base_core.h's keyword macros collide with names inside Apple's headers, so
// they are undefined around the include and restored after.

#if !defined(__OBJC__)
# error The macOS UI backends must be compiled as Objective-C (-x objective-c).
#endif

#undef internal
#undef global
#undef local_persist
#define FileInfo MAC_CarbonFileInfo // CarbonCore's Finder.h has its own FileInfo
#import <Cocoa/Cocoa.h>
#undef FileInfo
#define internal      static
#define global        static
#define local_persist static

// rad: no ARC (Objective-C objects live in C structs here); release by hand.

#endif // MAC_COCOA_H
