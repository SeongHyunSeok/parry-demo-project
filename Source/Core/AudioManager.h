#pragma once

#include <filesystem>
#include <memory>
#include <string>

enum class SoundId { SwordSlash, SwordClash, FatalHit, Count };

// Main-thread API. The audio engine mixes playback on its own thread.
class AudioManager
{
public:
    static AudioManager& GetInstance();
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool Initialize(const std::filesystem::path& soundDirectory = {});
    void Shutdown();
    bool PlaySfx(SoundId sound);
    void StopSfx();
    bool PlayBgm();
    void SetSfxVolume(float volume);
    void SetBgmVolume(float volume);
    float GetSfxVolume() const { return sfxVolume; }
    float GetBgmVolume() const { return bgmVolume; }
    const std::string& GetLastError() const { return lastError; }

private:
    AudioManager();
    struct Impl;
    std::unique_ptr<Impl> impl;
    float sfxVolume = 0.8f;
    float bgmVolume = 0.25f;
    std::string lastError;
};
