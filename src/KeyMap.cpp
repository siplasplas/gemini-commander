#include "KeyMap.h"
#include <toml++/toml.h>
#include <stdexcept>
#include <algorithm>
#include <cctype>

#include "FilePaneWidget.h"

namespace {

inline std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Map modifier token (lower-case) to bitmask
inline std::uint8_t modifierFromToken(const std::string& tokenLower)
{
    if (tokenLower == "ctrl" || tokenLower == "control")
        return Mod_Ctrl;
    if (tokenLower == "shift")
        return Mod_Shift;
    if (tokenLower == "alt")
        return Mod_Alt;
    if (tokenLower == "meta" || tokenLower == "win" || tokenLower == "cmd")
        return Mod_Meta;

    return Mod_None;
}

// Simple split by '+'
std::vector<std::string> splitCombo(const std::string& combo)
{
    std::vector<std::string> result;
    std::string current;

    for (char ch : combo) {
        if (ch == '+') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty())
        result.push_back(current);

    return result;
}

// Parse combo string like "Ctrl+Shift+F5", "Alt+Enter", "NumPlus"
// into (key, modifiers).
//
// Rule:
// - Tokens recognized as modifiers go to modifier bitmask
// - All other tokens are considered key candidates
// - The last non-modifier token is used as key
//
// If no non-modifier token is found, throws std::runtime_error.
std::pair<std::string, std::uint8_t>
parseCombo(const std::string& combo)
{
    auto tokens = splitCombo(combo);
    if (tokens.empty()) {
        throw std::runtime_error("Empty combo string in key binding");
    }

    std::uint8_t mods = Mod_None;
    std::vector<std::string> keyTokens;

    for (auto& t : tokens) {
        auto begin = t.find_first_not_of(" \t");
        auto end   = t.find_last_not_of(" \t");
        if (begin == std::string::npos) {
            continue;
        }
        std::string trimmed = t.substr(begin, end - begin + 1);
        if (trimmed.empty())
            continue;

        std::string lower = toLower(trimmed);
        std::uint8_t m = modifierFromToken(lower);
        if (m != Mod_None) {
            mods |= m;
        } else {
            keyTokens.push_back(trimmed);
        }
    }

    if (keyTokens.empty()) {
        throw std::runtime_error("No key part found in combo: " + combo);
    }

    // Use the last non-modifier token as the key
    std::string key = keyTokens.back();

    return { key, mods };
}

} // namespace

void KeyMap::load(const std::filesystem::path& filePath)
{
    const std::string pathStr = filePath.string();

    // Detect Qt resource path
    const bool isQtResource = (pathStr.rfind(":/", 0) == 0);

    toml::table tbl;

    if (isQtResource) {
        // --- Qt resource ---
        QFile f(QString::fromStdString(pathStr));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            throw std::runtime_error("Cannot open Qt resource: " + pathStr);
        }

        QByteArray data = f.readAll();
        std::string text = data.toStdString();

        try {
            tbl = toml::parse(text);
        } catch (const toml::parse_error& err) {
            std::string msg = "Failed to parse TOML (Qt resource): ";
            msg += err.description();
            throw std::runtime_error(msg);
        }

    } else {
        // --- Normal filesystem path ---
        try {
            tbl = toml::parse_file(pathStr);
        } catch (const toml::parse_error& err) {
            std::string msg = "Failed to parse TOML file: ";
            msg += err.description();
            if (err.source().path) {
                msg += " at ";
                msg += *err.source().path->c_str();
            }
            throw std::runtime_error(msg);
        }
    }

    // And now parse `tbl` normally
    bindings_.clear();

    for (auto&& [widgetName, node] : tbl) {
        if (!node.is_table()) continue;

        auto* widgetTable = node.as_table();
        if (!widgetTable) continue;

        auto keysNode = widgetTable->get("keys");
        if (!keysNode || !keysNode->is_array()) continue;

        auto* arr = keysNode->as_array();
        if (!arr) continue;

        for (auto&& item : *arr) {
            if (!item.is_table()) continue;
            auto* keyTable = item.as_table();
            if (!keyTable) continue;

            auto comboNode   = keyTable->get("combo");
            auto handlerNode = keyTable->get("handler");
            if (!comboNode || !handlerNode) continue;

            auto comboOpt   = comboNode->value<std::string>();
            auto handlerOpt = handlerNode->value<std::string>();
            if (!comboOpt || !handlerOpt) continue;

            auto [key, mods] = parseCombo(*comboOpt);

            KeyBindingEntry e;
            e.widget    = widgetName.str();
            e.key       = key;
            e.modifiers = mods;
            e.handler   = *handlerOpt;

            bindings_.push_back(std::move(e));
        }
    }
}

std::vector<KeyBindingEntry>
KeyMap::entriesForWidget(const std::string& widgetName) const
{
    std::vector<KeyBindingEntry> out;
    out.reserve(bindings_.size());

    for (const auto& e : bindings_) {
        if (e.widget == widgetName) {
            out.push_back(e);
        }
    }

    return out;
}
