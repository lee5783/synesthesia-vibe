//
//  audio_input.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-25.
//

#include "audio_input.h"

#include "audio_router.h"

#include <audioclient.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propidl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr int kTargetSampleRate = 44100;
    constexpr int kOutputChannels = 2;
    constexpr REFERENCE_TIME kBufferDuration = 2000000;
    constexpr DWORD kPollMilliseconds = 10;
    constexpr double kSilenceAfterSeconds = 0.05;
    constexpr double kMaxSilenceSeconds = 0.1;
    constexpr double kReopenSeconds = 0.5;
    constexpr float kSurroundWeight = 0.707f;

    const PROPERTYKEY kDeviceFriendlyName = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };

    enum class SampleKind
    {
        Float32,
        Int16,
        Int24,
        Int32
    };

    template <typename T>
    void ReleaseCom(T*& pointer)
    {
        if (pointer != nullptr)
        {
            pointer->Release();
            pointer = nullptr;
        }
    }

    std::string HresultText(const char* what, HRESULT hr)
    {
        char text[160];
        snprintf(text, sizeof(text), "%s failed (HRESULT 0x%08lX)", what, static_cast<unsigned long>(hr));
        return text;
    }

    std::string Utf8(const wchar_t* text)
    {
        if (text == nullptr)
            return std::string();

        const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        if (bytes <= 1)
            return std::string();

        std::string result(static_cast<size_t>(bytes - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], bytes, nullptr, nullptr);
        return result;
    }

    float ReadSample(const BYTE* sample, SampleKind kind)
    {
        switch (kind)
        {
        case SampleKind::Float32:
        {
            float value = 0.0f;
            memcpy(&value, sample, sizeof(value));
            return value;
        }
        case SampleKind::Int16:
        {
            int16_t value = 0;
            memcpy(&value, sample, sizeof(value));
            return value / 32768.0f;
        }
        case SampleKind::Int24:
        {
            const int32_t value = static_cast<int32_t>((static_cast<uint32_t>(sample[0]) << 8) | (static_cast<uint32_t>(sample[1]) << 16)
                                                       | (static_cast<uint32_t>(sample[2]) << 24));
            return static_cast<float>(value / 2147483648.0);
        }
        case SampleKind::Int32:
        {
            int32_t value = 0;
            memcpy(&value, sample, sizeof(value));
            return static_cast<float>(value / 2147483648.0);
        }
        }
        return 0.0f;
    }

    class DeviceNotifier : public IMMNotificationClient
    {
    public:
        DeviceNotifier(EDataFlow flow, HANDLE changed) : m_refs(1), m_flow(flow), m_changed(changed) {}

        ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&m_refs)); }

        ULONG STDMETHODCALLTYPE Release() override
        {
            const LONG refs = InterlockedDecrement(&m_refs);
            if (refs == 0)
                delete this;
            return static_cast<ULONG>(refs);
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
        {
            if (out == nullptr)
                return E_POINTER;

            if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient))
            {
                *out = static_cast<IMMNotificationClient*>(this);
                AddRef();
                return S_OK;
            }

            *out = nullptr;
            return E_NOINTERFACE;
        }

        HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override
        {
            if (flow == m_flow && role == eConsole)
                SetEvent(m_changed);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override
        {
            SetEvent(m_changed);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }

        HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override
        {
            SetEvent(m_changed);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

    private:
        virtual ~DeviceNotifier() = default;

        LONG m_refs;
        EDataFlow m_flow;
        HANDLE m_changed;
    };
}

struct svw_audio_input::Stream
{
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioCaptureClient* capture = nullptr;
    std::wstring deviceId;
    SampleKind kind = SampleKind::Float32;
    int bytesPerSample = 4;
    int channels = 0;
    int sampleRate = 0;
    double phase = 0.0;
    float previousLeft = 0.0f;
    float previousRight = 0.0f;
    double lastOpenAttempt = 0.0;
    bool open = false;
};

svw_audio_input::svw_audio_input(int source)
    : m_source(source)
    , m_gain(1.0f)
    , m_captureThreadId(0)
    , m_stop(CreateEventW(nullptr, TRUE, FALSE, nullptr))
    , m_started(CreateEventW(nullptr, TRUE, FALSE, nullptr))
    , m_startStatus(SV_ERR_IO)
    , m_running(false)
    , m_pcmFn(nullptr)
    , m_pcmUser(nullptr)
    , m_lastPushAt(0.0)
    , m_ticksPerSecond(1.0)
{
    LARGE_INTEGER frequency;
    if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0)
        m_ticksPerSecond = static_cast<double>(frequency.QuadPart);
}

svw_audio_input::~svw_audio_input()
{
    Stop();

    if (m_stop != nullptr)
        CloseHandle(m_stop);
    if (m_started != nullptr)
        CloseHandle(m_started);
}

sv_status svw_audio_input::Start()
{
    std::lock_guard<std::mutex> lock(m_controlMutex);
    if (m_running)
        return SV_OK;

    if (m_stop == nullptr || m_started == nullptr)
        return Fail(SV_ERR_IO, "svw_audio_input_start: could not create the capture thread's events");

    ResetEvent(m_stop);
    ResetEvent(m_started);
    m_startStatus = SV_ERR_IO;
    m_startError.clear();
    m_thread = std::thread(&svw_audio_input::ThreadMain, this);
    WaitForSingleObject(m_started, INFINITE);

    if (m_startStatus != SV_OK)
    {
        m_thread.join();
        return Fail(m_startStatus, "svw_audio_input_start: " + m_startError);
    }

    m_running = true;
    Fail(SV_OK, "");
    return SV_OK;
}

sv_status svw_audio_input::Stop()
{
    if (OnCaptureThread())
        return Fail(SV_ERR_ARG, "svw_audio_input_stop: called from the svw_pcm_fn callback");

    std::lock_guard<std::mutex> lock(m_controlMutex);
    if (!m_running)
        return SV_OK;

    SetEvent(m_stop);
    m_thread.join();
    m_running = false;
    return SV_OK;
}

sv_status svw_audio_input::SetGain(float gain)
{
    if (!std::isfinite(gain) || gain < 0.0f)
        return Fail(SV_ERR_ARG, "svw_audio_input_set_gain: gain must be finite and >= 0");

    m_gain = gain;
    return SV_OK;
}

sv_status svw_audio_input::SetPcmCallback(svw_pcm_fn fn, void* user)
{
    if (OnCaptureThread())
        return Fail(SV_ERR_ARG, "svw_audio_input_set_pcm_callback: called from the svw_pcm_fn callback");

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_pcmFn = fn;
    m_pcmUser = user;
    return SV_OK;
}

std::string svw_audio_input::LastError() const
{
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_lastError;
}

std::string svw_audio_input::DeviceName() const
{
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_deviceName;
}

sv_status svw_audio_input::Fail(sv_status status, const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError = message;
    return status;
}

void svw_audio_input::SetDeviceName(const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_deviceName = name;
}

void svw_audio_input::ThreadMain()
{
    m_captureThreadId = GetCurrentThreadId();
    SetThreadDescription(GetCurrentThread(), m_source == SVW_AUDIO_LOOPBACK ? L"synesthesia-vibe loopback" : L"synesthesia-vibe microphone");

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInitialized = SUCCEEDED(comResult);

    IMMDeviceEnumerator* enumerator = nullptr;
    DeviceNotifier* notifier = nullptr;
    const HANDLE deviceChanged = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Stream stream;
    std::string error;
    sv_status status = SV_OK;

    if (!comInitialized && comResult != RPC_E_CHANGED_MODE)
    {
        status = SV_ERR_IO;
        error = HresultText("CoInitializeEx", comResult);
    }

    if (status == SV_OK)
    {
        const HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                            reinterpret_cast<void**>(&enumerator));
        if (FAILED(hr))
        {
            status = SV_ERR_IO;
            error = HresultText("CoCreateInstance(MMDeviceEnumerator)", hr);
        }
    }

    if (status == SV_OK && deviceChanged != nullptr)
    {
        notifier = new DeviceNotifier(m_source == SVW_AUDIO_LOOPBACK ? eRender : eCapture, deviceChanged);
        if (FAILED(enumerator->RegisterEndpointNotificationCallback(notifier)))
            ReleaseCom(notifier);
    }

    if (status == SV_OK)
        status = OpenStream(enumerator, stream, error);

    m_startStatus = status;
    m_startError = error;
    SetEvent(m_started);

    if (status == SV_OK)
        CaptureLoop(enumerator, deviceChanged, stream);

    CloseStream(stream);
    if (notifier != nullptr)
    {
        enumerator->UnregisterEndpointNotificationCallback(notifier);
        ReleaseCom(notifier);
    }
    ReleaseCom(enumerator);
    if (deviceChanged != nullptr)
        CloseHandle(deviceChanged);
    if (comInitialized)
        CoUninitialize();
    m_captureThreadId = 0;
}

void svw_audio_input::CaptureLoop(IMMDeviceEnumerator* enumerator, HANDLE deviceChanged, Stream& stream)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    m_lastPushAt = counter.QuadPart / m_ticksPerSecond;

    const HANDLE handles[2] = { m_stop, deviceChanged };
    const DWORD handleCount = deviceChanged != nullptr ? 2 : 1;

    for (;;)
    {
        const DWORD result = WaitForMultipleObjects(handleCount, handles, FALSE, kPollMilliseconds);
        if (result == WAIT_OBJECT_0)
            return;

        QueryPerformanceCounter(&counter);
        const double now = counter.QuadPart / m_ticksPerSecond;

        bool reopen = false;
        if (result == WAIT_OBJECT_0 + 1)
        {
            IMMDevice* device = nullptr;
            LPWSTR id = nullptr;
            const EDataFlow flow = m_source == SVW_AUDIO_LOOPBACK ? eRender : eCapture;
            if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(flow, eConsole, &device)) && SUCCEEDED(device->GetId(&id)))
                reopen = !stream.open || stream.deviceId != id;
            else
                reopen = stream.open;
            if (id != nullptr)
                CoTaskMemFree(id);
            ReleaseCom(device);
        }

        if (stream.open && !DrainPackets(stream))
            reopen = true;

        if (reopen)
        {
            CloseStream(stream);
            stream.lastOpenAttempt = 0.0;
        }

        if (!stream.open && now - stream.lastOpenAttempt >= kReopenSeconds)
        {
            stream.lastOpenAttempt = now;
            std::string error;
            if (OpenStream(enumerator, stream, error) == SV_OK)
                Fail(SV_OK, "capture rebuilt on " + DeviceName());
            else
                Fail(SV_ERR_IO, "capture lost, retrying: " + error);
        }

        QueryPerformanceCounter(&counter);
        const double after = counter.QuadPart / m_ticksPerSecond;
        if (after - m_lastPushAt > kSilenceAfterSeconds)
            PushSilence(std::min(after - m_lastPushAt, kMaxSilenceSeconds));
    }
}

sv_status svw_audio_input::OpenStream(IMMDeviceEnumerator* enumerator, Stream& stream, std::string& error)
{
    const bool loopback = m_source == SVW_AUDIO_LOOPBACK;
    const char* role = loopback ? "loopback" : "microphone";

    HRESULT hr = enumerator->GetDefaultAudioEndpoint(loopback ? eRender : eCapture, eConsole, &stream.device);
    if (hr == E_NOTFOUND)
    {
        error = loopback ? "no default audio output device" : "no default microphone";
        return SV_ERR_IO;
    }
    if (FAILED(hr))
    {
        error = HresultText("GetDefaultAudioEndpoint", hr);
        return SV_ERR_IO;
    }

    LPWSTR id = nullptr;
    if (SUCCEEDED(stream.device->GetId(&id)))
    {
        stream.deviceId = id;
        CoTaskMemFree(id);
    }

    IPropertyStore* properties = nullptr;
    if (SUCCEEDED(stream.device->OpenPropertyStore(STGM_READ, &properties)))
    {
        PROPVARIANT name;
        PropVariantInit(&name);
        if (SUCCEEDED(properties->GetValue(kDeviceFriendlyName, &name)) && name.vt == VT_LPWSTR)
            SetDeviceName(Utf8(name.pwszVal));
        PropVariantClear(&name);
        ReleaseCom(properties);
    }

    hr = stream.device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&stream.client));
    if (hr == E_ACCESSDENIED)
    {
        error = loopback ? "loopback capture of the default audio output is denied (E_ACCESSDENIED): a policy or another app is blocking it" : "microphone access is denied: turn on Settings > Privacy > Microphone for desktop apps";
        CloseStream(stream);
        return SV_ERR_IO;
    }
    if (FAILED(hr))
    {
        error = HresultText("IMMDevice::Activate(IAudioClient)", hr);
        CloseStream(stream);
        return SV_ERR_IO;
    }

    WAVEFORMATEX* format = nullptr;
    hr = stream.client->GetMixFormat(&format);
    if (FAILED(hr))
    {
        error = HresultText("IAudioClient::GetMixFormat", hr);
        CloseStream(stream);
        return SV_ERR_IO;
    }

    bool isFloat = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    bool isPcm = format->wFormatTag == WAVE_FORMAT_PCM;
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        const WAVEFORMATEXTENSIBLE* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        isFloat = extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        isPcm = extensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM;
    }

    stream.channels = format->nChannels;
    stream.sampleRate = static_cast<int>(format->nSamplesPerSec);
    stream.bytesPerSample = format->wBitsPerSample / 8;
    bool supported = stream.channels >= 1 && stream.sampleRate >= 8000;
    if (isFloat && format->wBitsPerSample == 32)
        stream.kind = SampleKind::Float32;
    else if (isPcm && format->wBitsPerSample == 16)
        stream.kind = SampleKind::Int16;
    else if (isPcm && format->wBitsPerSample == 24)
        stream.kind = SampleKind::Int24;
    else if (isPcm && format->wBitsPerSample == 32)
        stream.kind = SampleKind::Int32;
    else
        supported = false;

    if (!supported)
    {
        char text[128];
        snprintf(text, sizeof(text), "unsupported mix format tag=%u bits=%u channels=%d rate=%d", static_cast<unsigned>(format->wFormatTag),
                 static_cast<unsigned>(format->wBitsPerSample), stream.channels, stream.sampleRate);
        error = text;
        CoTaskMemFree(format);
        CloseStream(stream);
        return SV_ERR_IO;
    }

    hr = stream.client->Initialize(AUDCLNT_SHAREMODE_SHARED, loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0, kBufferDuration, 0, format, nullptr);
    CoTaskMemFree(format);
    if (hr == E_ACCESSDENIED)
    {
        error = loopback ? "loopback capture of the default audio output is denied (E_ACCESSDENIED): a policy or another app is blocking it" : "microphone access is denied: turn on Settings > Privacy > Microphone for desktop apps";
        CloseStream(stream);
        return SV_ERR_IO;
    }
    if (FAILED(hr))
    {
        error = HresultText(loopback ? "IAudioClient::Initialize(loopback)" : "IAudioClient::Initialize(microphone)", hr);
        CloseStream(stream);
        return SV_ERR_IO;
    }

    hr = stream.client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&stream.capture));
    if (SUCCEEDED(hr))
        hr = stream.client->Start();
    if (FAILED(hr))
    {
        error = HresultText(role, hr);
        CloseStream(stream);
        return SV_ERR_IO;
    }

    stream.phase = 0.0;
    stream.previousLeft = 0.0f;
    stream.previousRight = 0.0f;
    stream.open = true;
    return SV_OK;
}

void svw_audio_input::CloseStream(Stream& stream)
{
    if (stream.client != nullptr && stream.open)
        stream.client->Stop();

    ReleaseCom(stream.capture);
    ReleaseCom(stream.client);
    ReleaseCom(stream.device);
    stream.deviceId.clear();
    stream.open = false;
}

bool svw_audio_input::DrainPackets(Stream& stream)
{
    UINT32 packetFrames = 0;
    HRESULT hr = stream.capture->GetNextPacketSize(&packetFrames);

    while (SUCCEEDED(hr) && packetFrames != 0)
    {
        BYTE* data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        hr = stream.capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
        if (FAILED(hr))
            break;

        ConvertPacket(stream, data, frames, (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0);
        stream.capture->ReleaseBuffer(frames);
        PushStaged();
        hr = stream.capture->GetNextPacketSize(&packetFrames);
    }

    if (SUCCEEDED(hr))
        return true;

    Fail(SV_ERR_IO, HresultText(hr == AUDCLNT_E_DEVICE_INVALIDATED ? "capture (device invalidated)" : "capture", hr));
    return false;
}

void svw_audio_input::ConvertPacket(Stream& stream, const BYTE* data, UINT32 frames, bool silent)
{
    const double step = static_cast<double>(stream.sampleRate) / kTargetSampleRate;
    const size_t blockAlign = static_cast<size_t>(stream.bytesPerSample) * stream.channels;

    for (UINT32 index = 0; index < frames; ++index)
    {
        float left = 0.0f;
        float right = 0.0f;

        if (!silent && data != nullptr)
        {
            const BYTE* frame = data + index * blockAlign;
            left = ReadSample(frame, stream.kind);
            right = stream.channels > 1 ? ReadSample(frame + stream.bytesPerSample, stream.kind) : left;

            if (stream.channels >= 6)
            {
                const float centre = ReadSample(frame + 2 * stream.bytesPerSample, stream.kind) * kSurroundWeight;
                const float lowFrequency = ReadSample(frame + 3 * stream.bytesPerSample, stream.kind) * kSurroundWeight;
                const float surroundLeft = ReadSample(frame + 4 * stream.bytesPerSample, stream.kind) * kSurroundWeight;
                const float surroundRight = ReadSample(frame + 5 * stream.bytesPerSample, stream.kind) * kSurroundWeight;
                left = (left + centre + lowFrequency + surroundLeft) * 0.5f;
                right = (right + centre + lowFrequency + surroundRight) * 0.5f;
            }
        }

        if (stream.sampleRate == kTargetSampleRate)
        {
            m_staged.push_back(left);
            m_staged.push_back(right);
            continue;
        }

        while (stream.phase < 1.0)
        {
            const float weight = static_cast<float>(stream.phase);
            m_staged.push_back(stream.previousLeft + (left - stream.previousLeft) * weight);
            m_staged.push_back(stream.previousRight + (right - stream.previousRight) * weight);
            stream.phase += step;
        }

        stream.phase -= 1.0;
        stream.previousLeft = left;
        stream.previousRight = right;
    }
}

void svw_audio_input::PushStaged()
{
    if (m_staged.empty())
        return;

    const float gain = m_gain.load();
    if (gain != 1.0f)
    {
        for (float& sample : m_staged)
            sample *= gain;
    }

    const size_t frames = m_staged.size() / kOutputChannels;
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        if (m_pcmFn != nullptr)
            m_pcmFn(m_staged.data(), frames, kOutputChannels, m_pcmUser);
    }

    svw::PushPcmToLiveContext(m_staged.data(), frames, kOutputChannels);
    m_staged.clear();

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    m_lastPushAt = counter.QuadPart / m_ticksPerSecond;
}

void svw_audio_input::PushSilence(double seconds)
{
    const size_t frames = static_cast<size_t>(seconds * kTargetSampleRate);
    if (frames == 0)
        return;

    m_staged.assign(frames * kOutputChannels, 0.0f);
    PushStaged();
}
