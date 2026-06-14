#pragma once
#include <string>

namespace liil {
namespace command {

class AIEngine {
public:
    // Takes user input, sends it to OpenRouter AI via an async thread,
    // and invokes CommandEngine::ExecuteAction on the parsed JSON.
    static void ProcessCommand(const std::string& input);

    // Whisper transkripsiyasidan keyingi xatoliklarni to'g'rilash (Normalization Layer)
    static std::string CorrectText(const std::string& rawWhisperText);

private:};

} // namespace command
} // namespace liil
