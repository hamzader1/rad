// typelat: key-to-screen latency of whichever app has keyboard focus (any
// editor: Raid, Zed, ...). Types `x`, then Backspace, alternately; posts each
// key as a real HID event and watches a small screen region around the text
// cursor (ScreenCaptureKit) until its pixels change. Latency = the display
// time of the first changed frame - the time the key was posted. It does not
// include the monitor's own processing delay (same for every app), so use it
// to compare apps on the same monitor.
//
// Build: clang -fobjc-arc -O2 tests/perf/typelat.m -o build/typelat \
//          -framework Foundation -framework AppKit -framework CoreMedia \
//          -framework CoreVideo -framework ScreenCaptureKit
// Use:   build/typelat [-n samples] [-d start_delay_s] [-w region_w] [-h region_h]
//          [-at x,y]   (watch a w x h box there, global points; default: the
//                       focused app's whole window)
//          [-pid pid]  (activate that app, watch its window, and post the keys
//                       to it only, not to whatever has focus: for unattended
//                       runs on an app that starts with its text cursor placed,
//                       e.g. raid --user:<scratch file> <file>)
//   1. Run it, then within the start delay (5 s) click into the editor's
//      text (an empty line of an open file) and take your hands off. It
//      watches the focused app's whole window (-at x,y: a box instead).
//   2. Do not touch anything until it prints the results.
// Needs (System Settings > Privacy & Security, for the terminal app):
// Accessibility (to post keys) and Screen Recording (to read pixels).
// Turn off cursor blinking in the editor (a blink looks like a change).

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <CoreMedia/CoreMedia.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <mach/mach_time.h>
#include <stdatomic.h>

static double g_ticks_to_ms;
static double ms_from_ticks(uint64_t t) { return (double)t * g_ticks_to_ms; }

//- shared state (stream queue only, except where noted)
static uint8_t *g_latest;          // last frame's pixels
static uint8_t *g_baseline;        // the frame before the key
static uint8_t *g_prev;            // the frame before the last one
static size_t g_frame_bytes;
static size_t g_w, g_h;            // pixels
static int g_have_frame;
static int g_measuring;
static uint64_t g_key_time;
static int g_key_index;
static int g_samples_wanted = 60;
static int g_warmup = 6;
static double *g_results;
static int g_results_count;
static int g_timeouts;
static int g_false_changes;
static int g_frames_since_key;      // frames received while measuring
static size_t g_max_changed;       // most pixels changed vs the baseline while measuring
static int g_saved_debug;        // changes seen right before a key (blink?)
static dispatch_queue_t g_queue;
static uint64_t g_last_frame_time;
static double g_frame_interval_sum;
static int g_frame_interval_count;

static pid_t g_pid; // -pid: keys go to this process only

static void save_png(const uint8_t *bgra, const char *path);
static size_t changed_pixels(const uint8_t *a, const uint8_t *b);

static void post_key(void)
{
  CGKeyCode code = (g_key_index % 2 == 0) ? 7 /* x */ : 51 /* backspace */;
  CGEventRef down = CGEventCreateKeyboardEvent(NULL, code, true);
  CGEventRef up = CGEventCreateKeyboardEvent(NULL, code, false);
  g_key_time = mach_absolute_time();
  if(g_pid != 0)
  {
    CGEventPostToPid(g_pid, down);
    CGEventPostToPid(g_pid, up);
  }
  else
  {
    CGEventPost(kCGHIDEventTap, down);
    CGEventPost(kCGHIDEventTap, up);
  }
  CFRelease(down);
  CFRelease(up);
}

static void finish(void)
{
  int n = g_results_count;
  double *v = g_results;
  for(int i = 0; i < n; i++) for(int j = i+1; j < n; j++) if(v[j] < v[i]) { double t = v[i]; v[i] = v[j]; v[j] = t; }
  double sum = 0; for(int i = 0; i < n; i++) sum += v[i];
  printf("\nsamples %d (timeouts %d, warmup %d dropped)\n", n, g_timeouts, g_warmup);
  if(n > 0)
  {
    printf("key -> screen:  avg %.1f ms   median %.1f ms   p95 %.1f ms   min %.1f ms   max %.1f ms\n",
           sum / n, v[n/2], v[(int)((n-1)*0.95)], v[0], v[n-1]);
  }
  if(g_frame_interval_count > 0)
  {
    printf("screen updates seen every ~%.1f ms (the display's refresh, when the app draws continuously)\n",
           g_frame_interval_sum / g_frame_interval_count);
  }
  if(g_false_changes > 2)
  {
    printf("warning: the region changed %d times with no key pending (cursor blink? animation?): results may be low\n", g_false_changes);
  }
  exit(0);
}

static void schedule_next_key(void)
{
  // random gap 180-320 ms: keys land at random points of the refresh cycle
  double gap_ms = 180.0 + (double)arc4random_uniform(140);
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(gap_ms * 1e6)), g_queue, ^{
    if(!g_have_frame) { schedule_next_key(); return; }
    memcpy(g_baseline, g_latest, g_frame_bytes);
    g_measuring = 1;
    g_frames_since_key = 0;
    g_max_changed = 0;
    int index = g_key_index;
    post_key();
    // timeout
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(600 * 1e6)), g_queue, ^{
      if(g_measuring && g_key_index == index)
      {
        g_measuring = 0;
        g_timeouts += 1;
        printf("  key %d: no change within 600 ms (%d frames came, at most %zu pixels changed; region %zux%zu px)\n",
               index, g_frames_since_key, g_max_changed, g_w, g_h);
        if(g_saved_debug < 2)
        {
          char path[256];
          snprintf(path, sizeof(path), "/tmp/typelat_key%d_before.png", index); save_png(g_baseline, path);
          snprintf(path, sizeof(path), "/tmp/typelat_key%d_after.png", index); save_png(g_latest, path);
          printf("    saved /tmp/typelat_key%d_before.png and _after.png (what the region looked like)\n", index);
          g_saved_debug += 1;
        }
        g_key_index += 1;
        if(g_timeouts > 10) { printf("too many timeouts: is the editor focused, the mouse on the text cursor, and are permissions granted?\n"); finish(); }
        schedule_next_key();
      }
    });
  });
}

static size_t changed_pixels(const uint8_t *a, const uint8_t *b)
{
  size_t changed = 0;
  for(size_t i = 0; i < g_frame_bytes; i += 4)
  {
    int d = abs((int)a[i] - (int)b[i]) + abs((int)a[i+1] - (int)b[i+1]) + abs((int)a[i+2] - (int)b[i+2]);
    if(d > 24) { changed += 1; }
  }
  return changed;
}

static void save_png(const uint8_t *bgra, const char *path)
{
  CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
  CGContextRef ctx = CGBitmapContextCreate((void *)bgra, g_w, g_h, 8, g_w*4, cs, kCGImageAlphaPremultipliedFirst|kCGBitmapByteOrder32Little);
  CGImageRef img = CGBitmapContextCreateImage(ctx);
  NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:img];
  [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:[NSString stringWithUTF8String:path] atomically:YES];
  CGImageRelease(img); CGContextRelease(ctx); CGColorSpaceRelease(cs);
}

static int frames_differ(const uint8_t *a, const uint8_t *b)
{
  // a few pixels' worth of difference (ignore tiny noise)
  size_t changed = 0;
  for(size_t i = 0; i < g_frame_bytes; i += 4)
  {
    int d = abs((int)a[i] - (int)b[i]) + abs((int)a[i+1] - (int)b[i+1]) + abs((int)a[i+2] - (int)b[i+2]);
    if(d > 24) { changed += 1; if(changed >= 4) { return 1; } }
  }
  return 0;
}

@interface Output : NSObject <SCStreamOutput, SCStreamDelegate>
@end

@implementation Output
- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sb ofType:(SCStreamOutputType)type
{
  if(type != SCStreamOutputTypeScreen) { return; }
  CFArrayRef atts = CMSampleBufferGetSampleAttachmentsArray(sb, false);
  if(atts == NULL || CFArrayGetCount(atts) == 0) { return; }
  NSDictionary *info = (__bridge NSDictionary *)CFArrayGetValueAtIndex(atts, 0);
  NSNumber *status = info[SCStreamFrameInfoStatus];
  if(status == nil || status.integerValue != SCFrameStatusComplete) { return; }
  NSNumber *display_time = info[SCStreamFrameInfoDisplayTime];
  uint64_t t = display_time ? display_time.unsignedLongLongValue : mach_absolute_time();
  CVImageBufferRef img = CMSampleBufferGetImageBuffer(sb);
  if(img == NULL) { return; }
  CVPixelBufferLockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
  size_t w = CVPixelBufferGetWidth(img), h = CVPixelBufferGetHeight(img);
  size_t stride = CVPixelBufferGetBytesPerRow(img);
  uint8_t *base = CVPixelBufferGetBaseAddress(img);
  if(g_latest == NULL)
  {
    g_w = w; g_h = h; g_frame_bytes = w*h*4;
    g_latest = malloc(g_frame_bytes);
    g_baseline = malloc(g_frame_bytes);
    g_prev = malloc(g_frame_bytes);
  }
  if(w == g_w && h == g_h)
  {
    memcpy(g_prev, g_latest, g_frame_bytes);
    for(size_t y = 0; y < h; y++) { memcpy(g_latest + y*w*4, base + y*stride, w*4); }
  }
  CVPixelBufferUnlockBaseAddress(img, kCVPixelBufferLock_ReadOnly);
  if(g_last_frame_time != 0)
  {
    double dt = ms_from_ticks(t - g_last_frame_time);
    if(dt > 2 && dt < 40) { g_frame_interval_sum += dt; g_frame_interval_count += 1; }
  }
  g_last_frame_time = t;
  if(!g_have_frame)
  {
    g_have_frame = 1;
    return;
  }
  if(!g_measuring)
  {
    // a change long after the last key, with none pending: blink/animation
    if(ms_from_ticks(t - g_key_time) > 400 && frames_differ(g_latest, g_prev)) { g_false_changes += 1; }
    return;
  }
  g_frames_since_key += 1;
  { size_t c = changed_pixels(g_latest, g_baseline); if(c > g_max_changed) { g_max_changed = c; } }
  if(t >= g_key_time && frames_differ(g_latest, g_baseline))
  {
    g_measuring = 0;
    double ms = ms_from_ticks(t - g_key_time);
    if(g_key_index >= g_warmup)
    {
      g_results[g_results_count++] = ms;
      printf("  key %d (%s): %.1f ms\n", g_key_index, (g_key_index % 2 == 0) ? "x" : "backspace", ms);
    }
    g_key_index += 1;
    if(g_results_count >= g_samples_wanted) { finish(); }
    schedule_next_key();
  }
}
- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error
{
  printf("capture stopped: %s\n", error.localizedDescription.UTF8String);
  exit(1);
}
@end

int main(int argc, char **argv)
{
  @autoreleasepool
  {
    mach_timebase_info_data_t tb; mach_timebase_info(&tb);
    g_ticks_to_ms = (double)tb.numer / (double)tb.denom / 1e6;
    double delay_s = 5, region_w = 400, region_h = 100;
    int have_at = 0; double at_x = 0, at_y = 0;
    for(int i = 1; i < argc; i++)
    {
      if(!strcmp(argv[i], "-n") && i+1 < argc) { g_samples_wanted = atoi(argv[++i]); }
      else if(!strcmp(argv[i], "-d") && i+1 < argc) { delay_s = atof(argv[++i]); }
      else if(!strcmp(argv[i], "-w") && i+1 < argc) { region_w = atof(argv[++i]); }
      else if(!strcmp(argv[i], "-h") && i+1 < argc) { region_h = atof(argv[++i]); }
      else if(!strcmp(argv[i], "-at") && i+1 < argc) { have_at = (sscanf(argv[++i], "%lf,%lf", &at_x, &at_y) == 2); }
      else if(!strcmp(argv[i], "-pid") && i+1 < argc) { g_pid = (pid_t)atoi(argv[++i]); }
      else { printf("usage: typelat [-n samples] [-d start_delay_s] [-w w] [-h h] [-at x,y] [-pid pid]\n"); return 2; }
    }
    g_results = calloc((size_t)g_samples_wanted + 1, sizeof(double));
    if(!AXIsProcessTrusted())
    {
      // rad: without it macOS drops the posted keys silently
      printf("STOP: this terminal app has no Accessibility permission, so macOS blocks the keys it sends.\n"
             "  System Settings > Privacy & Security > Accessibility: turn on your terminal app\n"
             "  (Terminal, iTerm, ...), then quit it completely (Cmd+Q), reopen it and run this again.\n");
      NSDictionary *opts = @{(__bridge NSString *)kAXTrustedCheckOptionPrompt: @YES};
      AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)opts);
      return 1;
    }
    printf("permissions: keys OK\n");
    if(!CGPreflightScreenCaptureAccess())
    {
      printf("note: no Screen Recording permission yet (System Settings > Privacy & Security > Screen Recording); asking...\n");
      CGRequestScreenCaptureAccess();
    }
    NSRunningApplication *pid_app = nil;
    if(g_pid != 0)
    {
      pid_app = [NSRunningApplication runningApplicationWithProcessIdentifier:g_pid];
      if(pid_app == nil) { printf("no process %d\n", (int)g_pid); return 1; }
      [pid_app activateWithOptions:0];
      printf("activated %s; starting in %.0f s\n", pid_app.localizedName.UTF8String, delay_s);
      fflush(stdout);
      [NSThread sleepForTimeInterval:delay_s];
    }
    else if(!have_at)
    {
      printf("click into the editor's text (an empty line in a file with text), then hands off: starting in %.0f s\n", delay_s);
      fflush(stdout);
      [NSThread sleepForTimeInterval:delay_s];
    }
    // region: global points, top-left origin (CG). Default: the focused app's
    // largest window (any visible reaction to the key counts: the character,
    // the cursor, a status bar); -at x,y: a w x h box there instead
    CGRect global = CGRectZero;
    NSRunningApplication *front_app = (pid_app != nil ? pid_app : NSWorkspace.sharedWorkspace.frontmostApplication);
    if(have_at)
    {
      global = CGRectMake(at_x - region_w/2, at_y - region_h/2, region_w, region_h);
    }
    else
    {
      CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly|kCGWindowListExcludeDesktopElements, kCGNullWindowID);
      double best_area = 0;
      for(CFIndex i = 0; windows && i < CFArrayGetCount(windows); i++)
      {
        NSDictionary *w = (__bridge NSDictionary *)CFArrayGetValueAtIndex(windows, i);
        if([w[(id)kCGWindowOwnerPID] intValue] != front_app.processIdentifier || [w[(id)kCGWindowLayer] intValue] != 0) { continue; }
        CGRect b; CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)w[(id)kCGWindowBounds], &b);
        if(b.size.width*b.size.height > best_area) { best_area = b.size.width*b.size.height; global = b; }
      }
      if(windows) { CFRelease(windows); }
      if(best_area == 0) { printf("no window of %s found\n", front_app.localizedName.UTF8String); return 1; }
    }
    CGPoint center = CGPointMake(CGRectGetMidX(global), CGRectGetMidY(global));
    CGDirectDisplayID display_id = 0; uint32_t count = 0;
    CGGetDisplaysWithPoint(center, 1, &display_id, &count);
    if(count == 0) { printf("no display under (%.0f, %.0f)\n", center.x, center.y); return 1; }
    CGRect bounds = CGDisplayBounds(display_id);
    CGRect region = CGRectIntersection(global, bounds);
    region.origin.x -= bounds.origin.x;
    region.origin.y -= bounds.origin.y;
    region_w = region.size.width;
    region_h = region.size.height;
    double scale = (double)CGDisplayPixelsWide(display_id) / bounds.size.width;
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display_id);
    double hz = mode ? CGDisplayModeGetRefreshRate(mode) : 0;
    if(mode) { CGDisplayModeRelease(mode); }
    printf("watching %.0fx%.0f pt (%s window) on display %u (refresh %.0f Hz as reported)\n", region_w, region_h, have_at ? "box in the" : "the whole", display_id, hz);
    printf("typing into: %s\n", front_app.localizedName.UTF8String);
    fflush(stdout);

    g_queue = dispatch_queue_create("typelat", DISPATCH_QUEUE_SERIAL);
    Output *output = [Output new];
    [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
      if(error != nil) { printf("screen capture not allowed: %s\n", error.localizedDescription.UTF8String); exit(1); }
      SCDisplay *display = nil;
      for(SCDisplay *d in content.displays) { if(d.displayID == display_id) { display = d; } }
      if(display == nil) { printf("display not found\n"); exit(1); }
      SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:display excludingWindows:@[]];
      SCStreamConfiguration *config = [SCStreamConfiguration new];
      config.sourceRect = region;
      config.width = (size_t)(region_w * scale);
      config.height = (size_t)(region_h * scale);
      config.pixelFormat = kCVPixelFormatType_32BGRA;
      config.minimumFrameInterval = CMTimeMake(1, 240);
      config.showsCursor = NO;
      config.queueDepth = 4;
      SCStream *stream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:output];
      NSError *add_error = nil;
      [stream addStreamOutput:output type:SCStreamOutputTypeScreen sampleHandlerQueue:g_queue error:&add_error];
      if(add_error != nil) { printf("capture: %s\n", add_error.localizedDescription.UTF8String); exit(1); }
      [stream startCaptureWithCompletionHandler:^(NSError *start_error) {
        if(start_error != nil) { printf("capture failed to start: %s\n", start_error.localizedDescription.UTF8String); exit(1); }
        printf("measuring %d keys (+%d warmup)...\n", g_samples_wanted, g_warmup);
        fflush(stdout);
        dispatch_async(g_queue, ^{ schedule_next_key(); });
      }];
      (void)CFBridgingRetain(stream);
    }];
    dispatch_main();
  }
  return 0;
}
