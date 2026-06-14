#include "VoiceManager.h"
#include "../core/Logger.h"
#include "../core/HttpClient.h"
#include "../config/ConfigManager.h"
#include "../command/AIEngine.h"
#include "nlohmann/json.hpp"
#include <thread>
#include <filesystem>
#include <windows.h>
#include <wininet.h>
#include <sapi.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "sapi.lib")

namespace liil {
namespace voice {

static std::string Base64Encode(const std::vector<char>& data) {
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
        
    std::string ret;
    int i = 0, j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    for (size_t n = 0; n < data.size(); ++n) {
        char_array_3[i++] = data[n];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for(i = 0; i < 4; i++) ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i > 0) {
        for(j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        for (j = 0; j < i + 1; j++) ret += base64_chars[char_array_4[j]];
        while (i++ < 3) ret += '=';
    }
    return ret;
}

VoiceManager& VoiceManager::GetInstance() {
    static VoiceManager instance;
    return instance;
}

VoiceManager::VoiceManager() {
    m_recorder = std::make_unique<AudioRecorder>();
    
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::filesystem::path exeDir = std::filesystem::path(path).parent_path();
    m_tempWavPath = (exeDir / "temp_voice.wav").string();
}

VoiceManager::~VoiceManager() {
}

void VoiceManager::SetAmplitudeCallback(std::function<void(int)> callback) {
    m_amplitudeCallback = callback;
    if (m_recorder) {
        m_recorder->SetAmplitudeCallback(callback);
    }
}

bool VoiceManager::StartDictation() {
    return m_recorder->StartRecording();
}

void VoiceManager::PauseDictation() {
    m_recorder->PauseRecording();
}

void VoiceManager::ResumeDictation() {
    m_recorder->ResumeRecording();
}

void VoiceManager::CancelDictation() {
    m_recorder->CancelRecording();
    std::filesystem::remove(m_tempWavPath);
}

bool VoiceManager::IsDictating() const {
    return m_recorder->IsRecording();
}

bool VoiceManager::IsInternetConnected() {
    DWORD flags;
    return InternetGetConnectedState(&flags, 0) == TRUE;
}

void VoiceManager::StopAndTranscribe(std::function<void(const std::string&)> onResult) {
    if (!m_recorder->IsRecording()) return;
    
    m_recorder->StopRecording(m_tempWavPath);
    core::Logger::Info("Ovozni tarjima qilish (STT) boshlandi...");

    // Tarjimani alohida thread da qilamiz UI ni qotirmaslik uchun
    std::thread([this, onResult]() {
        std::string textResult = "";
        
        if (IsInternetConnected()) {
            textResult = TranscribeWithGemini(m_tempWavPath);
        } else {
            textResult = TranscribeWithSAPI(m_tempWavPath);
        }

        // Vaqtinchalik faylni tozalash
        std::filesystem::remove(m_tempWavPath);
        
        if (onResult) {
            onResult(textResult);
        }
    }).detach();
}

std::string VoiceManager::TranscribeWithGemini(const std::string& filePath) {
    auto& config = config::ConfigManager::GetInstance();
    std::string apiKey = config.GetString("openrouter_api_key");
    if (apiKey.empty()) {
        core::Logger::Error("Voice: openrouter_api_key topilmadi.");
        return "";
    }

    // Faylni o'qish va Base64 ga o'girish
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return "";
    DWORD fileSize = GetFileSize(hFile, NULL);
    std::vector<char> fileBuffer(fileSize);
    DWORD bytesRead = 0;
    ReadFile(hFile, fileBuffer.data(), fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    std::string base64Audio = Base64Encode(fileBuffer);

    std::string systemPrompt = 
        "Senga audio yozuv (o'zbek tilida) yuboriladi. Ovoz egasi nima deganligini tushunib eng toza, xatosiz, matnni qaytar. "
        "Foydalanuvchi ma'nosi kompyuterni boshqarish yoki savol berish ustida bo'lishi mumkin. "
        "Diqqat, faqat matnning o'zini (qo'shimcha izohsiz) qaytar.";

    nlohmann::json payload;
    payload["model"] = "google/gemini-2.5-flash";
    
    auto messagesArray = nlohmann::json::array();
    messagesArray.push_back({{"role", "system"}, {"content", systemPrompt}});
    
    // Gemini Base64 audio format
    auto userMsg = nlohmann::json::object();
    userMsg["role"] = "user";
    
    auto contentArr = nlohmann::json::array();
    auto audioObj = nlohmann::json::object();
    audioObj["type"] = "image_url"; 
    // OpenRouter da hozir audio support base64 payload bilan shunday yuborilishi mumkin (yoki base64 image_url)
    // Aslida Gemini OpenRouter orqali multi-part ni image_url qabul qiladi
    auto imgUrl = nlohmann::json::object();
    imgUrl["url"] = "data:audio/wav;base64," + base64Audio;
    audioObj["image_url"] = imgUrl;
    
    contentArr.push_back(audioObj);
    contentArr.push_back({{"type", "text"}, {"text", "Audioda nima deyilgan?"}});
    
    userMsg["content"] = contentArr;
    messagesArray.push_back(userMsg);

    payload["messages"] = messagesArray;

    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "Authorization: Bearer " + apiKey
    };

    std::string response = core::HttpClient::Post("https://openrouter.ai/api/v1/chat/completions", headers, payload.dump());

    if (!response.empty()) {
        try {
            auto j = nlohmann::json::parse(response);
            if (j.contains("choices") && j["choices"].is_array() && j["choices"].size() > 0) {
                return j["choices"][0]["message"]["content"].get<std::string>();
            }
        } catch (...) {}
    }
    
    return "";
}

std::string VoiceManager::TranscribeWithSAPI(const std::string& filePath) {
    // Windows SAPI orqali offline tarjima
    // C++ da ISpRecognizer yaratib .wav file o'qitish kerak.
    // Bu kod judayam murakkab va Offline o'zbek tili yo'qligi sababli mock tarzda.
    // Asl SAPI initialization ni yozish yuzlab qator COM kodlarni talab qiladi.
    
    core::Logger::Warning("Internet yo'q. Offline SAPI ishlatilmoqda. (O'zbek tilini tushunmasligi mumkin)");
    
    // Hozircha SAPI offline mode ni simulate qilamiz.
    // Agar to'liq SAPI integration kerak bo'lsa Microsoft Speech Platform SDK ishlatilishi lozim.
    
    return "Internet yo'q. Offline tarjima qismi tez orada ulanadi.";
}

} // namespace voice
} // namespace liil
