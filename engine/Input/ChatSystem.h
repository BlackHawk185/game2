#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <queue>

/**
 * @brief Simple chat and command system for the MMORPG engine
 * 
 * Provides in-game chat functionality and a command system for debugging
 * and administrative functions. Integrates with day/night cycle and other systems.
 */
class ChatSystem {
public:
    enum class MessageType {
        CHAT,           // Player chat messages
        SYSTEM,         // System notifications
        COMMAND,        // Command output
        MSG_ERROR,      // Error messages (renamed to avoid Windows ERROR macro)
        DEBUG           // Debug information
    };

    struct ChatMessage {
        std::string text;
        MessageType type;
        float timestamp;
        std::string sender;     // Empty for system messages
        
        ChatMessage(const std::string& msg, MessageType msgType, float time = 0.0f, const std::string& from = "")
            : text(msg), type(msgType), timestamp(time), sender(from) {}
    };

    struct Command {
        std::string name;
        std::string description;
        std::function<void(const std::vector<std::string>&)> handler;
        int minArgs;
        int maxArgs;    // -1 for unlimited
        
        Command(const std::string& cmdName, const std::string& desc, 
                std::function<void(const std::vector<std::string>&)> func,
                int min = 0, int max = -1)
            : name(cmdName), description(desc), handler(func), minArgs(min), maxArgs(max) {}
    };

    ChatSystem();
    ~ChatSystem() = default;

    // Core functionality
    void update(float deltaTime);
    void render();
    void clear();

    // Message handling
    void addMessage(const std::string& message, MessageType type = MessageType::CHAT, 
                   const std::string& sender = "");
    void addSystemMessage(const std::string& message);
    void addErrorMessage(const std::string& message);
    void addDebugMessage(const std::string& message);

    // Chat input
    void processInput(const std::string& input);
    bool isInputActive() const;
    void setInputActive(bool active);
    
    const std::string& getCurrentInput() const;
    void setCurrentInput(const std::string& input);
    void appendToInput(char c);
    void backspaceInput();
    void clearInput();

    // Message retrieval
    const std::vector<ChatMessage>& getMessages() const;
    std::vector<ChatMessage> getRecentMessages(int count) const;
    std::vector<ChatMessage> getMessagesByType(MessageType type) const;

    // Command system
    void registerCommand(const std::string& name, const std::string& description,
                        std::function<void(const std::vector<std::string>&)> handler,
                        int minArgs = 0, int maxArgs = -1);
    void unregisterCommand(const std::string& name);
    bool executeCommand(const std::string& commandLine);
    std::vector<std::string> getAvailableCommands() const;
    std::string getCommandHelp(const std::string& command) const;

    // Settings
    void setMaxMessages(int max);
    int getMaxMessages() const;
    
    void setShowTimestamps(bool show);
    bool getShowTimestamps() const;
    
    void setShowDebug(bool show);
    bool getShowDebug() const;

    // Auto-completion
    std::vector<std::string> getCommandSuggestions(const std::string& partial) const;
    std::string getNextSuggestion(const std::string& partial);

    // History
    void addToHistory(const std::string& command);
    std::string getPreviousHistoryItem();
    std::string getNextHistoryItem();
    void clearHistory();

    // Rendering info (for UI integration)
    bool shouldRender() const;
    void setShouldRender(bool render);

private:
    // Message storage
    std::vector<ChatMessage> m_messages;
    int m_maxMessages;
    
    // Input state
    std::string m_currentInput;
    bool m_inputActive;
    bool m_shouldRender;
    
    // Commands
    std::unordered_map<std::string, Command> m_commands;
    
    // History
    std::vector<std::string> m_commandHistory;
    int m_historyIndex;
    
    // Auto-completion
    mutable std::vector<std::string> m_lastSuggestions;
    mutable int m_suggestionIndex;
    
    // Settings
    bool m_showTimestamps;
    bool m_showDebug;
    float m_totalTime;
    
    // Built-in commands
    void registerBuiltinCommands();
    
    // Command implementations
    void cmdHelp(const std::vector<std::string>& args);
    void cmdClear(const std::vector<std::string>& args);
    void cmdTime(const std::vector<std::string>& args);
    void cmdTimeSpeed(const std::vector<std::string>& args);
    void cmdWeather(const std::vector<std::string>& args);
    void cmdTeleport(const std::vector<std::string>& args);
    void cmdDebug(const std::vector<std::string>& args);
    void cmdQuit(const std::vector<std::string>& args);
    
    // Utility functions
    std::vector<std::string> splitString(const std::string& str, char delimiter = ' ') const;
    std::string joinStrings(const std::vector<std::string>& strings, const std::string& separator = " ") const;
    std::string toLowerCase(const std::string& str) const;
    void trimMessage();  // Remove old messages if over limit
};

// Global chat system instance
extern ChatSystem* g_chatSystem;

// Convenience macros
#define CHAT_MSG(msg) if(g_chatSystem) g_chatSystem->addMessage(msg)
#define SYSTEM_MSG(msg) if(g_chatSystem) g_chatSystem->addSystemMessage(msg)
#define ERROR_MSG(msg) if(g_chatSystem) g_chatSystem->addErrorMessage(msg)
#define DEBUG_MSG(msg) if(g_chatSystem) g_chatSystem->addDebugMessage(msg)
