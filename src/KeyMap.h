#pragma once

#include <cstdint>
#include <filesystem>
#include <qnamespace.h>
#include <string>
#include <vector>

// Bitmask of keyboard modifiers
enum KeyModifier : std::uint8_t {
    Mod_None  = 0,
    Mod_Ctrl  = 1 << 0,
    Mod_Shift = 1 << 1,
    Mod_Alt   = 1 << 2,
    Mod_Meta  = 1 << 3
};

struct KeyBindingEntry {
    std::string widget;     // e.g. "MainFrame", "CommandLine", "SearchEdit"
    std::string key;        // e.g. "F5", "Tab", "NumPlus", "Left"
    std::uint8_t modifiers; // bitmask from KeyModifier
    std::string handler;    // e.g. "onCopy", "onMove"
};

class KeyMap {
private:
    static const std::vector<std::pair<Qt::Key, std::string>> keys_;
public:
    // Parse keys TOML file and fill internal bindings.
    // Throws std::runtime_error on parse errors.
    void load(const std::filesystem::path& filePath);

    // Access all parsed entries.
    [[nodiscard]] const std::vector<KeyBindingEntry>& entries() const noexcept {
        return bindings_;
    }

    // Filtered view for a given widget name.
    [[nodiscard]] std::vector<KeyBindingEntry> entriesForWidget(const std::string& widgetName) const;
    // Collect all unique handler names in alphabetical order
    [[nodiscard]] std::vector<std::string> allHandlers() const;
    void printHandlerDeclarations() const;
    void printHandlerDefinitions() const;
    void printInvokeCalls() const;
private:
    std::vector<KeyBindingEntry> bindings_;
};
