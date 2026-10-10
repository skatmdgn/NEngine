#include "nengine/audio/audio_device.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <ks.h>
#include <ksmedia.h>

namespace nengine::audio {
namespace {

enum class DeviceSampleFormat {
    Float32,
    Pcm16
};

std::string hresult_text(
    const char* operation,
    HRESULT result) {

    std::ostringstream out;
    out
        << operation
        << " failed (HRESULT 0x"
        << std::hex
        << std::uppercase
        << static_cast<unsigned long>(
            result)
        << ')';

    return out.str();
}

bool inspect_format(
    const WAVEFORMATEX* format,
    DeviceSampleFormat& sample_format) {

    if (!format ||
        format->nChannels == 0 ||
        format->nSamplesPerSec == 0) {
        return false;
    }

    if (format->wFormatTag ==
            WAVE_FORMAT_IEEE_FLOAT &&
        format->wBitsPerSample == 32) {

        sample_format =
            DeviceSampleFormat::Float32;
        return true;
    }

    if (format->wFormatTag ==
            WAVE_FORMAT_PCM &&
        format->wBitsPerSample == 16) {

        sample_format =
            DeviceSampleFormat::Pcm16;
        return true;
    }

    if (format->wFormatTag !=
            WAVE_FORMAT_EXTENSIBLE ||
        format->cbSize <
            sizeof(WAVEFORMATEXTENSIBLE) -
                sizeof(WAVEFORMATEX)) {

        return false;
    }

    const auto* extended =
        reinterpret_cast<
            const WAVEFORMATEXTENSIBLE*>(
                format);

    if (IsEqualGUID(
            extended->SubFormat,
            KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) &&
        format->wBitsPerSample == 32) {

        sample_format =
            DeviceSampleFormat::Float32;
        return true;
    }

    if (IsEqualGUID(
            extended->SubFormat,
            KSDATAFORMAT_SUBTYPE_PCM) &&
        format->wBitsPerSample == 16) {

        sample_format =
            DeviceSampleFormat::Pcm16;
        return true;
    }

    return false;
}

float channel_sample(
    const std::vector<float>& stereo,
    std::size_t frame,
    std::uint16_t channel,
    std::uint16_t channels) {

    const float left =
        stereo[frame * 2u];
    const float right =
        stereo[frame * 2u + 1u];

    if (channels == 1u) {
        return (left + right) * 0.5f;
    }

    if (channel == 0u) {
        return left;
    }

    if (channel == 1u) {
        return right;
    }

    return 0.0f;
}

} // namespace

struct AudioOutputDevice::Impl {
    bool open{false};
    bool null_fallback{false};
    bool com_initialized{false};

    std::string diagnostic{};

    IMMDeviceEnumerator* enumerator{nullptr};
    IMMDevice* endpoint{nullptr};
    IAudioClient* client{nullptr};
    IAudioRenderClient* render{nullptr};
    WAVEFORMATEX* format{nullptr};

    DeviceSampleFormat sample_format{
        DeviceSampleFormat::Float32};

    std::uint32_t sample_rate{48000};
    std::uint16_t channels{2};
    std::size_t buffer_frames{1440};
    std::uint64_t submitted_frames{0};
    std::vector<float> scratch{};

    void release_wasapi() noexcept {
        if (client) {
            client->Stop();
        }

        if (render) {
            render->Release();
            render = nullptr;
        }

        if (client) {
            client->Release();
            client = nullptr;
        }

        if (endpoint) {
            endpoint->Release();
            endpoint = nullptr;
        }

        if (enumerator) {
            enumerator->Release();
            enumerator = nullptr;
        }

        if (format) {
            CoTaskMemFree(format);
            format = nullptr;
        }

        if (com_initialized) {
            CoUninitialize();
            com_initialized = false;
        }
    }

    void use_null_fallback(
        std::string reason) noexcept {

        release_wasapi();
        null_fallback = true;
        open = true;
        diagnostic = std::move(reason);
        sample_rate = 48000;
        channels = 2;
        buffer_frames = 1440;
        submitted_frames = 0;
    }
};

AudioOutputDevice::AudioOutputDevice()
    : impl_(
          std::make_unique<Impl>()) {}

AudioOutputDevice::~AudioOutputDevice() {
    close();
}

AudioOutputDevice::AudioOutputDevice(
    AudioOutputDevice&&) noexcept =
    default;

AudioOutputDevice&
AudioOutputDevice::operator=(
    AudioOutputDevice&&) noexcept =
    default;

bool AudioOutputDevice::open(
    std::string* error) {

    if (!impl_) {
        if (error) {
            *error =
                "audio output implementation is unavailable";
        }
        return false;
    }

    close();

    const HRESULT com_result =
        CoInitializeEx(
            nullptr,
            COINIT_MULTITHREADED);

    if (SUCCEEDED(com_result)) {
        impl_->com_initialized = true;
    } else if (
        com_result !=
            RPC_E_CHANGED_MODE) {

        impl_->use_null_fallback(
            hresult_text(
                "CoInitializeEx",
                com_result));

        if (error) error->clear();
        return true;
    }

    HRESULT result =
        CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            reinterpret_cast<void**>(
                &impl_->enumerator));

    if (FAILED(result)) {
        impl_->use_null_fallback(
            hresult_text(
                "MMDeviceEnumerator",
                result));
        if (error) error->clear();
        return true;
    }

    result =
        impl_->enumerator
            ->GetDefaultAudioEndpoint(
                eRender,
                eConsole,
                &impl_->endpoint);

    if (FAILED(result)) {
        impl_->use_null_fallback(
            hresult_text(
                "GetDefaultAudioEndpoint",
                result));
        if (error) error->clear();
        return true;
    }

    result =
        impl_->endpoint->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void**>(
                &impl_->client));

    if (FAILED(result)) {
        impl_->use_null_fallback(
            hresult_text(
                "IAudioClient activation",
                result));
        if (error) error->clear();
        return true;
    }

    result =
        impl_->client->GetMixFormat(
            &impl_->format);

    if (FAILED(result) ||
        !inspect_format(
            impl_->format,
            impl_->sample_format)) {

        impl_->use_null_fallback(
            FAILED(result)
                ? hresult_text(
                      "GetMixFormat",
                      result)
                : std::string{
                      "WASAPI mix format is not float32 or PCM16"});

        if (error) error->clear();
        return true;
    }

    result =
        impl_->client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            0,
            300000,
            0,
            impl_->format,
            nullptr);

    if (FAILED(result)) {
        impl_->use_null_fallback(
            hresult_text(
                "IAudioClient::Initialize",
                result));
        if (error) error->clear();
        return true;
    }

    UINT32 buffer_frames = 0;

    result =
        impl_->client->GetBufferSize(
            &buffer_frames);

    if (FAILED(result) ||
        buffer_frames == 0u) {

        impl_->use_null_fallback(
            FAILED(result)
                ? hresult_text(
                      "IAudioClient::GetBufferSize",
                      result)
                : std::string{
                      "WASAPI returned an empty render buffer"});

        if (error) error->clear();
        return true;
    }

    result =
        impl_->client->GetService(
            __uuidof(IAudioRenderClient),
            reinterpret_cast<void**>(
                &impl_->render));

    if (FAILED(result)) {
        impl_->use_null_fallback(
            hresult_text(
                "IAudioRenderClient service",
                result));
        if (error) error->clear();
        return true;
    }

    result =
        impl_->client->Start();

    if (FAILED(result)) {
        impl_->use_null_fallback(
            hresult_text(
                "IAudioClient::Start",
                result));
        if (error) error->clear();
        return true;
    }

    impl_->null_fallback = false;
    impl_->open = true;
    impl_->diagnostic.clear();
    impl_->sample_rate =
        impl_->format->nSamplesPerSec;
    impl_->channels =
        impl_->format->nChannels;
    impl_->buffer_frames =
        static_cast<std::size_t>(
            buffer_frames);
    impl_->submitted_frames = 0;

    if (error) {
        error->clear();
    }

    return true;
}

void AudioOutputDevice::close() noexcept {
    if (!impl_) return;

    impl_->release_wasapi();
    impl_->open = false;
    impl_->null_fallback = false;
    impl_->submitted_frames = 0;
    impl_->scratch.clear();
}

bool AudioOutputDevice::is_open()
    const noexcept {

    return impl_ &&
        impl_->open;
}

AudioDeviceInfo AudioOutputDevice::info()
    const {

    AudioDeviceInfo result;

    if (!impl_) {
        return result;
    }

    result.open = impl_->open;
    result.backend =
        impl_->null_fallback
            ? "Null (WASAPI unavailable)"
            : "WASAPI";
    result.sample_rate =
        impl_->sample_rate;
    result.channels =
        impl_->channels;
    result.buffer_frames =
        impl_->buffer_frames;
    result.buffer_duration_ms =
        impl_->sample_rate == 0u
            ? 0.0
            : static_cast<double>(
                  impl_->buffer_frames) *
                1000.0 /
                static_cast<double>(
                    impl_->sample_rate);
    result.submitted_frames =
        impl_->submitted_frames;
    result.diagnostic =
        impl_->diagnostic;

    return result;
}

AudioRenderStats AudioOutputDevice::pump(
    const AudioMixSnapshot& snapshot,
    const AudioClipResolver& resolver,
    std::string* error) {

    if (!impl_ ||
        !impl_->open) {

        if (error) {
            *error =
                "audio output device is not open";
        }

        return {};
    }

    if (impl_->null_fallback) {
        auto stats =
            render_stereo_mix(
                snapshot,
                resolver,
                impl_->sample_rate,
                impl_->buffer_frames,
                impl_->scratch);

        impl_->submitted_frames +=
            static_cast<std::uint64_t>(
                stats.frames_rendered);

        if (error) error->clear();
        return stats;
    }

    UINT32 padding = 0;

    const HRESULT padding_result =
        impl_->client
            ->GetCurrentPadding(
                &padding);

    if (FAILED(padding_result)) {
        if (error) {
            *error =
                hresult_text(
                    "IAudioClient::GetCurrentPadding",
                    padding_result);
        }
        return {};
    }

    const auto total_frames =
        static_cast<UINT32>(
            impl_->buffer_frames);

    if (padding >= total_frames) {
        if (error) error->clear();
        return {};
    }

    const UINT32 available_frames =
        total_frames - padding;

    auto stats =
        render_stereo_mix(
            snapshot,
            resolver,
            impl_->sample_rate,
            static_cast<std::size_t>(
                available_frames),
            impl_->scratch);

    BYTE* destination = nullptr;

    HRESULT result =
        impl_->render->GetBuffer(
            available_frames,
            &destination);

    if (FAILED(result) ||
        !destination) {

        if (error) {
            *error =
                FAILED(result)
                    ? hresult_text(
                          "IAudioRenderClient::GetBuffer",
                          result)
                    : "WASAPI returned a null render buffer";
        }
        return {};
    }

    const auto channels =
        impl_->channels;

    if (impl_->sample_format ==
        DeviceSampleFormat::Float32) {

        auto* output =
            reinterpret_cast<float*>(
                destination);

        for (UINT32 frame = 0;
             frame < available_frames;
             ++frame) {

            for (std::uint16_t channel = 0;
                 channel < channels;
                 ++channel) {

                output[
                    static_cast<std::size_t>(
                        frame) *
                        channels +
                    channel] =
                    channel_sample(
                        impl_->scratch,
                        frame,
                        channel,
                        channels);
            }
        }
    } else {
        auto* output =
            reinterpret_cast<std::int16_t*>(
                destination);

        for (UINT32 frame = 0;
             frame < available_frames;
             ++frame) {

            for (std::uint16_t channel = 0;
                 channel < channels;
                 ++channel) {

                const float sample =
                    std::clamp(
                        channel_sample(
                            impl_->scratch,
                            frame,
                            channel,
                            channels),
                        -1.0f,
                        1.0f);

                output[
                    static_cast<std::size_t>(
                        frame) *
                        channels +
                    channel] =
                    static_cast<std::int16_t>(
                        std::lrint(
                            sample *
                            32767.0f));
            }
        }
    }

    result =
        impl_->render->ReleaseBuffer(
            available_frames,
            0);

    if (FAILED(result)) {
        if (error) {
            *error =
                hresult_text(
                    "IAudioRenderClient::ReleaseBuffer",
                    result);
        }
        return {};
    }

    impl_->submitted_frames +=
        static_cast<std::uint64_t>(
            available_frames);

    if (error) {
        error->clear();
    }

    return stats;
}

} // namespace nengine::audio
