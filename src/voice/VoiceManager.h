#pragma once
#include "AudioRecorder.h"
#include <string>
#include <functional>
#include <memory>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")

namespace liil {
namespace voice {

class VoiceManager {
public:
    static VoiceManager& GetInstance();
    
    // UI Equalizer uchun callback
    void SetAmplitudeCallback(std::function<void(int)> callback);
    
    // Ovozni yozishni boshlash (Dictation UI ochilganda)
    bool StartDictation();
    
    // Yozishni to'xtatish va transkriptsiya qilish (AI ga yuborish uchun)
    // Callback orqali tarjima qilingan text ni qaytaradi.
    void StopAndTranscribe(std::function<void(const std::string&)> onResult);
    
    void CancelDictation();
    void PauseDictation();
    void ResumeDictation();
    
    bool IsDictating() const;

private:
    VoiceManager();
    ~VoiceManager();
    
    bool IsInternetConnected();
    std::string TranscribeWithGemini(const std::string& filePath);
    std::string TranscribeWithSAPI(const std::string& filePath);
    
    std::unique_ptr<AudioRecorder> m_recorder;
    std::string m_tempWavPath;
    std::function<void(int)> m_amplitudeCallback;
};

} // namespace voice
} // namespace liil
