#include "ChatSystem.h"
#include "../Time/DayNightController.h"
#include "../Time/TimeManager.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iomanip>

// Global instance
ChatSystem* g_chatSystem = nullptr;

ChatSystem::ChatSystem()
    : m_maxMessages(100)
    , m_inputActive(false)
    , m_shouldRender(true)
    , m_historyIndex(-1)
    , m_suggestionIndex(0)
    , m_showTimestamps(true)
    , m_showDebug(false)
    , m_totalTime(0.0f)
{
    registerBuiltinCommands();
    addSystemMessage("Chat system initialized. Type /help for commands.");
}

void ChatSystem::update(float deltaTime) {
    m_totalTime += deltaTime;
    trimMessage();
}

void ChatSystem::clear() {
    m_messages.clear();
    addSystemMessage("Chat cleared.");
}

void ChatSystem::addMessage(const std::string& message, MessageType type, const std::string& sender) {
    if (type == MessageType::DEBUG && !m_showDebug) {
        return; // Skip debug messages if disabled
    }
    
    m_messages.emplace_back(message, type, m_totalTime, sender);
    trimMessage();
}

void ChatSystem::addSystemMessage(const std::string& message) {
    addMessage("[SYSTEM] " + message, MessageType::SYSTEM);
}

void ChatSystem::addErrorMessage(const std::string& message) {
    addMessage("[ERROR] " + message, MessageType::MSG_ERROR);
}

void ChatSystem::addDebugMessage(const std::string& message) {
    addMessage("[DEBUG] " + message, MessageType::DEBUG);
}

void ChatSystem::processInput(const std::string& input) {
    if (input.empty()) return;
    
    // Add to history
    addToHistory(input);
    
    // Check if it's a command (starts with /)
    if (input[0] == '/') {
        if (!executeCommand(input.substr(1))) {
            addErrorMessage("Unknown command: " + input);
        }
    } else {
        // Regular chat message
        addMessage(input, MessageType::CHAT, "Player");
    }
    
    clearInput();
}

bool ChatSystem::isInputActive() const {
    return m_inputActive;
}

void ChatSystem::setInputActive(bool active) {
    m_inputActive = active;
    if (active) {
        m_historyIndex = -1; // Reset history navigation
    }
}

const std::string& ChatSystem::getCurrentInput() const {
    return m_currentInput;
}

void ChatSystem::setCurrentInput(const std::string& input) {
    m_currentInput = input;
}

void ChatSystem::appendToInput(char c) {
    if (c >= 32 && c <= 126) { // Printable ASCII
        m_currentInput += c;
    }
}

void ChatSystem::backspaceInput() {
    if (!m_currentInput.empty()) {
        m_currentInput.pop_back();
    }
}

void ChatSystem::clearInput() {
    m_currentInput.clear();
}

const std::vector<ChatSystem::ChatMessage>& ChatSystem::getMessages() const {
    return m_messages;
}

std::vector<ChatSystem::ChatMessage> ChatSystem::getRecentMessages(int count) const {
    if (count >= static_cast<int>(m_messages.size())) {
        return m_messages;
    }
    
    return std::vector<ChatMessage>(m_messages.end() - count, m_messages.end());
}

std::vector<ChatSystem::ChatMessage> ChatSystem::getMessagesByType(MessageType type) const {
    std::vector<ChatMessage> filtered;
    for (const auto& msg : m_messages) {
        if (msg.type == type) {
            filtered.push_back(msg);
        }
    }
    return filtered;
}

void ChatSystem::render() {
    if (!m_inputActive && m_messages.empty()) {
        return;
    }
    
    // Use ImGui for chat rendering instead of legacy OpenGL
    ImGui::SetNextWindowPos(ImVec2(10, ImGui::GetIO().DisplaySize.y - 200), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(600, 150), ImGuiCond_Always);
    
    // Chat window
    if (m_inputActive || !m_messages.empty()) {
        ImGui::Begin("Chat", nullptr, 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse);
        
        // Display recent messages
        ImGui::BeginChild("ChatMessages", ImVec2(0, 100), true);
        
        // Show last 5 messages
        int startIdx = std::max(0, static_cast<int>(m_messages.size()) - 5);
        for (int i = startIdx; i < static_cast<int>(m_messages.size()); i++) {
            const auto& msg = m_messages[i];
            
            // Color messages by type
            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
            switch (msg.type) {
                case MessageType::SYSTEM: color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); break; // Green
                case MessageType::MSG_ERROR: color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); break; // Red
                case MessageType::DEBUG: color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); break; // Yellow
                case MessageType::COMMAND: color = ImVec4(0.0f, 1.0f, 1.0f, 1.0f); break; // Cyan
                default: break;
            }
            
            ImGui::TextColored(color, "%s", msg.text.c_str());
        }
        
        ImGui::EndChild();
        
        // Input field (only when active)
        if (m_inputActive) {
            ImGui::Separator();
            ImGui::Text(">");
            ImGui::SameLine();
            
            // Input text field
            char inputBuffer[256];
            strncpy_s(inputBuffer, m_currentInput.c_str(), sizeof(inputBuffer) - 1);
            inputBuffer[sizeof(inputBuffer) - 1] = '\0';
            
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##ChatInput", inputBuffer, sizeof(inputBuffer), 
                ImGuiInputTextFlags_EnterReturnsTrue)) {
                // Process input when Enter is pressed
                std::string input(inputBuffer);
                if (!input.empty()) {
                    processInput(input);
                }
                setInputActive(false);
            }
            
            // Update current input from buffer
            m_currentInput = inputBuffer;
        }
        
        ImGui::End();
    }
}

void ChatSystem::registerCommand(const std::string& name, const std::string& description,
                                std::function<void(const std::vector<std::string>&)> handler,
                                int minArgs, int maxArgs) {
    m_commands.emplace(toLowerCase(name), Command(name, description, handler, minArgs, maxArgs));
}

void ChatSystem::unregisterCommand(const std::string& name) {
    m_commands.erase(toLowerCase(name));
}

bool ChatSystem::executeCommand(const std::string& commandLine) {
    std::vector<std::string> parts = splitString(commandLine);
    if (parts.empty()) return false;
    
    std::string commandName = toLowerCase(parts[0]);
    auto it = m_commands.find(commandName);
    
    if (it == m_commands.end()) {
        return false;
    }
    
    const Command& cmd = it->second;
    std::vector<std::string> args(parts.begin() + 1, parts.end());
    
    // Check argument count
    if (static_cast<int>(args.size()) < cmd.minArgs) {
        addErrorMessage("Command '" + cmd.name + "' requires at least " + 
                       std::to_string(cmd.minArgs) + " arguments");
        return true;
    }
    
    if (cmd.maxArgs >= 0 && static_cast<int>(args.size()) > cmd.maxArgs) {
        addErrorMessage("Command '" + cmd.name + "' accepts at most " + 
                       std::to_string(cmd.maxArgs) + " arguments");
        return true;
    }
    
    // Execute command
    try {
        cmd.handler(args);
    } catch (const std::exception& e) {
        addErrorMessage("Error executing command '" + cmd.name + "': " + e.what());
    }
    
    return true;
}

std::vector<std::string> ChatSystem::getAvailableCommands() const {
    std::vector<std::string> commands;
    for (const auto& pair : m_commands) {
        commands.push_back(pair.second.name);
    }
    std::sort(commands.begin(), commands.end());
    return commands;
}

std::string ChatSystem::getCommandHelp(const std::string& command) const {
    auto it = m_commands.find(toLowerCase(command));
    if (it != m_commands.end()) {
        return it->second.description;
    }
    return "Unknown command";
}

void ChatSystem::setMaxMessages(int max) {
    m_maxMessages = std::max(10, max);
    trimMessage();
}

int ChatSystem::getMaxMessages() const {
    return m_maxMessages;
}

void ChatSystem::setShowTimestamps(bool show) {
    m_showTimestamps = show;
}

bool ChatSystem::getShowTimestamps() const {
    return m_showTimestamps;
}

void ChatSystem::setShowDebug(bool show) {
    m_showDebug = show;
}

bool ChatSystem::getShowDebug() const {
    return m_showDebug;
}

std::vector<std::string> ChatSystem::getCommandSuggestions(const std::string& partial) const {
    std::string lowerPartial = toLowerCase(partial);
    std::vector<std::string> suggestions;
    
    for (const auto& pair : m_commands) {
        if (pair.second.name.substr(0, lowerPartial.length()) == lowerPartial) {
            suggestions.push_back(pair.second.name);
        }
    }
    
    std::sort(suggestions.begin(), suggestions.end());
    return suggestions;
}

std::string ChatSystem::getNextSuggestion(const std::string& partial) {
    m_lastSuggestions = getCommandSuggestions(partial);
    if (m_lastSuggestions.empty()) return partial;
    
    std::string result = m_lastSuggestions[m_suggestionIndex % m_lastSuggestions.size()];
    m_suggestionIndex++;
    return result;
}

void ChatSystem::addToHistory(const std::string& command) {
    // Don't add duplicates or empty commands
    if (command.empty() || (!m_commandHistory.empty() && m_commandHistory.back() == command)) {
        return;
    }
    
    m_commandHistory.push_back(command);
    if (m_commandHistory.size() > 50) { // Limit history size
        m_commandHistory.erase(m_commandHistory.begin());
    }
}

std::string ChatSystem::getPreviousHistoryItem() {
    if (m_commandHistory.empty()) return "";
    
    if (m_historyIndex == -1) {
        m_historyIndex = static_cast<int>(m_commandHistory.size()) - 1;
    } else if (m_historyIndex > 0) {
        m_historyIndex--;
    }
    
    return m_commandHistory[m_historyIndex];
}

std::string ChatSystem::getNextHistoryItem() {
    if (m_commandHistory.empty() || m_historyIndex == -1) return "";
    
    m_historyIndex++;
    if (m_historyIndex >= static_cast<int>(m_commandHistory.size())) {
        m_historyIndex = -1;
        return "";
    }
    
    return m_commandHistory[m_historyIndex];
}

void ChatSystem::clearHistory() {
    m_commandHistory.clear();
    m_historyIndex = -1;
}

bool ChatSystem::shouldRender() const {
    return m_shouldRender;
}

void ChatSystem::setShouldRender(bool render) {
    m_shouldRender = render;
}

void ChatSystem::registerBuiltinCommands() {
    registerCommand("help", "Show available commands or help for specific command", 
                   [this](const std::vector<std::string>& args) { cmdHelp(args); }, 0, 1);
    
    registerCommand("clear", "Clear chat messages", 
                   [this](const std::vector<std::string>& args) { cmdClear(args); }, 0, 0);
    
    registerCommand("time", "Get/set current time of day", 
                   [this](const std::vector<std::string>& args) { cmdTime(args); }, 0, 2);
    
    registerCommand("timespeed", "Get/set time speed multiplier", 
                   [this](const std::vector<std::string>& args) { cmdTimeSpeed(args); }, 0, 1);
    
    registerCommand("weather", "Control weather effects", 
                   [this](const std::vector<std::string>& args) { cmdWeather(args); }, 0, 2);
    
    registerCommand("debug", "Toggle debug mode for day/night cycle", 
                   [this](const std::vector<std::string>& args) { cmdDebug(args); }, 0, 1);
    
    registerCommand("quit", "Exit the application", 
                   [this](const std::vector<std::string>& args) { cmdQuit(args); }, 0, 0);
}

void ChatSystem::cmdHelp(const std::vector<std::string>& args) {
    if (args.empty()) {
        addMessage("Available commands:", MessageType::COMMAND);
        auto commands = getAvailableCommands();
        for (const auto& cmd : commands) {
            addMessage("  /" + cmd + " - " + getCommandHelp(cmd), MessageType::COMMAND);
        }
        addMessage("Type /help <command> for more details", MessageType::COMMAND);
    } else {
        std::string help = getCommandHelp(args[0]);
        addMessage("/" + args[0] + " - " + help, MessageType::COMMAND);
    }
}

void ChatSystem::cmdClear(const std::vector<std::string>& args) {
    clear();
}

void ChatSystem::cmdTime(const std::vector<std::string>& args) {
    if (!g_dayNightCycle) {
        addErrorMessage("Day/night cycle not available");
        return;
    }
    
    if (args.empty()) {
        // Show current time
        int hours, minutes;
        g_dayNightCycle->getTime(hours, minutes);
        std::stringstream ss;
        ss << "Current time: " << std::setfill('0') << std::setw(2) << hours 
           << ":" << std::setw(2) << minutes << " (" << g_dayNightCycle->getCurrentPeriodName() << ")";
        addMessage(ss.str(), MessageType::COMMAND);
    } else if (args.size() == 1) {
        // Set time (hours only)
        try {
            int hours = std::stoi(args[0]);
            if (hours >= 0 && hours < 24) {
                g_dayNightCycle->setTime(hours, 0);
                addMessage("Time set to " + std::to_string(hours) + ":00", MessageType::COMMAND);
            } else {
                addErrorMessage("Hours must be between 0 and 23");
            }
        } catch (const std::exception&) {
            addErrorMessage("Invalid hour value");
        }
    } else {
        // Set time (hours and minutes)
        try {
            int hours = std::stoi(args[0]);
            int minutes = std::stoi(args[1]);
            if (hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60) {
                g_dayNightCycle->setTime(hours, minutes);
                std::stringstream ss;
                ss << "Time set to " << std::setfill('0') << std::setw(2) << hours 
                   << ":" << std::setw(2) << minutes;
                addMessage(ss.str(), MessageType::COMMAND);
            } else {
                addErrorMessage("Invalid time values (hours: 0-23, minutes: 0-59)");
            }
        } catch (const std::exception&) {
            addErrorMessage("Invalid time values");
        }
    }
}

void ChatSystem::cmdTimeSpeed(const std::vector<std::string>& args) {
    if (!g_dayNightCycle) {
        addErrorMessage("Day/night cycle not available");
        return;
    }
    
    if (args.empty()) {
        float speed = g_dayNightCycle->getTimeSpeed();
        addMessage("Time speed: " + std::to_string(speed) + "x", MessageType::COMMAND);
    } else {
        try {
            float speed = std::stof(args[0]);
            if (speed >= 0.0f && speed <= 100.0f) {
                g_dayNightCycle->setTimeSpeed(speed);
                addMessage("Time speed set to " + std::to_string(speed) + "x", MessageType::COMMAND);
            } else {
                addErrorMessage("Time speed must be between 0.0 and 100.0");
            }
        } catch (const std::exception&) {
            addErrorMessage("Invalid time speed value");
        }
    }
}

void ChatSystem::cmdWeather(const std::vector<std::string>& args) {
    if (!g_dayNightCycle) {
        addErrorMessage("Day/night cycle not available");
        return;
    }
    
    if (args.empty()) {
        addMessage("Usage: /weather <clouds> [precipitation]", MessageType::COMMAND);
        addMessage("  clouds: 0.0-1.0 (cloud cover)", MessageType::COMMAND);
        addMessage("  precipitation: 0.0-1.0 (rain/snow)", MessageType::COMMAND);
    } else {
        try {
            float clouds = std::stof(args[0]);
            float precipitation = args.size() > 1 ? std::stof(args[1]) : 0.0f;
            
            if (clouds >= 0.0f && clouds <= 1.0f && precipitation >= 0.0f && precipitation <= 1.0f) {
                g_dayNightCycle->setWeatherInfluence(clouds, precipitation);
                addMessage("Weather set: clouds=" + std::to_string(clouds) + 
                          ", precipitation=" + std::to_string(precipitation), MessageType::COMMAND);
            } else {
                addErrorMessage("Weather values must be between 0.0 and 1.0");
            }
        } catch (const std::exception&) {
            addErrorMessage("Invalid weather values");
        }
    }
}

void ChatSystem::cmdDebug(const std::vector<std::string>& args) {
    if (!g_dayNightCycle) {
        addErrorMessage("Day/night cycle not available");
        return;
    }
    
    if (args.empty()) {
        // Toggle debug mode
        bool currentMode = true; // We'll need to add getter to DayNightCycle
        g_dayNightCycle->setDebugMode(!currentMode);
        addMessage("Debug mode toggled", MessageType::COMMAND);
    } else {
        std::string mode = toLowerCase(args[0]);
        if (mode == "on" || mode == "true" || mode == "1") {
            g_dayNightCycle->setDebugMode(true);
            addMessage("Debug mode enabled", MessageType::COMMAND);
        } else if (mode == "off" || mode == "false" || mode == "0") {
            g_dayNightCycle->setDebugMode(false);
            addMessage("Debug mode disabled", MessageType::COMMAND);
        } else {
            addErrorMessage("Use 'on' or 'off'");
        }
    }
}

void ChatSystem::cmdQuit(const std::vector<std::string>& args) {
    addSystemMessage("Quit command received - this would exit the application");
    // Note: Actual quit implementation would need to be handled by the main application
}

std::vector<std::string> ChatSystem::splitString(const std::string& str, char delimiter) const {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

std::string ChatSystem::joinStrings(const std::vector<std::string>& strings, const std::string& separator) const {
    if (strings.empty()) return "";
    
    std::string result = strings[0];
    for (size_t i = 1; i < strings.size(); ++i) {
        result += separator + strings[i];
    }
    return result;
}

std::string ChatSystem::toLowerCase(const std::string& str) const {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

void ChatSystem::trimMessage() {
    while (static_cast<int>(m_messages.size()) > m_maxMessages) {
        m_messages.erase(m_messages.begin());
    }
}
