#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <vector>
#include <string>

struct MqttSource {
    std::string host;
    int port;
    std::string username;
    std::string password;
    std::string topic;
};

struct LavinMQConfig {
    std::string host;
    int port = 5672;
    std::string username;
    std::string password;
    std::string vhost = "/";
    std::string exchange = "amq.topic";
    std::string routing_key = "things.data";
};

/**
 * Loads MQTT sources from a JSON configuration file.
 *
 * @param configPath Path to the JSON configuration file
 * @param sources Vector to store the loaded MQTT sources
 * @param lavinConfig Optional pointer to store LavinMQ configuration
 * @return true if sources were successfully loaded, false otherwise
 */
bool loadMqttSources(const std::string& configPath, std::vector<MqttSource>& sources, LavinMQConfig* lavinConfig = nullptr);

#endif // CONFIG_LOADER_H
