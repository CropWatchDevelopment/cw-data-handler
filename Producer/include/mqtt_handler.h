#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <mosquitto.h>
#include <string>
#include <functional>

class MqttHandler {
private:
    struct mosquitto* mosq;
    std::function<void(const std::string&)> message_callback;
    std::string host;
    int port;
    std::string topic;
public:
    MqttHandler(const std::string& host, int port, const std::string& username, const std::string& password, const std::string& topic = "#");
    ~MqttHandler();
    void set_message_callback(std::function<void(const std::string&)> cb);
    void connect_and_subscribe();
    void loop();
    void disconnect();
    void on_connect(int rc);
    void on_message(const struct mosquitto_message* msg);
};

#endif // MQTT_HANDLER_H
