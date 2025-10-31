#include "MqttClient.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

namespace
{
    constexpr int kDefaultQosMin = 0;
    constexpr int kDefaultQosMax = 2;
} // namespace

MqttClient::MqttClient(MqttConfig cfg)
    : config_(std::move(cfg)) {}

MqttClient::~MqttClient()
{
    disconnect();
}

MqttClient::MqttClient(MqttClient &&other) noexcept
    : config_(std::move(other.config_)), client_(other.client_)
{
    other.client_ = nullptr;
}

MqttClient &MqttClient::operator=(MqttClient &&other) noexcept
{
    if (this != &other)
    {
        disconnect();
        config_ = std::move(other.config_);
        client_ = other.client_;
        other.client_ = nullptr;
    }
    return *this;
}

bool MqttClient::connect()
{
    std::lock_guard<std::mutex> lk(client_mtx_);

    if (client_)
        return true;

    int rc = MQTTClient_create(&client_, config_.address.c_str(),
                               config_.client_id.c_str(),
                               MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        std::cerr << "[MQTT] Failed to create client for '" << config_.name
                  << "': " << ErrorString(rc) << std::endl;
        reset_client();
        return false;
    }

    rc = MQTTClient_setCallbacks(client_, this, ConnectionLost, MessageArrived, DeliveryComplete);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        std::cerr << "[MQTT] Failed to set callbacks for '" << config_.name
                  << "': " << ErrorString(rc) << std::endl;
        reset_client();
        return false;
    }

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = config_.keep_alive; // e.g. 60
    conn_opts.cleansession = 0;                       // keep subscriptions on broker
    conn_opts.username = config_.username.empty() ? nullptr : config_.username.c_str();
    conn_opts.password = config_.password.empty() ? nullptr : config_.password.c_str();
    // Older Paho: only retryInterval exists
    conn_opts.retryInterval = 5; // seconds (broker retry if initial connect fails)

    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    if (config_.use_tls)
    {
        ssl_opts.enableServerCertAuth = config_.tls_insecure ? 0 : 1;
        ssl_opts.verify = config_.tls_insecure ? 0 : 1;
        ssl_opts.trustStore = config_.ca_cert.empty() ? nullptr : config_.ca_cert.c_str();
        ssl_opts.keyStore = config_.client_cert.empty() ? nullptr : config_.client_cert.c_str();
        ssl_opts.privateKey = config_.client_key.empty() ? nullptr : config_.client_key.c_str();
        conn_opts.ssl = &ssl_opts;
    }

    rc = MQTTClient_connect(client_, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        std::cerr << "[MQTT] Connect failed for '" << config_.name
                  << "': " << ErrorString(rc) << std::endl;
        reset_client();
        return false;
    }

    for (const auto &topic : config_.topics)
    {
        rc = MQTTClient_subscribe(client_, topic.filter.c_str(), topic.qos);
        if (rc != MQTTCLIENT_SUCCESS)
        {
            std::cerr << "[MQTT] Subscribe failed for '" << config_.name
                      << "' topic '" << topic.filter << "': " << ErrorString(rc) << std::endl;
        }
    }

    std::cout << "[MQTT] Connected " << config_.name << " -> " << config_.address << std::endl;
    return true;
}

bool MqttClient::is_connected() const
{
    std::lock_guard<std::mutex> lk(client_mtx_);
    return client_ && MQTTClient_isConnected(client_) == 1;
}

void MqttClient::ConnectionLost(void *context, char *cause)
{
    auto *self = static_cast<MqttClient *>(context);
    std::cerr << "[MQTT] Connection lost for '" << self->config_.name << "'"
              << (cause ? std::string{": "}.append(cause) : std::string{}) << std::endl;

    bool expected = false;
    if (!self->reconnecting_.compare_exchange_strong(expected, true))
    {
        // already reconnecting
        return;
    }

    std::thread([self]()
                {
        int attempt = 0;
        while (true) {
            {
                std::lock_guard<std::mutex> lk(self->client_mtx_);
                if (!self->client_) {
                    // client destroyed elsewhere; stop
                    self->reconnecting_ = false;
                    return;
                }
            }

            // Build fresh options each attempt
            MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
            conn_opts.keepAliveInterval = self->config_.keep_alive;
            conn_opts.cleansession     = 0;
            conn_opts.username         = self->config_.username.empty() ? nullptr : self->config_.username.c_str();
            conn_opts.password         = self->config_.password.empty() ? nullptr : self->config_.password.c_str();
            conn_opts.retryInterval    = 5;

            MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
            if (self->config_.use_tls) {
                ssl_opts.enableServerCertAuth = self->config_.tls_insecure ? 0 : 1;
                ssl_opts.verify               = self->config_.tls_insecure ? 0 : 1;
                ssl_opts.trustStore           = self->config_.ca_cert.empty() ? nullptr : self->config_.ca_cert.c_str();
                ssl_opts.keyStore             = self->config_.client_cert.empty() ? nullptr : self->config_.client_cert.c_str();
                ssl_opts.privateKey           = self->config_.client_key.empty() ? nullptr : self->config_.client_key.c_str();
                conn_opts.ssl = &ssl_opts;
            }

            int rc;
            {
                std::lock_guard<std::mutex> lk(self->client_mtx_);
                if (!self->client_) { self->reconnecting_ = false; return; }
                rc = MQTTClient_connect(self->client_, &conn_opts);
            }

            if (rc == MQTTCLIENT_SUCCESS) {
                std::cerr << "[MQTT] Reconnected '" << self->config_.name << "'\n";
                // Re-subscribe (broker should retain, but do it anyway)
                for (const auto &topic : self->config_.topics) {
                    int s = MQTTClient_subscribe(self->client_, topic.filter.c_str(), topic.qos);
                    if (s != MQTTCLIENT_SUCCESS) {
                        std::cerr << "[MQTT] Re-subscribe failed '" << topic.filter
                                  << "': " << self->ErrorString(s) << std::endl;
                    }
                }
                self->reconnecting_ = false;
                return;
            }

            ++attempt;
            int backoff = std::min(30, 1 << std::min(attempt, 5)); // 1,2,4,8,16,32→cap to 30
            std::cerr << "[MQTT] Reconnect failed (" << self->ErrorString(rc)
                      << "), retrying in " << backoff << "s\n";
            std::this_thread::sleep_for(std::chrono::seconds(backoff));
        } })
        .detach();
}

void MqttClient::disconnect()
{
    if (!client_)
    {
        return;
    }

    const int timeout_ms = 1000;
    MQTTClient_disconnect(client_, timeout_ms);
    MQTTClient_destroy(&client_);
    client_ = nullptr;
    std::cout << "[MQTT] Disconnected " << config_.name << std::endl;
}

int MqttClient::MessageArrived(void *context, char *topic_name, int topic_length, MQTTClient_message *message)
{
    auto *self = static_cast<MqttClient *>(context);
    const std::string topic(topic_name ? topic_name : "",
                            topic_length > 0 ? topic_length : (topic_name ? std::strlen(topic_name) : 0));
    const std::string payload(static_cast<char *>(message->payload), static_cast<size_t>(message->payloadlen));

    std::cout << "[MQTT] " << self->config_.name << " | " << topic << " | payload: " << payload << std::endl;

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topic_name);
    return 1;
}

void MqttClient::DeliveryComplete(void *, MQTTClient_deliveryToken)
{
    // No-op; this sample only subscribes.
}

std::string MqttClient::ErrorString(int rc)
{
    if (const char *err = MQTTClient_strerror(rc); err != nullptr)
    {
        return std::string(err);
    }
    std::ostringstream fallback;
    fallback << "code " << rc;
    return fallback.str();
}

std::string_view MqttClient::GetStringOr(const simdjson::dom::object &obj, std::string_view key, std::string_view fallback)
{
    if (auto maybe = obj[key].get_string(); !maybe.error())
    {
        return maybe.value();
    }
    return fallback;
}

uint64_t MqttClient::GetUint64Or(const simdjson::dom::object &obj, std::string_view key, uint64_t fallback)
{
    if (auto maybe = obj[key].get_uint64(); !maybe.error())
    {
        return maybe.value();
    }
    return fallback;
}

bool MqttClient::GetBoolOr(const simdjson::dom::object &obj, std::string_view key, bool fallback)
{
    if (auto maybe = obj[key].get_bool(); !maybe.error())
    {
        return maybe.value();
    }
    return fallback;
}

std::string MqttClient::ToString(std::string_view value)
{
    return std::string(value.begin(), value.end());
}

void MqttClient::reset_client()
{
    if (client_)
    {
        MQTTClient_destroy(&client_);
        client_ = nullptr;
    }
}

std::vector<MqttConfig> MqttClient::ParseConfigs(const simdjson::dom::element &root)
{
    std::vector<MqttConfig> result;
    auto array = root["mqtt_connections"].get_array();
    if (array.error())
    {
        return result;
    }

    for (const auto &entry : array.value())
    {
        auto entry_obj = entry.get_object();
        if (entry_obj.error())
        {
            continue;
        }

        const auto obj = entry_obj.value();
        MqttConfig cfg;
        cfg.name = ToString(GetStringOr(obj, "name", "mqtt-connection"));
        const auto host = GetStringOr(obj, "host", "localhost");
        const auto port = GetUint64Or(obj, "port", 1883);
        cfg.use_tls = GetBoolOr(obj, "use_tls", false);
        std::ostringstream address;
        address << (cfg.use_tls ? "ssl://" : "tcp://") << host << ':' << port;
        cfg.address = address.str();

        cfg.username = ToString(GetStringOr(obj, "username", ""));
        cfg.password = ToString(GetStringOr(obj, "password", ""));
        cfg.client_id = ToString(GetStringOr(obj, "client_id", cfg.name));
        cfg.keep_alive = static_cast<int>(GetUint64Or(obj, "keep_alive", 60));
        cfg.clean_session = GetBoolOr(obj, "clean_session", true);
        cfg.ca_cert = ToString(GetStringOr(obj, "ca_cert", ""));
        cfg.client_cert = ToString(GetStringOr(obj, "client_cert", ""));
        cfg.client_key = ToString(GetStringOr(obj, "client_key", ""));
        cfg.tls_insecure = GetBoolOr(obj, "tls_insecure", false);

        auto topics = obj["topics"].get_array();
        if (!topics.error())
        {
            for (const auto &topic_entry : topics.value())
            {
                auto topic_obj = topic_entry.get_object();
                if (topic_obj.error())
                {
                    continue;
                }

                const auto topic = topic_obj.value();
                MqttTopic t;
                t.filter = ToString(GetStringOr(topic, "filter", ""));
                const auto raw_qos = static_cast<int>(GetUint64Or(topic, "qos", 0));
                t.qos = std::clamp(raw_qos, kDefaultQosMin, kDefaultQosMax);
                if (!t.filter.empty())
                {
                    cfg.topics.push_back(std::move(t));
                }
            }
        }

        result.push_back(std::move(cfg));
    }

    return result;
}
