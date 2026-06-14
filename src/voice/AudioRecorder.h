#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>

namespace liil {
namespace voice {

class AudioRecorder {
public:
    AudioRecorder();
    ~AudioRecorder();

    bool StartRecording();
    void StopRecording(const std::string& outputFile);
    void CancelRecording();
    void PauseRecording();
    void ResumeRecording();

    void SetAmplitudeCallback(std::function<void(int)> callback);

    bool IsRecording() const { return m_isRecording; }
    bool IsPaused() const { return m_isPaused; }

private:
    static void CALLBACK WaveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
    void HandleData(PWAVEHDR pwh);
    void SaveToWav(const std::string& filename);

    HWAVEIN m_hWaveIn;
    WAVEFORMATEX m_wfx;
    std::vector<WAVEHDR> m_headers;
    std::vector<char> m_bufferStorage;

    std::vector<char> m_audioData; // All recorded data
    std::mutex m_dataMutex;

    std::atomic<bool> m_isRecording;
    std::atomic<bool> m_isPaused;
    std::function<void(int)> m_amplitudeCallback;
};

} // namespace voice
} // namespace liil
