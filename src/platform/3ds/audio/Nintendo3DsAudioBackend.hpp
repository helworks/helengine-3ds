#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <3ds.h>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "AudioAsset.hpp"
#include "AudioPlaybackRequest.hpp"
#include "IAudioBackend.hpp"

namespace helengine::nintendo3ds {
    /// <summary>
    /// Streams shared Helengine PCM audio through Nintendo 3DS NDSP output channel zero.
    /// </summary>
    class Nintendo3DsAudioBackend final : public ::IAudioBackend {
    public:
        /// <summary>
        /// Initializes NDSP, allocates the linear output buffers, and configures default bus gains.
        /// </summary>
        Nintendo3DsAudioBackend();

        /// <summary>
        /// Stops playback, releases linear audio buffers, and shuts down NDSP.
        /// </summary>
        ~Nintendo3DsAudioBackend();

        int32_t Play(::AudioAsset* asset, ::AudioPlaybackRequest* request) override;

        void Stop(int32_t voiceId) override;

        void SetBusGain(std::string busId, float gain) override;

        void SetBusPaused(std::string busId, bool paused) override;

        bool IsPlaying(int32_t voiceId) override;

        void Update() override;

    private:
        static constexpr int ChannelId = 0;
        // Keep a deeper startup queue so early 3DS scene bring-up stalls do not underrun music immediately.
        static constexpr int BufferCount = 4;
        static constexpr int BufferFrameCount = 4096;
        static constexpr int MaxSupportedChannelCount = 2;

        /// <summary>
        /// Stores one active streamed voice routed through the single 3DS NDSP channel.
        /// </summary>
        struct ActiveVoiceState {
            int32_t VoiceId;
            const std::int16_t* SourceSamples;
            int32_t SourceFrameCount;
            int32_t SourceSampleRate;
            int32_t SourceChannelCount;
            int32_t SourceFrameCursor;
            std::string BusId;
            float BaseGain;
            bool Loop;
            bool Playing;
            bool ReachedEndOfSource;
        };

        bool TryInitializeNdsp();

        void ConfigureChannel(const ActiveVoiceState& voice);

        void ApplyActiveVoiceState();

        bool FillAndQueueBuffer(int bufferIndex);

        bool HasQueuedWaveBuffers() const;

        void ReleaseActiveVoice();

        void ResetWaveBuffers();

        void ReleaseWaveBuffers();

        static std::string NormalizeBusId(std::string busId);

        static float ClampGain(float gain);

        static bool UsesPcmEncoding(const std::string& encodingFamilyId);

        float ResolveCombinedGain(const std::string& busId, float baseGain) const;

        bool IsBusPaused(const std::string& busId) const;

        bool NdspInitialized;
        bool NdspInitializationPermanentlyUnavailable;
        int32_t NextVoiceId;
        bool HasActiveVoice;
        Result LastNdspInitializationResult;
        ActiveVoiceState ActiveVoice;
        std::unordered_map<std::string, float> BusGainsById;
        std::unordered_set<std::string> PausedBusIds;
        std::array<ndspWaveBuf, BufferCount> WaveBuffers;
        std::array<std::int16_t*, BufferCount> OutputBuffers;
    };
}

#endif
