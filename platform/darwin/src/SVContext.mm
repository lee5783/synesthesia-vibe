//
//  SVContext.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "SVContext+Internal.h"
#import "SVAudioInput+Internal.h"
#import "SVPreparedPreset+Internal.h"

#include <TargetConditionals.h>
#include <dlfcn.h>
#include <math.h>
#include <pthread.h>
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
    constexpr size_t kValidateErrorBytes = 1024;
    constexpr float kDefaultPlaybackRate = 1.0f;
    constexpr float kMinimumPlaybackRate = 0.125f;
    constexpr float kMaximumPlaybackRate = 8.0f;
    constexpr float kDefaultDebugInterval = 1.0f;

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

    NSString* SVDebugString(const char* text)
    {
        return text != nullptr ? ([NSString stringWithUTF8String:text] ?: @"") : @"";
    }

    NSDictionary<NSString*, id>* SVDebugInfoDictionary(const sv_debug_info* info)
    {
        return @{
            @"time": @(info->time),
            @"frame": @(info->frame),
            @"display_fps": @(info->display_fps),
            @"sim_steps_per_frame": @(info->sim_steps_per_frame),
            @"playback_rate": @(info->playback_rate),
            @"preset_version": @(info->preset_version),
            @"warp_shader": @(info->warp_shader),
            @"comp_shader": @(info->comp_shader),
            @"warp_ps_version": @(info->warp_ps_version),
            @"comp_ps_version": @(info->comp_ps_version),
            @"bass": @(info->bass),
            @"mid": @(info->mid),
            @"treb": @(info->treb),
            @"bass_att": @(info->bass_att),
            @"mid_att": @(info->mid_att),
            @"treb_att": @(info->treb_att),
            @"texsize_x": @(info->texsize_x),
            @"texsize_y": @(info->texsize_y),
            @"width": @(info->width),
            @"height": @(info->height),
            @"feedback_format": @(info->feedback_format),
            @"gl_errors": @(info->gl_errors),
            @"limiter_active": @(info->limiter_active),
            @"preset_name": SVDebugString(info->preset_name),
            @"last_error": SVDebugString(info->last_error),
            @"fallback_reason": SVDebugString(info->fallback_reason)
        };
    }

    BOOL SVValidationResult(sv_status status, const char* message, NSError** error)
    {
        if (status == SV_OK)
            return YES;

        if (error != nullptr)
            *error = SVMakeError(SVErrorCodeForStatus(status), [NSString stringWithUTF8String:message] ?: @"");

        return NO;
    }
}

NSError* SVMakeError(NSInteger code, NSString* message)
{
    return [NSError errorWithDomain:SVErrorDomain
                               code:code
                           userInfo:@{ NSLocalizedDescriptionKey: message ?: @"" }];
}

float SVNormalizedPlaybackRate(float rate)
{
    return isfinite(rate) ? fminf(fmaxf(rate, kMinimumPlaybackRate), kMaximumPlaybackRate) : kDefaultPlaybackRate;
}

@interface SVContext ()

@property (nonatomic, copy, readwrite) SVContextConfiguration *configuration;
@property (nonatomic, assign, readwrite, getter=isPaused) BOOL paused;

- (void)storeDebugInfo:(const sv_debug_info *)info;
- (void)applyDebugCallback;

@end

namespace
{
    void SVDebugInfoTrampoline(const sv_debug_info* info, void* user)
    {
        [(__bridge SVContext*)user storeDebugInfo:info];
    }
}

@implementation SVContext
{
    sv_context* _context;
    pthread_rwlock_t _prepareLock;
    float _playbackRate;
    float _debugInterval;
    BOOL _debugEnabled;
    void (^_debugInfoHandler)(NSDictionary<NSString*, id>*);
    NSDictionary<NSString*, id>* _pendingDebugInfo;
#if DEBUG
    NSThread* _renderThread;
#endif
}

- (nullable instancetype)initWithConfiguration:(SVContextConfiguration *)configuration error:(NSError **)error
{
    self = [super init];

    if (self == nil)
        return nil;

    pthread_rwlock_init(&_prepareLock, nullptr);
    _configuration = [configuration copy];
    _playbackRate = kDefaultPlaybackRate;
    _debugInterval = kDefaultDebugInterval;
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
    config.max_fps = _configuration.maxFps;
    config.max_luminance = _configuration.maxLuminance;
    config.flash_limit = _configuration.flashLimit;

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

    SVRegisterAudioSinkContext(self);
    return self;
}

+ (BOOL)validatePresetAtURL:(NSURL *)url error:(NSError **)error
{
    char message[kValidateErrorBytes] = {};

    if (!url.isFileURL)
        return SVValidationResult(SV_ERR_ARG, "sv_validate_preset: path is empty", error);

    return SVValidationResult(sv_validate_preset(url.fileSystemRepresentation, message, sizeof(message)), message, error);
}

+ (BOOL)validatePresetText:(NSString *)text error:(NSError **)error
{
    char message[kValidateErrorBytes] = {};
    NSData* bytes = [text dataUsingEncoding:NSUTF8StringEncoding] ?: [NSData data];
    const char* utf8 = bytes.length > 0 ? static_cast<const char*>(bytes.bytes) : "";
    return SVValidationResult(sv_validate_preset_memory(utf8, bytes.length, message, sizeof(message)), message, error);
}

- (void)dealloc
{
    [self invalidate];
    pthread_rwlock_destroy(&_prepareLock);
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

- (nullable SVPreparedPreset *)preparePresetAtURL:(NSURL *)url error:(NSError **)error
{
    char message[kValidateErrorBytes] = {};
    sv_prepared_preset* prepared = nullptr;
    sv_status status = SV_ERR_ARG;
    const char* failure = "sv_prepare_preset: context destroyed or path is not a file URL";

    pthread_rwlock_rdlock(&_prepareLock);

    if (_context != nullptr && url.isFileURL)
    {
        status = sv_prepare_preset(_context, url.fileSystemRepresentation, &prepared, message, sizeof(message));
        failure = message;
    }

    pthread_rwlock_unlock(&_prepareLock);

    if (!SVValidationResult(status, failure, error))
        return nil;

    return [[SVPreparedPreset alloc] initWithRawPrepared:prepared];
}

- (nullable SVPreparedPreset *)preparePresetText:(NSString *)text error:(NSError **)error
{
    char message[kValidateErrorBytes] = {};
    sv_prepared_preset* prepared = nullptr;
    sv_status status = SV_ERR_ARG;
    const char* failure = "sv_prepare_preset_memory: context destroyed";
    NSData* bytes = [text dataUsingEncoding:NSUTF8StringEncoding] ?: [NSData data];
    const char* utf8 = bytes.length > 0 ? static_cast<const char*>(bytes.bytes) : "";

    pthread_rwlock_rdlock(&_prepareLock);

    if (_context != nullptr)
    {
        status = sv_prepare_preset_memory(_context, utf8, bytes.length, &prepared, message, sizeof(message));
        failure = message;
    }

    pthread_rwlock_unlock(&_prepareLock);

    if (!SVValidationResult(status, failure, error))
        return nil;

    return [[SVPreparedPreset alloc] initWithRawPrepared:prepared];
}

- (BOOL)commitPreparedPreset:(SVPreparedPreset *)prepared blendSeconds:(float)blendSeconds error:(NSError **)error
{
    [self assertRenderThread];

    if (_context == nullptr || prepared.rawPrepared == nullptr)
        return [self failWithStatus:SV_ERR_ARG error:error];

    return [self failWithStatus:sv_commit_preset(_context, prepared.rawPrepared, blendSeconds) error:error];
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

    if (_context == nullptr)
        return;

    sv_render_frame(_context, deltaTime, targetFramebuffer);

    if (_pendingDebugInfo == nil)
        return;

    NSDictionary<NSString*, id>* info = _pendingDebugInfo;
    _pendingDebugInfo = nil;

    if (_debugInfoHandler != nil)
        _debugInfoHandler(info);
}

- (void)storeDebugInfo:(const sv_debug_info *)info
{
    _pendingDebugInfo = SVDebugInfoDictionary(info);
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

- (float)playbackRate
{
    return _playbackRate;
}

- (void)setPlaybackRate:(float)playbackRate
{
    _playbackRate = SVNormalizedPlaybackRate(playbackRate);

    if (_context != nullptr)
        sv_set_playback_rate(_context, _playbackRate);
}

- (void (^)(NSDictionary<NSString *, id> *))debugInfoHandler
{
    return _debugInfoHandler;
}

- (void)setDebugInfoHandler:(void (^)(NSDictionary<NSString *, id> *))debugInfoHandler
{
    _debugInfoHandler = [debugInfoHandler copy];
    [self applyDebugCallback];
}

- (float)debugInterval
{
    return _debugInterval;
}

- (void)setDebugInterval:(float)debugInterval
{
    _debugInterval = isfinite(debugInterval) ? debugInterval : kDefaultDebugInterval;

    if (_context != nullptr)
        sv_set_debug_interval(_context, _debugInterval);
}

- (BOOL)debugEnabled
{
    return _debugEnabled;
}

- (void)setDebugEnabled:(BOOL)debugEnabled
{
    _debugEnabled = debugEnabled;
    [self applyDebugCallback];
}

- (void)applyDebugCallback
{
    const BOOL active = _debugEnabled && _debugInfoHandler != nil;

    if (!active)
        _pendingDebugInfo = nil;

    if (_context != nullptr)
        sv_set_debug_callback(_context, active ? SVDebugInfoTrampoline : nullptr, (__bridge void *)self);
}

- (SVAudioLevels)audioLevels
{
    SVAudioLevels levels = { 0.0f, 0.0f, 0.0f };

    if (_context != nullptr)
        sv_get_audio_levels(_context, &levels.bass, &levels.mid, &levels.treble);

    return levels;
}

- (SVPresetInfo)presetInfo
{
    SVPresetInfo result = { 0, 0, 0, NO, NO };
    sv_preset_info info = {};

    if (_context != nullptr && sv_get_preset_info(_context, &info) == SV_OK)
    {
        result.presetVersion = info.preset_version;
        result.warpPixelShaderVersion = info.warp_ps_version;
        result.compPixelShaderVersion = info.comp_ps_version;
        result.warpShaderActive = info.warp_shader != 0;
        result.compShaderActive = info.comp_shader != 0;
    }

    return result;
}

- (NSString *)presetFallbackReason
{
    sv_preset_info info = {};

    if (_context == nullptr || sv_get_preset_info(_context, &info) != SV_OK)
        return @"";

    info.fallback_reason[sizeof(info.fallback_reason) - 1] = '\0';
    return SVDebugString(info.fallback_reason);
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
    pthread_rwlock_wrlock(&_prepareLock);
    sv_destroy(_context);
    _context = nullptr;
    pthread_rwlock_unlock(&_prepareLock);
}

@end
