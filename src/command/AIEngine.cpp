#include "AIEngine.h"
#include "CommandEngine.h"
#include "../core/HttpClient.h"
#include "../core/Logger.h"
#include "../config/ConfigManager.h"
#include "../core/SystemMonitor.h"
#include "../core/MemoryManager.h"
#include "nlohmann/json.hpp"
#include <thread>
#include <vector>

namespace liil {
namespace command {

// Chat xotirasi (Maks 10 ta eng oxirgi xabarlar)
static std::vector<nlohmann::json> s_chatHistory;

void AIEngine::ProcessCommand(const std::string& input) {
    // UI bloklanmasligi uchun AI so'rovlarini alohida asinxron thread'da ishga tushiramiz
    std::thread([input]() {
        auto& config = config::ConfigManager::GetInstance();
        std::string apiKey = config.GetString("openrouter_api_key", "");
        
        if (apiKey.empty()) {
            core::Logger::Warning("AI ishlamaydi: 'openrouter_api_key' sozlamalarda (settings.ini) kiritilmagan.");
            return;
        }

        std::string systemPrompt = 
            "Sen kompyuterni boshqaruvchi LiiL assistentsan. "
            "Senga berilgan matnga mos keluvchi JSON massiv (array) qaytar. "
            "Agar bir nechta amal so'ralsa, barchasini ketma-ket massivga joyla. "
            "JSON formati: [{\"action\": \"<harakat>\", \"target\": \"<maqsad>\"}, ...]. "
            "Ruxsat etilgan action'lar: open_app, close_app, open_folder, volume_up, volume_down, volume_mute, brightness_up, brightness_down, take_screenshot, coding_mode, gaming_mode, view_logs, update_memory, chat. "
            "MUHIM: Foydalanuvchi loglarni aynan HOZIR ochiq bo'lishini aniq SO'RAMASA, 'view_logs' amalini QILMA! Faqat suhbatlashilsa loglarni ochma. "
            "Agar foydalanuvchi shaxsiy malumotini aytsa yoki o'zgartirishni xohlasa: action='update_memory' dan foydalan va target sifatida aniq JSON formatidagi string qoldir. "
            "Masalan target='{\"action\": \"save\", \"title\": \"Sevimli ovqat\", \"value\": \"KFC\", \"is_private\": false}'. "
            "Agar shaxsiy, sirli ma'lumot (parol, login) bo'lsa yoki o'zi 'maxfiy qil' desa AVTOMATIK tarzda is_private: true qo'y! "
            "Agar bor ma'lumotni o'zgartirish kerak bo'lsa: '{\"action\": \"edit\", \"old_title\": \"eski_nom\", \"title\": \"Yangi nom\", \"value\": \"Yangi\", \"is_private\": true}' "
            "Agar o'chirishni so'rasa: '{\"action\": \"delete\", \"title\": \"nomi\"}'. "
            "Kompyuter haqidagi savollarga System Context'dan o'qib 'chat' action orqali javob ber. Hech qachon 'bilmayman' dema! "
            "Sening javobing FAQAT JSON massivi bo'lishi shart.\n\n"
            + core::SystemMonitor::GetSystemContext() + "\n"
            + core::MemoryManager::GetInstance().GetAllFactsAsString();

        // Tarixni saqlash
        s_chatHistory.push_back({{"role", "user"}, {"content", input}});
        if (s_chatHistory.size() > 10) {
            s_chatHistory.erase(s_chatHistory.begin());
        }

        auto messagesArray = nlohmann::json::array();
        messagesArray.push_back({{"role", "system"}, {"content", systemPrompt}});
        for (const auto& msg : s_chatHistory) {
            messagesArray.push_back(msg);
        }

        nlohmann::json payload = {
            {"model", config.GetString("ai_model", "google/gemini-2.5-flash")},
            {"messages", messagesArray},
            {"temperature", 0.0} // Har doim aniq bir xil JSON uchun
        };

        std::vector<std::string> headers = {
            "Content-Type: application/json",
            "Authorization: Bearer " + apiKey
        };

        std::string url = "https://openrouter.ai/api/v1/chat/completions";

        std::string responseStr = core::HttpClient::Post(url, headers, payload.dump());

        if (responseStr.empty()) {
            core::Logger::Error("AI bilan ulanishda xatolik yuz berdi (Tarmoq yoki API xatosi).");
            return;
        }

        try {
            auto responseJson = nlohmann::json::parse(responseStr);
            if (responseJson.contains("choices") && responseJson["choices"].size() > 0) {
                std::string content = responseJson["choices"][0]["message"]["content"];
                
                // AI agar xato qilib Markdown formatda yuborsa, tozalaymiz
                if (content.find("```json") == 0) {
                    content = content.substr(7);
                    if (content.rfind("```") != std::string::npos) {
                        content = content.substr(0, content.rfind("```"));
                    }
                } else if (content.find("```") == 0) {
                    content = content.substr(3);
                    if (content.rfind("```") != std::string::npos) {
                        content = content.substr(0, content.rfind("```"));
                    }
                }

                auto actionJson = nlohmann::json::parse(content);
                
                if (actionJson.is_array()) {
                    for (const auto& item : actionJson) {
                        std::string action = item.value("action", "");
                        std::string target = item.value("target", "");
                        CommandEngine::GetInstance().ExecuteAction(action, target);
                        
                        // AI javobini xotiraga qo'shish (faqat chat bo'lsa mantiqan to'g'ri, ammo umuman olganda nima amallar qilganini bilsa ham yaxshi)
                        if (action == "chat") {
                            s_chatHistory.push_back({{"role", "assistant"}, {"content", "[{\"action\":\"chat\",\"target\":\"" + target + "\"}]"}});
                        }
                    }
                } else if (actionJson.is_object()) {
                    std::string action = actionJson.value("action", "");
                    std::string target = actionJson.value("target", "");
                    CommandEngine::GetInstance().ExecuteAction(action, target);
                    if (action == "chat") {
                        s_chatHistory.push_back({{"role", "assistant"}, {"content", "[{\"action\":\"chat\",\"target\":\"" + target + "\"}]"}});
                    }
                }
            } else {
                core::Logger::Error("AI noto'g'ri javob qaytardi: " + responseStr);
            }
        } catch (const nlohmann::json::exception& e) {
            core::Logger::Error("AI javobini JSON parse qilishda xato: " + std::string(e.what()));
        }
    }).detach();
}

std::string AIEngine::CorrectText(const std::string& rawWhisperText) {
    auto& config = config::ConfigManager::GetInstance();
    std::string apiKey = config.GetString("openrouter_api_key");
    if (apiKey.empty()) return rawWhisperText;

    std::string model = config.GetString("model");
    if (model.empty()) model = "google/gemini-2.5-flash";

    std::string systemPrompt = 
        "Senga Whisper orqali olingan o'zbek tilidagi gap beriladi. "
        "Unda imloviy yoki shevaga oid g'alati xatolar bo'lishi mumkin (masalan 'Menge logglarini kursad' -> 'Menga loglarni ko'rsat'). "
        "Sen faqat va faqat xatosi to'g'rilangan toza matnni qaytar. Hech qanday izoh, qo'shimcha so'z qo'shma.";

    nlohmann::json payload;
    payload["model"] = model;
    payload["messages"] = nlohmann::json::array();
    payload["messages"].push_back({{"role", "system"}, {"content", systemPrompt}});
    payload["messages"].push_back({{"role", "user"}, {"content", rawWhisperText}});

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
    return rawWhisperText;
}

} // namespace command
} // namespace liil
