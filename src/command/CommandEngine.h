#pragma once
#include <string>
#include <unordered_map>
#include <functional>

namespace liil {
namespace command {

class CommandEngine {
public:
    static CommandEngine& GetInstance();
    
    // Delete copy and move constructors
    CommandEngine(const CommandEngine&) = delete;
    CommandEngine& operator=(const CommandEngine&) = delete;
    CommandEngine(CommandEngine&&) = delete;
    CommandEngine& operator=(CommandEngine&&) = delete;

    void Initialize();
    void Execute(const std::string& input);

private:
    CommandEngine() = default;
    ~CommandEngine() = default;
    
    std::string NormalizeInput(const std::string& input);
    void RegisterCommand(const std::string& trigger, std::function<void()> action);
    
    std::unordered_map<std::string, std::function<void()>> m_commands;
};

} // namespace command
} // namespace liil
