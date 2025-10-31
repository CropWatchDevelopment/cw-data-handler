#include "config_loader.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <simdjson/dom.h>

using namespace simdjson;

bool loadMqttSources(const std::string& configPath, std::vector<MqttSource>& sources, LavinMQConfig* lavinConfig) {
    std::ifstream configFile;
    const std::filesystem::path configName(configPath);
    std::vector<std::filesystem::path> candidates;

    const auto add_candidate = [&candidates](const std::filesystem::path& candidate) {
        if (candidate.empty()) {
            return;
        }
        if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()) {
            candidates.push_back(candidate);
        }
    };

    const auto enqueue_search_paths = [&](const std::filesystem::path& base_dir) {
        if (base_dir.empty()) {
            return;
        }
        auto dir = base_dir;
        for (int depth = 0; depth < 5 && !dir.empty(); ++depth) {
            add_candidate(dir / configName);
            add_candidate(dir / "build" / configName);
            add_candidate(dir / "build-debug" / configName);
            dir = dir.parent_path();
        }
    };

    if (configName.is_absolute()) {
        add_candidate(configName);
    } else {
        add_candidate(configName);
        const auto cwd = std::filesystem::current_path();
        enqueue_search_paths(cwd);

        std::error_code exeEc;
        auto exeTarget = std::filesystem::read_symlink("/proc/self/exe", exeEc);
        if (!exeEc) {
            std::error_code canonEc;
            auto exeCanonical = std::filesystem::weakly_canonical(exeTarget, canonEc);
            const auto& origin = canonEc ? exeTarget : exeCanonical;
            enqueue_search_paths(origin.parent_path());
        }
    }

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (!candidate.empty() && std::filesystem::exists(candidate, ec) && !ec) {
            configFile.open(candidate);
            if (configFile.is_open()) {
                break;
            }
        }
    }

    if (!configFile.is_open()) {
        std::cerr << "Warning: Cannot open config file: " << configPath << ". Using default sources." << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << configFile.rdbuf();
    std::string configJson = buffer.str();

    dom::parser parser;
    auto doc_result = parser.parse(configJson);
    if (doc_result.error()) {
        std::cerr << "Error parsing config JSON: " << doc_result.error() << ". Using default sources." << std::endl;
        return false;
    }

    auto doc = doc_result.value();

    LavinMQConfig lavin_settings;
    bool lavin_ok = true;
    auto lavin_result = doc["lavinmq"];
    if (lavin_result.error() || !lavin_result.is_object()) {
        std::cerr << "Error: lavinmq configuration not found in config. Using default LavinMQ settings is not supported." << std::endl;
        lavin_ok = false;
    } else {
        auto lavin_obj = lavin_result.get_object();

        try {
            auto host_result = lavin_obj["host"];
            if (!host_result.error() && host_result.is_string()) {
                lavin_settings.host = std::string(host_result);
            } else {
                lavin_ok = false;
            }
        } catch (...) {
            lavin_ok = false;
        }

        try {
            auto port_result = lavin_obj["port"];
            if (!port_result.error()) {
                int64_t port_val = port_result;
                lavin_settings.port = static_cast<int>(port_val);
            } else {
                lavin_ok = false;
            }
        } catch (...) {
            lavin_ok = false;
        }

        try {
            auto user_result = lavin_obj["username"];
            if (!user_result.error() && user_result.is_string()) {
                lavin_settings.username = std::string(user_result);
            } else {
                lavin_ok = false;
            }
        } catch (...) {
            lavin_ok = false;
        }

        try {
            auto pass_result = lavin_obj["password"];
            if (!pass_result.error() && pass_result.is_string()) {
                lavin_settings.password = std::string(pass_result);
            } else {
                lavin_ok = false;
            }
        } catch (...) {
            lavin_ok = false;
        }

        try {
            auto vhost_result = lavin_obj["vhost"];
            if (!vhost_result.error() && vhost_result.is_string()) {
                lavin_settings.vhost = std::string(vhost_result);
            }
        } catch (...) {
            lavin_ok = false;
        }

        try {
            auto exchange_result = lavin_obj["exchange"];
            if (!exchange_result.error() && exchange_result.is_string()) {
                lavin_settings.exchange = std::string(exchange_result);
            }
        } catch (...) {
            lavin_ok = false;
        }

        try {
            auto routing_result = lavin_obj["routing_key"];
            if (!routing_result.error() && routing_result.is_string()) {
                lavin_settings.routing_key = std::string(routing_result);
            }
        } catch (...) {
            lavin_ok = false;
        }
    }

    // Load MQTT sources
    auto sources_result = doc["mqtt_sources"];
    if (sources_result.error()) {
        std::cerr << "Error: mqtt_sources not found in config. Using default sources." << std::endl;
        return false;
    }

    if (!sources_result.is_array()) {
        std::cerr << "Error: mqtt_sources is not an array. Using default sources." << std::endl;
        return false;
    }

    auto sources_array = sources_result.get_array();
    for (size_t i = 0; i < sources_array.size(); ++i) {
        auto source_result = sources_array.at(i);
        if (source_result.error()) continue;

        auto source = source_result.value();

        MqttSource mqtt_source;
        bool valid_source = true;

        try {
            auto src_host = source["host"];
            if (!src_host.error() && src_host.is_string()) {
                mqtt_source.host = std::string(src_host);
            } else {
                valid_source = false;
            }
        } catch (...) {
            valid_source = false;
        }

        try {
            auto src_port = source["port"];
            if (!src_port.error()) {
                int64_t port_val = src_port;
                mqtt_source.port = static_cast<int>(port_val);
            } else {
                valid_source = false;
            }
        } catch (...) {
            valid_source = false;
        }

        try {
            auto src_username = source["username"];
            if (!src_username.error() && src_username.is_string()) {
                mqtt_source.username = std::string(src_username);
            } else {
                valid_source = false;
            }
        } catch (...) {
            valid_source = false;
        }

        try {
            auto src_password = source["password"];
            if (!src_password.error() && src_password.is_string()) {
                mqtt_source.password = std::string(src_password);
            } else {
                valid_source = false;
            }
        } catch (...) {
            valid_source = false;
        }

        try {
            auto src_topic = source["topic"];
            if (!src_topic.error() && src_topic.is_string()) {
                mqtt_source.topic = std::string(src_topic);
            } else {
                valid_source = false;
            }
        } catch (...) {
            valid_source = false;
        }

        if (valid_source) {
            sources.push_back(mqtt_source);
        }
    }

    if (sources.empty()) {
        std::cerr << "Warning: No valid MQTT sources found in config. Using default sources." << std::endl;
        return false;
    }

    if (lavin_ok && lavinConfig) {
        *lavinConfig = lavin_settings;
    }

    if (!lavin_ok) {
        std::cerr << "Error: LavinMQ configuration is invalid." << std::endl;
    }

    std::cout << "Loaded " << sources.size() << " MQTT sources from config." << std::endl;
    return lavin_ok;
}
