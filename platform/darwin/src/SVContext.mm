//
//  SVContext.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "SVContext+Internal.h"

#include <TargetConditionals.h>
#include <dlfcn.h>
#include <string.h>

#if TARGET_OS_OSX
#include <OpenGL/OpenGL.h>
#else
#import <OpenGLES/EAGL.h>
#endif

NSErrorDomain const SVErrorDomain = @"SVErrorDomain";

namespace
{
    constexpr const char* kRendererUnavailablePrefix = "renderer unavailable";

#if TARGET_OS_OSX
    constexpr const char* kGlFrameworkPath = "/System/Library/Frameworks/OpenGL.framework/OpenGL";
#else
    constexpr const char* kGlFrameworkPath = "/System/Library/Frameworks/OpenGLES.framework/OpenGLES";
#endif

    void* SVGetGlProcAddress(const char* name)
    {
        static void* frameworkHandle = dlopen(kGlFrameworkPath, RTLD_LAZY | RTLD_GLOBAL);
        void* symbol = frameworkHandle != nullptr ? dlsym(frameworkHandle, name) : nullptr;

        if (symbol == nullptr)
            symbol = dlsym(RTLD_DEFAULT, name);

        return symbol;
    }

    bool SVHasCurrentGlContext()
    {
#if TARGET_OS_OSX
        return CGLGetCurrentContext() != nullptr;
#else
        return [EAGLContext currentContext] != nil;
#endif
    }

    sv_status SVEnsureGlLoaded()
    {
        static NSLock* lock = [[NSLock alloc] init];
        static bool loaded = false;

        [lock lock];
        sv_status status = SV_OK;

        if (!loaded)
        {
            status = sv_init_gl(SVGetGlProcAddress);
            loaded = status == SV_OK;
        }

        [lock unlock];
        return status;
    }

    const char* SVFileSystemPath(NSURL* url)
    {
        return url != nil ? url.fileSystemRepresentation : nullptr;
    }

    NSInteger SVErrorCodeForStatus(sv_status status)
    {
        switch (status)
        {
        case SV_ERR_IO: return SVErrorIO;
        case SV_ERR_PARSE: return SVErrorParse;
        case SV_ERR_SHADER: return SVErrorShader;
        case SV_ERR_GL: return SVErrorGL;
        case SV_ERR_ARG: return SVErrorArgument;
        case SV_OK: break;
        }

        return SVErrorArgument;
    }
}

NSError* SVMakeError(NSInteger code, NSString* message)
{
    return [NSError errorWithDomain:SVErrorDomain
                               code:code
                           userInfo:@{ NSLocalizedDescriptionKey: message ?: @"" }];
}

@interface SVContext ()

@property (nonatomic, copy, readwrite) SVContextConfiguration *configuration;
@property (nonatomic, assign, readwrite, getter=isPaused) BOOL paused;

@end

@implementation SVContext
{
    sv_context* _context;
#if DEBUG
    NSThread* _renderThread;
#endif
}

- (nullable instancetype)initWithConfiguration:(SVContextConfiguration *)configuration error:(NSError **)error
{
    self = [super init];

    if (self == nil)
        return nil;

    _configuration = [configuration copy];
#if DEBUG
    _renderThread = [NSThread currentThread];
#endif

    if (!SVHasCurrentGlContext())
    {
        if (error != nullptr)
            *error = SVMakeError(SVErrorRendererUnavailable, @"renderer unavailable: no GL context is current on this thread");
        return nil;
    }

    const sv_status glStatus = SVEnsureGlLoaded();

    sv_config config;
    memset(&config, 0, sizeof(config));
    config.width = static_cast<int>(_configuration.width);
    config.height = static_cast<int>(_configuration.height);
    config.mesh_x = static_cast<int>(_configuration.meshX);
    config.mesh_y = static_cast<int>(_configuration.meshY);
    config.texsize_x = static_cast<int>(_configuration.texsizeX);
    config.texsize_y = static_cast<int>(_configuration.texsizeY);
    config.fps_hint = _configuration.fpsHint;
    config.resource_path = SVFileSystemPath(_configuration.resourceURL);
    config.texture_search_path = SVFileSystemPath(_configuration.textureSearchURL);
    config.cache_path = SVFileSystemPath(_configuration.cacheURL);
    config.rng_seed = _configuration.rngSeed;
    config.deterministic = _configuration.deterministic;
    config.pin_date = _configuration.pinDate;
    config.year = _configuration.year;
    config.month = _configuration.month;
    config.day = _configuration.day;
    config.weekday = _configuration.weekday;
    config.hour = _configuration.hour;
    config.minute = _configuration.minute;
    config.second = _configuration.second;
    config.milliseconds = _configuration.milliseconds;
    config.pin_mouse = _configuration.pinMouse;
    config.mouse_x = _configuration.mouseX;
    config.mouse_y = _configuration.mouseY;

    _context = sv_create(&config);

    if (_context == nullptr)
    {
        if (error != nullptr)
            *error = SVMakeError(SVErrorArgument, @"sv_create returned NULL");
        return nil;
    }

    const char* lastError = sv_last_error(_context);

    if (strncmp(lastError, kRendererUnavailablePrefix, strlen(kRendererUnavailablePrefix)) == 0)
    {
        NSString* message = [NSString stringWithFormat:@"%s (sv_init_gl status %d)", lastError, static_cast<int>(glStatus)];
        sv_destroy(_context);
        _context = nullptr;

        if (error != nullptr)
            *error = SVMakeError(SVErrorRendererUnavailable, message);
        return nil;
    }

    return self;
}

- (void)dealloc
{
    [self invalidate];
}

- (sv_context *)rawContext
{
    return _context;
}

- (void)assertRenderThread
{
#if DEBUG
    NSAssert([NSThread currentThread] == _renderThread, @"SVContext used off the thread it was created on");
#endif
}

- (BOOL)failWithStatus:(sv_status)status error:(NSError **)error
{
    if (status == SV_OK)
        return YES;

    if (error != nullptr)
        *error = SVMakeError(SVErrorCodeForStatus(status), self.lastErrorMessage ?: @"");

    return NO;
}

- (BOOL)loadPresetAtURL:(NSURL *)url blendSeconds:(float)blendSeconds error:(NSError **)error
{
    [self assertRenderThread];

    if (_context == nullptr || !url.isFileURL)
        return [self failWithStatus:SV_ERR_ARG error:error];

    return [self failWithStatus:sv_load_preset(_context, url.fileSystemRepresentation, blendSeconds) error:error];
}

- (BOOL)loadPresetText:(NSString *)text blendSeconds:(float)blendSeconds error:(NSError **)error
{
    [self assertRenderThread];

    if (_context == nullptr)
        return [self failWithStatus:SV_ERR_ARG error:error];

    NSData* bytes = [text dataUsingEncoding:NSUTF8StringEncoding];
    const sv_status status = sv_load_preset_memory(_context, static_cast<const char*>(bytes.bytes), bytes.length, blendSeconds);
    return [self failWithStatus:status error:error];
}

- (void)pushPCMFloat:(const float *)interleaved frames:(NSUInteger)frames channels:(NSInteger)channels
{
    if (_context != nullptr)
        sv_push_pcm_float(_context, interleaved, frames, static_cast<int>(channels));
}

- (void)pushPCMInt16:(const int16_t *)interleaved frames:(NSUInteger)frames channels:(NSInteger)channels
{
    if (_context != nullptr)
        sv_push_pcm_s16(_context, interleaved, frames, static_cast<int>(channels));
}

- (void)renderFrameWithDeltaTime:(NSTimeInterval)deltaTime targetFramebuffer:(uint32_t)targetFramebuffer
{
    [self assertRenderThread];

    if (_context != nullptr)
        sv_render_frame(_context, deltaTime, targetFramebuffer);
}

- (void)resizeToWidth:(NSInteger)width height:(NSInteger)height
{
    [self assertRenderThread];

    if (_context != nullptr)
        sv_resize(_context, static_cast<int>(width), static_cast<int>(height));
}

- (void)pause
{
    if (_context == nullptr)
        return;

    sv_pause(_context);
    self.paused = YES;
}

- (void)resume
{
    if (_context == nullptr)
        return;

    sv_resume(_context);
    self.paused = NO;
}

- (SVAudioLevels)audioLevels
{
    SVAudioLevels levels = { 0.0f, 0.0f, 0.0f };

    if (_context != nullptr)
        sv_get_audio_levels(_context, &levels.bass, &levels.mid, &levels.treble);

    return levels;
}

- (nullable NSString *)lastErrorMessage
{
    if (_context == nullptr)
        return nil;

    const char* message = sv_last_error(_context);

    if (message == nullptr || message[0] == '\0')
        return nil;

    return [NSString stringWithUTF8String:message];
}

- (BOOL)dumpFrameStateToURL:(NSURL *)url error:(NSError **)error
{
    [self assertRenderThread];

    if (_context == nullptr || !url.isFileURL)
        return [self failWithStatus:SV_ERR_ARG error:error];

    NSFileManager* fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtURL:url error:nil];
    sv_dump_frame_state(_context, url.fileSystemRepresentation);

    if ([fileManager fileExistsAtPath:url.path])
        return YES;

    if (error != nullptr)
        *error = SVMakeError(SVErrorIO, [NSString stringWithFormat:@"sv_dump_frame_state wrote nothing to %@", url.path]);

    return NO;
}

- (void)invalidate
{
    if (_context == nullptr)
        return;

    [self assertRenderThread];
    sv_destroy(_context);
    _context = nullptr;
}

@end
