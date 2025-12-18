#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <qnamespace.h>
#include <QString>
#include <string>
#include <vector>

struct KeyBindingEntry {
    std::string widget;     // e.g. "MainFrame", "CommandLine", "SearchEdit"
    std::string key;        // e.g. "F5", "Tab", "NumPlus", "Left"
    Qt::KeyboardModifiers modifiers; // KeyModifier
    std::string handler;    // e.g. "onCopy", "onMove"
};

// Key for fast lookup in bindings map
struct BindingKey {
    std::string widget;
    std::string key;
    Qt::KeyboardModifiers modifiers;

    bool operator<(const BindingKey& other) const {
        if (widget != other.widget) return widget < other.widget;
        if (key != other.key) return key < other.key;
        return modifiers < other.modifiers;
    }
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

    // Find handler for given key, modifiers, and widget name.
    // Returns handler name, "none", "default", or empty string if not found.
    // Supports LETTERS (A-Z) and DIGITS (0-9) pseudo-keys.
    [[nodiscard]] QString handlerFor(int qtKey, Qt::KeyboardModifiers mods, const QString& widgetName) const;

private:
    // Convert Qt::Key to string key name
    QString keyToString(int qtKey) const;
    std::vector<KeyBindingEntry> bindings_;
    std::map<BindingKey, std::string> bindingsMap_;  // Fast lookup map
};
