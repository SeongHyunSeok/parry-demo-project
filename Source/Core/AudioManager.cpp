#include "AudioManager.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>

#define MINIAUDIO_IMPLEMENTATION
#include "../third-party/miniaudio/miniaudio.h"

namespace
{
    constexpr size_t VoiceCount = 8;
    constexpr size_t EffectCount = static_cast<size_t>(SoundId::Count);
    constexpr const wchar_t* EffectFiles[] = {
        L"sword-slash.mp3", L"sword-clash.mp3", L"fatal-hit.mp3"
    };

    std::filesystem::path FindSoundDirectory()
    {
        std::wstring executable(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, executable.data(),
            static_cast<DWORD>(executable.size()));
        if (length > 0 && length < executable.size())
        {
            executable.resize(length);
            auto directory = std::filesystem::path(executable).parent_path() / L"Sound";
            std::error_code error;
            if (std::filesystem::is_directory(directory, error))
                return directory;
        }
        // Supports launching from the project root before the first asset copy.
        return L"Sound";
    }
}

struct AudioManager::Impl
{
    struct Voice { ma_sound sound{}; bool ready = false; };
    ma_engine engine{};
    bool engineReady = false;
    std::array<std::array<Voice, VoiceCount>, EffectCount> effects{};
    std::array<size_t, EffectCount> nextVoice{};
    Voice music;
};

AudioManager::AudioManager() = default;
AudioManager::~AudioManager() { Shutdown(); }

AudioManager& AudioManager::GetInstance()
{
    static AudioManager instance;
    return instance;
}

bool AudioManager::Initialize(const std::filesystem::path& soundDirectory)
{
    Shutdown();
    lastError.clear();
    impl = std::make_unique<Impl>();
    auto report = [this](const std::string& message, ma_result result) {
        lastError += message + ": " + ma_result_description(result) + "\n";
        OutputDebugStringA(("Audio: " + lastError).c_str());
    };
    const auto result = ma_engine_init(nullptr, &impl->engine);
    if (result != MA_SUCCESS)
    {
        report("Could not initialize audio output", result);
        impl.reset();
        return false;
    }
    impl->engineReady = true;
    const auto directory = soundDirectory.empty() ? FindSoundDirectory() : soundDirectory;
    for (size_t effect = 0; effect < EffectCount; ++effect)
    {
        auto& voices = impl->effects[effect];
        const auto file = directory / EffectFiles[effect];
        auto loaded = ma_sound_init_from_file_w(&impl->engine, file.c_str(),
            MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
            nullptr, nullptr, &voices[0].sound);
        if (loaded != MA_SUCCESS)
        {
            report("Could not load " + file.filename().string(), loaded);
            continue;
        }
        voices[0].ready = true;
        for (size_t i = 1; i < VoiceCount; ++i)
        {
            loaded = ma_sound_init_copy(&impl->engine, &voices[0].sound,
                MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &voices[i].sound);
            voices[i].ready = loaded == MA_SUCCESS;
            if (!voices[i].ready)
                report("Could not create effect voice", loaded);
        }
    }

    const auto file = directory / L"mountain-presence.mp3";
    const auto loaded = ma_sound_init_from_file_w(&impl->engine, file.c_str(),
        MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
        nullptr, nullptr, &impl->music.sound);
    impl->music.ready = loaded == MA_SUCCESS;
    if (impl->music.ready)
        ma_sound_set_looping(&impl->music.sound, MA_TRUE);
    else
        report("Could not load mountain-presence.mp3", loaded);

    SetSfxVolume(sfxVolume);
    SetBgmVolume(bgmVolume);
    return lastError.empty();
}

void AudioManager::Shutdown()
{
    if (!impl) return;
    for (auto& voices : impl->effects)
        for (auto& voice : voices)
            if (voice.ready) ma_sound_uninit(&voice.sound);
    if (impl->music.ready) ma_sound_uninit(&impl->music.sound);
    if (impl->engineReady) ma_engine_uninit(&impl->engine);
    impl.reset();
}

bool AudioManager::PlaySfx(SoundId sound)
{
    const size_t effect = static_cast<size_t>(sound);
    if (!impl || !impl->engineReady || effect >= EffectCount) return false;
    auto& voices = impl->effects[effect];
    for (size_t offset = 0; offset < VoiceCount; ++offset)
    {
        const size_t index = (impl->nextVoice[effect] + offset) % VoiceCount;
        auto& voice = voices[index];
        if (!voice.ready || ma_sound_is_playing(&voice.sound)) continue;
        // A completed voice remains at EOF until explicitly rewound.
        if (ma_sound_seek_to_pcm_frame(&voice.sound, 0) != MA_SUCCESS) return false;
        ma_sound_set_volume(&voice.sound, sfxVolume);
        if (ma_sound_start(&voice.sound) != MA_SUCCESS) return false;
        impl->nextVoice[effect] = (index + 1) % VoiceCount;
        return true;
    }
    // Keep existing sounds intact when all eight voices are busy.
    return false;
}

void AudioManager::StopSfx()
{
    if (!impl) return;
    for (auto& voices : impl->effects)
        for (auto& voice : voices)
            if (voice.ready) ma_sound_stop(&voice.sound);
    impl->nextVoice.fill(0);
}

bool AudioManager::PlayBgm()
{
    if (!impl || !impl->music.ready) return false;
    if (ma_sound_is_playing(&impl->music.sound)) return true;
    return ma_sound_start(&impl->music.sound) == MA_SUCCESS;
}

void AudioManager::SetSfxVolume(float volume)
{
    sfxVolume = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 0.0f;
    if (!impl) return;
    for (auto& voices : impl->effects)
        for (auto& voice : voices)
            if (voice.ready) ma_sound_set_volume(&voice.sound, sfxVolume);
}

void AudioManager::SetBgmVolume(float volume)
{
    bgmVolume = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 0.0f;
    if (impl && impl->music.ready) ma_sound_set_volume(&impl->music.sound, bgmVolume);
}
