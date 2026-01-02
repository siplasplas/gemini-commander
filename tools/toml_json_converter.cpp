#include <iostream>
#include <fstream>
#include <string>
#include <toml++/toml.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

json toml_node_to_json(const toml::node& node);

json toml_table_to_json(const toml::table& tbl)
{
    json j = json::object();
    for (auto&& [k, v] : tbl) {
        j[std::string(k.str())] = toml_node_to_json(v);
    }
    return j;
}

json toml_array_to_json(const toml::array& arr)
{
    json j = json::array();
    for (auto&& elem : arr) {
        j.push_back(toml_node_to_json(elem));
    }
    return j;
}

template <typename T>
std::string toml_chrono_to_string(const T& value)
{
    std::ostringstream oss;
    oss << value; // używa operator<< z toml++
    return oss.str();
}

json toml_node_to_json(const toml::node& node)
{
    if (node.is_table())
        return toml_table_to_json(*node.as_table());
    if (node.is_array())
        return toml_array_to_json(*node.as_array());

    if (auto v = node.as_integer())
        return json(v->get());
    if (auto v = node.as_floating_point())
        return json(v->get());
    if (auto v = node.as_boolean())
        return json(v->get());
    if (auto v = node.as_string())
        return json(v->get());

    if (auto v = node.as_date())
        return json(toml_chrono_to_string(*v));
    if (auto v = node.as_time())
        return json(toml_chrono_to_string(*v));
    if (auto v = node.as_date_time())
        return json(toml_chrono_to_string(*v));

    return nullptr;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Usage:\n"
                  << "  toml_json_converter to-json  input.toml > out.json\n"
                  << "  toml_json_converter to-toml  input.json > out.toml (TODO)\n";
        return 1;
    }

    std::string mode = argv[1];
    std::string path = argv[2];

    try {
        if (mode == "to-json") {
            auto tbl = toml::parse_file(path);
            json j = toml_table_to_json(tbl);
            std::cout << j.dump(4) << "\n";
        }
        else if (mode == "to-toml") {
            // TODO: wczytaj JSON i zbuduj toml::table,
            // np. funkcją json_to_toml_table(const json&).
            std::cerr << "to-toml not implemented yet\n";
            return 2;
        }
        else {
            std::cerr << "Unknown mode: " << mode << "\n";
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
