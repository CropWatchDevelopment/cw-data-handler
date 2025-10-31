#include "mqtt_handler.h"
#include <iostream>
#include <cstring>

static MqttHandler* instance = nullptr;

void on_connect_wrapper(struct mosquitto* mosq, void* obj, int rc) {
    if (instance) instance->on_connect(rc);
}

void on_message_wrapper(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg) {
    if (instance) instance->on_message(msg);
}

MqttHandler::MqttHandler(const std::string& host, int port, const std::string& username, const std::string& password, const std::string& topic)
    : mosq(nullptr), host(host), port(port), topic(topic) {
    instance = this;
    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        std::cerr << "Error: Out of memory for MQTT." << std::endl;
        return;
    }
    mosquitto_username_pw_set(mosq, username.c_str(), password.c_str());
    mosquitto_connect_callback_set(mosq, on_connect_wrapper);
    mosquitto_message_callback_set(mosq, on_message_wrapper);
}

MqttHandler::~MqttHandler() {
    if (mosq) {
        mosquitto_destroy(mosq);
    }
    mosquitto_lib_cleanup();
}

void MqttHandler::set_message_callback(std::function<void(const std::string&)> cb) {
    message_callback = cb;
}

void MqttHandler::connect_and_subscribe() {
    int rc = mosquitto_connect(mosq, host.c_str(), port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "Unable to connect to MQTT broker: " << mosquitto_strerror(rc) << std::endl;
    }
}

void MqttHandler::loop() {
    mosquitto_loop_start(mosq);
}

void MqttHandler::disconnect() {
    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
}

void MqttHandler::on_connect(int rc) {
    if (rc == 0) {
        std::cout << "🔌 Connected to MQTT broker" << std::endl;
        mosquitto_subscribe(mosq, NULL, topic.c_str(), 1);
    } else {
        std::cerr << "Failed to connect, return code: " << rc << std::endl;
    }
}

void MqttHandler::on_message(const struct mosquitto_message* msg) {
    if (msg->payloadlen > 0) {
        std::string json_str(static_cast<char*>(msg->payload), msg->payloadlen);
        if (message_callback) {
            message_callback(json_str);
        }
    }
}
