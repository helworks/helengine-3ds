#include "platform/3ds/audio/Nintendo3DsAudioBackend.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

namespace helengine::nintendo3ds {
    namespace {
        constexpr const char* AudioTracePath = "sdmc:/helengine_3ds_audio_trace.txt";
        constexpr const char* AudioDiagnosticLogPath = "sdmc:/helengine_3ds_last_error.txt";
        constexpr Result NdspMissingDspComponentResult = static_cast<Result>(0xD880A7FAu);

        void ResetAudioTrace() {
            std::FILE* file = std::fopen(AudioTracePath, "wb");
            if (file != nullptr) {
                std::fclose(file);
            }
        }

        void AppendAudioTrace(const std::string& message) {
            std::FILE* file = std::fopen(AudioTracePath, "ab");
            if (file == nullptr) {
                return;
            }

            std::fwrite(message.c_str(), 1, message.size(), file);
            const char newline = '\n';
            std::fwrite(&newline, 1, 1, file);
            std::fclose(file);
        }

        void WriteAudioDiagnosticLog(const char* phase, const std::string& message) {
            std::FILE* file = std::fopen(AudioDiagnosticLogPath, "wb");
            if (file == nullptr) {
                return;
            }

            const char* resolvedPhase = phase != nullptr ? phase : "audio-init";
            std::fprintf(file, "phase=%s\nmessage=%s\n", resolvedPhase, message.c_str());
            std::fclose(file);
        }

        std::string FormatResultHex(Result result) {
            char buffer[16] = {};
            std::snprintf(buffer, sizeof(buffer), "0x%08X", static_cast<unsigned int>(static_cast<uint32_t>(result)));
            return std::string(buffer);
        }

        std::string BuildNdspInitializationFailureMessage(Result result) {
            std::string message = "Nintendo 3DS NDSP initialization failed.";
            message += "\nresult=" + std::to_string(static_cast<int32_t>(result));
            message += "\nhex=" + FormatResultHex(result);
            if (result == NdspMissingDspComponentResult) {
                message += "\nreason=NDSP DSP component was not found.";
                message += "\nexpected=/3ds/dspfirm.cdc or hb:ndsp";
            }

            return message;
        }
    }

    Nintendo3DsAudioBackend::Nintendo3DsAudioBackend()
        : NdspInitialized(false)
        , NdspInitializationPermanentlyUnavailable(false)
        , NextVoiceId(0)
        , HasActiveVoice(false)
        , LastNdspInitializationResult(0)
        , ActiveVoice()
        , BusGainsById()
        , PausedBusIds()
        , WaveBuffers()
        , OutputBuffers() {
        ResetAudioTrace();
        AppendAudioTrace("ctor");
        BusGainsById.emplace("master", 1.0f);
        BusGainsById.emplace("music", 1.0f);
        BusGainsById.emplace("sfx", 1.0f);

        for (int bufferIndex = 0; bufferIndex < BufferCount; bufferIndex++) {
            OutputBuffers[bufferIndex] = static_cast<std::int16_t*>(
                linearAlloc(sizeof(std::int16_t) * BufferFrameCount * MaxSupportedChannelCount));
            if (OutputBuffers[bufferIndex] == nullptr) {
                AppendAudioTrace("linearAlloc failed");
                ReleaseWaveBuffers();
                throw std::bad_alloc();
            }
        }

        ResetWaveBuffers();
        TryInitializeNdsp();
    }

    Nintendo3DsAudioBackend::~Nintendo3DsAudioBackend() {
        ReleaseActiveVoice();
        ReleaseWaveBuffers();
        if (NdspInitialized) {
            ndspExit();
            NdspInitialized = false;
        }
    }

    int32_t Nintendo3DsAudioBackend::Play(::AudioAsset* asset, ::AudioPlaybackRequest* request) {
        if (asset == nullptr) {
            throw std::invalid_argument("asset");
        }
        AppendAudioTrace(
            std::string("play asset=")
            + asset->Id
            + " sampleRate="
            + std::to_string(asset->SampleRate)
            + " channels="
            + std::to_string(asset->Channels)
            + " encoding="
            + asset->EncodingFamilyId
            + " bytes="
            + std::to_string(asset->EncodedBytes != nullptr ? asset->EncodedBytes->Length : 0));
        if (asset->SampleRate <= 0) {
            throw std::runtime_error("Nintendo 3DS audio playback requires a positive sample rate.");
        }
        if (asset->Channels != 1 && asset->Channels != 2) {
            throw std::runtime_error("Nintendo 3DS audio playback supports only mono or stereo PCM assets.");
        }
        if (!UsesPcmEncoding(asset->EncodingFamilyId)) {
            throw std::runtime_error("Nintendo 3DS audio playback currently requires shared PCM assets.");
        }
        if (asset->EncodedBytes == nullptr || asset->EncodedBytes->Length <= 0 || asset->EncodedBytes->Data == nullptr) {
            throw std::runtime_error("Nintendo 3DS audio playback requires one non-empty encoded payload.");
        }

        const int32_t bytesPerFrame = static_cast<int32_t>(sizeof(std::int16_t) * asset->Channels);
        if (bytesPerFrame <= 0 || (asset->EncodedBytes->Length % bytesPerFrame) != 0) {
            throw std::runtime_error("Nintendo 3DS audio playback requires 16-bit PCM frame alignment.");
        }

        ReleaseActiveVoice();

        ActiveVoiceState voice = {};
        voice.VoiceId = NextVoiceId++;
        voice.SourceSamples = reinterpret_cast<const std::int16_t*>(asset->EncodedBytes->Data);
        voice.SourceFrameCount = asset->EncodedBytes->Length / bytesPerFrame;
        voice.SourceSampleRate = asset->SampleRate;
        voice.SourceChannelCount = asset->Channels;
        voice.SourceFrameCursor = 0;
        voice.BusId = NormalizeBusId(request != nullptr && !request->BusId.empty() ? request->BusId : asset->DefaultBusId);
        voice.BaseGain = ClampGain(request != nullptr ? request->Gain : 1.0f);
        voice.Loop = request != nullptr ? request->Loop : asset->DefaultLoop;
        voice.Playing = true;
        voice.ReachedEndOfSource = false;

        ActiveVoice = voice;
        HasActiveVoice = true;

        if (TryInitializeNdsp()) {
            AppendAudioTrace("play ndsp initialized");
            ConfigureChannel(ActiveVoice);
            for (int bufferIndex = 0; bufferIndex < BufferCount; bufferIndex++) {
                if (!FillAndQueueBuffer(bufferIndex)) {
                    break;
                }
            }

            if (!HasQueuedWaveBuffers()) {
                AppendAudioTrace("play no queued buffers");
                ReleaseActiveVoice();
                throw std::runtime_error("Nintendo 3DS audio playback could not queue any NDSP wave buffers.");
            }
        } else if (NdspInitializationPermanentlyUnavailable) {
            AppendAudioTrace("play unavailable release");
            ReleaseActiveVoice();
            return voice.VoiceId;
        }

        ApplyActiveVoiceState();
        AppendAudioTrace(
            std::string("play queued voiceId=")
            + std::to_string(voice.VoiceId)
            + " paused="
            + (IsBusPaused(voice.BusId) ? "true" : "false")
            + " gain="
            + std::to_string(ResolveCombinedGain(voice.BusId, voice.BaseGain)));
        return voice.VoiceId;
    }

    void Nintendo3DsAudioBackend::Stop(int32_t voiceId) {
        if (!HasActiveVoice || ActiveVoice.VoiceId != voiceId) {
            return;
        }

        ReleaseActiveVoice();
    }

    void Nintendo3DsAudioBackend::SetBusGain(std::string busId, float gain) {
        BusGainsById[NormalizeBusId(std::move(busId))] = ClampGain(gain);
        ApplyActiveVoiceState();
    }

    void Nintendo3DsAudioBackend::SetBusPaused(std::string busId, bool paused) {
        std::string normalizedBusId = NormalizeBusId(std::move(busId));
        if (paused) {
            PausedBusIds.insert(normalizedBusId);
        } else {
            PausedBusIds.erase(normalizedBusId);
        }

        ApplyActiveVoiceState();
    }

    bool Nintendo3DsAudioBackend::IsPlaying(int32_t voiceId) {
        if (!NdspInitialized && NdspInitializationPermanentlyUnavailable) {
            return false;
        }

        return HasActiveVoice && ActiveVoice.VoiceId == voiceId && (ActiveVoice.Playing || HasQueuedWaveBuffers());
    }

    void Nintendo3DsAudioBackend::Update() {
        if (!HasActiveVoice) {
            return;
        }
        if (!NdspInitialized) {
            if (NdspInitializationPermanentlyUnavailable) {
                AppendAudioTrace("update unavailable release");
                ReleaseActiveVoice();
                return;
            }

            if (!TryInitializeNdsp()) {
                AppendAudioTrace("update ndsp init deferred");
                return;
            }

            AppendAudioTrace("update ndsp initialized");
            ConfigureChannel(ActiveVoice);
            for (int bufferIndex = 0; bufferIndex < BufferCount; bufferIndex++) {
                if (!FillAndQueueBuffer(bufferIndex)) {
                    break;
                }
            }

            if (!HasQueuedWaveBuffers()) {
                AppendAudioTrace("update no queued buffers");
                ReleaseActiveVoice();
                return;
            }

            ApplyActiveVoiceState();
            return;
        }

        for (int bufferIndex = 0; bufferIndex < BufferCount; bufferIndex++) {
            if (WaveBuffers[bufferIndex].status == NDSP_WBUF_DONE) {
                AppendAudioTrace(std::string("buffer done index=") + std::to_string(bufferIndex));
                FillAndQueueBuffer(bufferIndex);
            }
        }

        if (ActiveVoice.ReachedEndOfSource && !HasQueuedWaveBuffers()) {
            AppendAudioTrace("voice reached end");
            ReleaseActiveVoice();
        }
    }

    bool Nintendo3DsAudioBackend::TryInitializeNdsp() {
        if (NdspInitialized) {
            return true;
        }
        if (NdspInitializationPermanentlyUnavailable) {
            return false;
        }

        Result initResult = ndspInit();
        if (R_FAILED(initResult)) {
            if (LastNdspInitializationResult != initResult) {
                const std::string failureMessage = BuildNdspInitializationFailureMessage(initResult);
                AppendAudioTrace(failureMessage);
                WriteAudioDiagnosticLog("audio-init", failureMessage);
            }

            LastNdspInitializationResult = initResult;
            if (initResult == NdspMissingDspComponentResult) {
                NdspInitializationPermanentlyUnavailable = true;
            }

            return false;
        }

        NdspInitialized = true;
        LastNdspInitializationResult = 0;
        AppendAudioTrace("ndspInit ok");
        ndspSetOutputMode(NDSP_OUTPUT_STEREO);
        ndspSetMasterVol(1.0f);
        ndspChnReset(ChannelId);
        ndspChnSetInterp(ChannelId, NDSP_INTERP_LINEAR);
        return true;
    }

    void Nintendo3DsAudioBackend::ConfigureChannel(const ActiveVoiceState& voice) {
        if (!NdspInitialized) {
            return;
        }

        ndspChnWaveBufClear(ChannelId);
        ndspChnReset(ChannelId);
        ndspChnSetInterp(ChannelId, NDSP_INTERP_LINEAR);
        ndspChnSetRate(ChannelId, static_cast<float>(voice.SourceSampleRate));
        ndspChnSetFormat(ChannelId, voice.SourceChannelCount == 1 ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16);
    }

    void Nintendo3DsAudioBackend::ApplyActiveVoiceState() {
        if (!NdspInitialized) {
            return;
        }

        float mix[12] = {};
        if (!HasActiveVoice) {
            ndspChnSetMix(ChannelId, mix);
            ndspChnSetPaused(ChannelId, true);
            return;
        }

        const bool shouldPause = IsBusPaused(ActiveVoice.BusId);
        const float combinedGain = ResolveCombinedGain(ActiveVoice.BusId, ActiveVoice.BaseGain);
        mix[0] = combinedGain;
        mix[1] = combinedGain;
        ndspChnSetMix(ChannelId, mix);
        ndspChnSetPaused(ChannelId, shouldPause);
        AppendAudioTrace(
            std::string("apply paused=")
            + (shouldPause ? "true" : "false")
            + " gain="
            + std::to_string(combinedGain)
            + " bus="
            + ActiveVoice.BusId);
    }

    bool Nintendo3DsAudioBackend::FillAndQueueBuffer(int bufferIndex) {
        if (!HasActiveVoice || bufferIndex < 0 || bufferIndex >= BufferCount) {
            return false;
        }

        ndspWaveBuf& waveBuffer = WaveBuffers[bufferIndex];
        std::int16_t* outputBuffer = OutputBuffers[bufferIndex];
        if (outputBuffer == nullptr) {
            return false;
        }

        const int32_t channelCount = ActiveVoice.SourceChannelCount;
        const int32_t maxSampleCount = BufferFrameCount * channelCount;
        std::memset(outputBuffer, 0, static_cast<std::size_t>(maxSampleCount) * sizeof(std::int16_t));

        int32_t framesWritten = 0;
        bool reachedEndOfSource = false;
        while (framesWritten < BufferFrameCount) {
            if (ActiveVoice.SourceFrameCursor >= ActiveVoice.SourceFrameCount) {
                if (!ActiveVoice.Loop) {
                    reachedEndOfSource = true;
                    break;
                }

                ActiveVoice.SourceFrameCursor = 0;
            }

            if (channelCount == 1) {
                outputBuffer[framesWritten] = ActiveVoice.SourceSamples[ActiveVoice.SourceFrameCursor];
            } else {
                const int32_t sourceSampleOffset = ActiveVoice.SourceFrameCursor * channelCount;
                const int32_t outputSampleOffset = framesWritten * channelCount;
                outputBuffer[outputSampleOffset] = ActiveVoice.SourceSamples[sourceSampleOffset];
                outputBuffer[outputSampleOffset + 1] = ActiveVoice.SourceSamples[sourceSampleOffset + 1];
            }

            framesWritten++;
            ActiveVoice.SourceFrameCursor++;
        }

        if (framesWritten <= 0) {
            ActiveVoice.ReachedEndOfSource = true;
            AppendAudioTrace(std::string("queue empty buffer index=") + std::to_string(bufferIndex));
            return false;
        }

        waveBuffer.nsamples = static_cast<u32>(framesWritten);
        waveBuffer.looping = false;
        DSP_FlushDataCache(outputBuffer, static_cast<u32>(framesWritten * channelCount * sizeof(std::int16_t)));
        ndspChnWaveBufAdd(ChannelId, &waveBuffer);
        AppendAudioTrace(
            std::string("queue buffer index=")
            + std::to_string(bufferIndex)
            + " frames="
            + std::to_string(framesWritten)
            + " cursor="
            + std::to_string(ActiveVoice.SourceFrameCursor)
            + " loop="
            + (ActiveVoice.Loop ? "true" : "false"));

        ActiveVoice.ReachedEndOfSource = reachedEndOfSource;
        return true;
    }

    bool Nintendo3DsAudioBackend::HasQueuedWaveBuffers() const {
        for (int bufferIndex = 0; bufferIndex < BufferCount; bufferIndex++) {
            const u8 status = WaveBuffers[bufferIndex].status;
            if (status == NDSP_WBUF_QUEUED || status == NDSP_WBUF_PLAYING) {
                return true;
            }
        }

        return false;
    }

    void Nintendo3DsAudioBackend::ReleaseActiveVoice() {
        if (NdspInitialized) {
            ndspChnWaveBufClear(ChannelId);
            ndspChnReset(ChannelId);
        }

        if (HasActiveVoice) {
            AppendAudioTrace(std::string("release voiceId=") + std::to_string(ActiveVoice.VoiceId));
        }
        ResetWaveBuffers();
        HasActiveVoice = false;
        ActiveVoice = {};
    }

    void Nintendo3DsAudioBackend::ResetWaveBuffers() {
        for (int bufferIndex = 0; bufferIndex < BufferCount; bufferIndex++) {
            ndspWaveBuf& waveBuffer = WaveBuffers[bufferIndex];
            std::memset(&waveBuffer, 0, sizeof(ndspWaveBuf));
            waveBuffer.data_pcm16 = OutputBuffers[bufferIndex];
        }
    }

    void Nintendo3DsAudioBackend::ReleaseWaveBuffers() {
        for (int bufferIndex = 0; bufferIndex < BufferCount; bufferIndex++) {
            if (OutputBuffers[bufferIndex] != nullptr) {
                linearFree(OutputBuffers[bufferIndex]);
                OutputBuffers[bufferIndex] = nullptr;
            }
        }

        ResetWaveBuffers();
    }

    std::string Nintendo3DsAudioBackend::NormalizeBusId(std::string busId) {
        if (busId.empty()) {
            return "master";
        }

        std::transform(
            busId.begin(),
            busId.end(),
            busId.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        return busId;
    }

    float Nintendo3DsAudioBackend::ClampGain(float gain) {
        if (!(gain >= 0.0f) || gain != gain) {
            return 0.0f;
        }

        return std::clamp(gain, 0.0f, 1.0f);
    }

    bool Nintendo3DsAudioBackend::UsesPcmEncoding(const std::string& encodingFamilyId) {
        return encodingFamilyId == "pcm-streamed" || encodingFamilyId == "pcm-buffered";
    }

    float Nintendo3DsAudioBackend::ResolveCombinedGain(const std::string& busId, float baseGain) const {
        float masterGain = 1.0f;
        auto masterGainIterator = BusGainsById.find("master");
        if (masterGainIterator != BusGainsById.end()) {
            masterGain = masterGainIterator->second;
        }

        float busGain = 1.0f;
        auto busGainIterator = BusGainsById.find(busId);
        if (busGainIterator != BusGainsById.end()) {
            busGain = busGainIterator->second;
        }

        return ClampGain(masterGain * busGain * baseGain);
    }

    bool Nintendo3DsAudioBackend::IsBusPaused(const std::string& busId) const {
        return PausedBusIds.contains("master") || PausedBusIds.contains(busId);
    }
}

#endif
