#pragma once

#include <MQTTClient.h>
#include <simdjson.h>

#include <string>
#include <string_view>
#include <vector>

struct MqttTopic
{
    std::string filter;
    int qos{0};
};

struct MqttConfig
{
    std::string name;
    std::string address;
    std::string username;
    std::string password;
    std::string client_id;
    std::string ca_cert;
    std::string client_cert;
    std::string client_key;
    int keep_alive{60};
    bool clean_session{true};
    bool use_tls{false};
    bool tls_insecure{false};
    std::vector<MqttTopic> topics;
};

class MqttClient
{
public:
    explicit MqttClient(MqttConfig cfg);
    ~MqttClient();

    MqttClient(const MqttClient &) = delete;
    MqttClient &operator=(const MqttClient &) = delete;
    MqttClient(MqttClient &&) noexcept;
    MqttClient &operator=(MqttClient &&) noexcept;

    bool connect();
    void disconnect();
    bool is_connected() const;

    const MqttConfig &config() const { return config_; }

    static std::vector<MqttConfig> ParseConfigs(const simdjson::dom::element &root);

private:
    std::atomic<bool> reconnecting_{false};
    mutable std::mutex client_mtx_;

    static void ConnectionLost(void *context, char *cause);
    static int MessageArrived(void *context, char *topic_name, int topic_length, MQTTClient_message *message);
    static void DeliveryComplete(void *context, MQTTClient_deliveryToken token);
    static std::string ErrorString(int rc);

    static std::string_view GetStringOr(const simdjson::dom::object &obj, std::string_view key, std::string_view fallback);
    static uint64_t GetUint64Or(const simdjson::dom::object &obj, std::string_view key, uint64_t fallback);
    static bool GetBoolOr(const simdjson::dom::object &obj, std::string_view key, bool fallback);
    static std::string ToString(std::string_view value);

    void reset_client();

    MqttConfig config_;
    MQTTClient client_{nullptr};
};
