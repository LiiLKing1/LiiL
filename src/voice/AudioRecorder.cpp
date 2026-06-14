#include "AudioRecorder.h"
#include "../core/Logger.h"
#include <fstream>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "winmm.lib")

namespace liil {
namespace voice {

// Simple WAV header struct
struct WavHeader {
    char riff[4] = {'R','I','F','F'};
    uint32_t fileSize = 0;
    char wave[4] = {'W','A','V','E'};
    char fmt[4]  = {'f','m','t',' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = 16000;
    uint32_t byteRate = 16000 * 2;
    uint16_t blockAlign = 2;
    uint16_t bitsPerSample = 16;
    char data[4] = {'d','a','t','a'};
    uint32_t dataSize = 0;
};

const int NUM_BUFFERS = 4;
const int BUFFER_SIZE = 16000 * 2 / 2; // 0.5 soniyalik buffer (16kHz, 16bit = 32000 bytes/sec)

AudioRecorder::AudioRecorder() : m_hWaveIn(NULL), m_isRecording(false), m_isPaused(false) {
    m_wfx.wFormatTag = WAVE_FORMAT_PCM;
    m_wfx.nChannels = 1;
    m_wfx.nSamplesPerSec = 16000; // Optimal for Whisper
    m_wfx.wBitsPerSample = 16;
    m_wfx.nBlockAlign = (m_wfx.nChannels * m_wfx.wBitsPerSample) / 8;
    m_wfx.nAvgBytesPerSec = m_wfx.nSamplesPerSec * m_wfx.nBlockAlign;
    m_wfx.cbSize = 0;
}

AudioRecorder::~AudioRecorder() {
    CancelRecording();
}

void AudioRecorder::SetAmplitudeCallback(std::function<void(int)> callback) {
    m_amplitudeCallback = callback;
}

void CALLBACK AudioRecorder::WaveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (uMsg == WIM_DATA) {
        AudioRecorder* recorder = reinterpret_cast<AudioRecorder*>(dwInstance);
        PWAVEHDR pwh = reinterpret_cast<PWAVEHDR>(dwParam1);
        recorder->HandleData(pwh);
    }
}

void AudioRecorder::HandleData(PWAVEHDR pwh) {
    if (!m_isRecording) return;
    
    if (pwh->dwBytesRecorded > 0) {
        // Amplitude hisoblash (RMS)
        if (m_amplitudeCallback && !m_isPaused) {
            long long sumSquares = 0;
            int16_t* samples = reinterpret_cast<int16_t*>(pwh->lpData);
            int numSamples = pwh->dwBytesRecorded / 2;
            
            for (int i = 0; i < numSamples; i++) {
                sumSquares += samples[i] * samples[i];
            }
            
            double rms = 0;
            if (numSamples > 0) {
                rms = std::sqrt(sumSquares / numSamples);
            }
            
            // 0-32768 dan -> 0-100 foizga o'tkazamiz
            int amplitude = static_cast<int>((rms / 32768.0) * 100.0 * 5.0); // 5.0 multiplier for visual sensitivity
            amplitude = std::clamp(amplitude, 0, 100);
            m_amplitudeCallback(amplitude);
        }

        // Bufferga yozish (agar pauzada bo'lmasa)
        if (!m_isPaused) {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_audioData.insert(m_audioData.end(), pwh->lpData, pwh->lpData + pwh->dwBytesRecorded);
        }
    }

    // Yana yozish uchun bufferni qaytarish
    if (m_isRecording) {
        waveInAddBuffer(m_hWaveIn, pwh, sizeof(WAVEHDR));
    }
}

bool AudioRecorder::StartRecording() {
    if (m_isRecording) return false;

    m_audioData.clear();
    m_isPaused = false;

    MMRESULT res = waveInOpen(&m_hWaveIn, WAVE_MAPPER, &m_wfx, (DWORD_PTR)WaveInProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
    if (res != MMSYSERR_NOERROR) {
        core::Logger::Error("Voice: waveInOpen xatosi");
        return false;
    }

    m_headers.resize(NUM_BUFFERS);
    m_bufferStorage.resize(NUM_BUFFERS * BUFFER_SIZE);

    for (int i = 0; i < NUM_BUFFERS; i++) {
        m_headers[i].lpData = m_bufferStorage.data() + (i * BUFFER_SIZE);
        m_headers[i].dwBufferLength = BUFFER_SIZE;
        m_headers[i].dwBytesRecorded = 0;
        m_headers[i].dwUser = 0;
        m_headers[i].dwFlags = 0;
        m_headers[i].dwLoops = 0;

        waveInPrepareHeader(m_hWaveIn, &m_headers[i], sizeof(WAVEHDR));
        waveInAddBuffer(m_hWaveIn, &m_headers[i], sizeof(WAVEHDR));
    }

    m_isRecording = true;
    res = waveInStart(m_hWaveIn);
    if (res != MMSYSERR_NOERROR) {
        core::Logger::Error("Voice: waveInStart xatosi");
        CancelRecording();
        return false;
    }
    
    core::Logger::Info("Ovoz yozish boshlandi...");
    return true;
}

void AudioRecorder::PauseRecording() {
    if (m_isRecording) {
        m_isPaused = !m_isPaused; // Toggle pause
    }
}

void AudioRecorder::ResumeRecording() {
    if (m_isRecording) {
        m_isPaused = false;
    }
}

void AudioRecorder::CancelRecording() {
    if (!m_isRecording) return;
    
    m_isRecording = false;
    waveInStop(m_hWaveIn);
    waveInReset(m_hWaveIn);

    for (int i = 0; i < NUM_BUFFERS; i++) {
        waveInUnprepareHeader(m_hWaveIn, &m_headers[i], sizeof(WAVEHDR));
    }

    waveInClose(m_hWaveIn);
    m_hWaveIn = NULL;
    m_audioData.clear();
    
    // Equalizer ni nolga tushirish
    if (m_amplitudeCallback) {
        m_amplitudeCallback(0);
    }
    
    core::Logger::Info("Ovoz yozish bekor qilindi.");
}

void AudioRecorder::StopRecording(const std::string& outputFile) {
    if (!m_isRecording) return;
    
    m_isRecording = false;
    waveInStop(m_hWaveIn);
    waveInReset(m_hWaveIn);

    for (int i = 0; i < NUM_BUFFERS; i++) {
        waveInUnprepareHeader(m_hWaveIn, &m_headers[i], sizeof(WAVEHDR));
    }

    waveInClose(m_hWaveIn);
    m_hWaveIn = NULL;

    if (m_amplitudeCallback) {
        m_amplitudeCallback(0);
    }

    SaveToWav(outputFile);
    core::Logger::Info("Ovoz yozib olindi: " + outputFile);
}

void AudioRecorder::SaveToWav(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    if (m_audioData.empty()) return;

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return;

    WavHeader header;
    header.dataSize = m_audioData.size();
    header.fileSize = 36 + header.dataSize;

    file.write(reinterpret_cast<char*>(&header), sizeof(WavHeader));
    file.write(m_audioData.data(), m_audioData.size());
    file.close();
}

} // namespace voice
} // namespace liil
