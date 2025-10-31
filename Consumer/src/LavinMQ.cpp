#include "LavinMQ.h"

#include <iostream>
#include <sstream>
#include <string_view>
#include <sys/time.h>
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/ssl_socket.h>
#include <rabbitmq-c/tcp_socket.h>

namespace {
std::string_view GetStringOr(const simdjson::dom::object &obj, std::string_view key, std::string_view fallback) {
    if (auto value = obj[key].get_string(); !value.error()) {
        return value.value();
    }
    return fallback;
}

uint64_t GetUint64Or(const simdjson::dom::object &obj, std::string_view key, uint64_t fallback) {
    if (auto value = obj[key].get_uint64(); !value.error()) {
        return value.value();
    }
    return fallback;
}

bool GetBoolOr(const simdjson::dom::object &obj, std::string_view key, bool fallback) {
    if (auto value = obj[key].get_bool(); !value.error()) {
        return value.value();
    }
    return fallback;
}

std::string ToString(std::string_view value) {
    return std::string(value.begin(), value.end());
}
} // namespace

LavinMQConnection::LavinMQConnection(LavinMQConfig cfg)
    : config_(std::move(cfg)) {}

LavinMQConnection::~LavinMQConnection() {
    disconnect();
}

bool LavinMQConnection::LogOnError(int status, const char *context) {
    if (status == AMQP_STATUS_OK) {
        return true;
    }
    std::cerr << "[LavinMQ] " << context << " failed: " << amqp_error_string2(status) << std::endl;
    return false;
}

std::string LavinMQConnection::RpcReplyMessage(const amqp_rpc_reply_t &reply) {
    switch (reply.reply_type) {
    case AMQP_RESPONSE_NONE:
        return "missing RPC reply";
    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
        return amqp_error_string2(reply.library_error);
    case AMQP_RESPONSE_SERVER_EXCEPTION: {
        std::ostringstream out;
        switch (reply.reply.id) {
        case AMQP_CONNECTION_CLOSE_METHOD: {
            const auto *m = static_cast<amqp_connection_close_t *>(reply.reply.decoded);
            out << "connection closed (" << m->reply_code << "): "
                << std::string(static_cast<char *>(m->reply_text.bytes), m->reply_text.len);
            break;
        }
        case AMQP_CHANNEL_CLOSE_METHOD: {
            const auto *m = static_cast<amqp_channel_close_t *>(reply.reply.decoded);
            out << "channel closed (" << m->reply_code << "): "
                << std::string(static_cast<char *>(m->reply_text.bytes), m->reply_text.len);
            break;
        }
        default:
            out << "server exception, method id " << reply.reply.id;
            break;
        }
        return out.str();
    }
    case AMQP_RESPONSE_NORMAL:
    default:
        return "ok";
    }
}

bool LavinMQConnection::LogOnError(const amqp_rpc_reply_t &reply, const char *context) {
    if (reply.reply_type == AMQP_RESPONSE_NORMAL) {
        return true;
    }
    std::cerr << "[LavinMQ] " << context << " failed: " << RpcReplyMessage(reply) << std::endl;
    return false;
}

bool LavinMQConnection::connect() {
    if (connection_) {
        return true;
    }

    constexpr int kChannelMax = 0;
    constexpr int kFrameMax = 131072;

    connection_ = amqp_new_connection();
    if (!connection_) {
        std::cerr << "[LavinMQ] Unable to allocate connection object" << std::endl;
        return false;
    }

    amqp_socket_t *socket = config_.tls ? amqp_ssl_socket_new(connection_) : amqp_tcp_socket_new(connection_);
    if (!socket) {
        std::cerr << "[LavinMQ] Unable to allocate socket" << std::endl;
        amqp_destroy_connection(connection_);
        connection_ = nullptr;
        return false;
    }

    if (!LogOnError(amqp_socket_open(socket, config_.host.c_str(), config_.port), "socket open")) {
        amqp_destroy_connection(connection_);
        connection_ = nullptr;
        return false;
    }

    auto login = amqp_login(connection_,
                            config_.virtual_host.c_str(),
                            kChannelMax,
                            kFrameMax,
                            config_.heartbeat,
                            AMQP_SASL_METHOD_PLAIN,
                            config_.username.c_str(),
                            config_.password.c_str());
    if (!LogOnError(login, "login")) {
        amqp_destroy_connection(connection_);
        connection_ = nullptr;
        return false;
    }

    amqp_channel_open(connection_, channel_);
    if (!LogOnError(amqp_get_rpc_reply(connection_), (const char *)"channel open")) {
        amqp_connection_close(connection_, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(connection_);
        connection_ = nullptr;
        return false;
    }

    std::cout << "[LavinMQ] Connected " << config_.host << ':' << config_.port
              << " vhost '" << config_.virtual_host << "'";
    if (!config_.connection_name.empty()) {
        std::cout << " connection '" << config_.connection_name << "'";
    }
    std::cout << std::endl;

    return true;
}

void LavinMQConnection::disconnect() {
    if (!connection_) {
        return;
    }

    amqp_channel_close(connection_, channel_, AMQP_REPLY_SUCCESS);
    amqp_connection_close(connection_, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(connection_);
    connection_ = nullptr;
    std::cout << "[LavinMQ] Disconnected" << std::endl;
}

bool LavinMQConnection::is_connected() const {
    return connection_ != nullptr;
}

LavinMQConfig LavinMQConnection::FromJson(const simdjson::dom::object &lavinmq) {
    LavinMQConfig cfg;
    cfg.host = ToString(GetStringOr(lavinmq, "host", "localhost"));
    cfg.port = static_cast<int>(GetUint64Or(lavinmq, "port", 5672));
    cfg.username = ToString(GetStringOr(lavinmq, "username", "guest"));
    cfg.password = ToString(GetStringOr(lavinmq, "password", "guest"));
    cfg.virtual_host = ToString(GetStringOr(lavinmq, "virtual_host", "/"));
    cfg.heartbeat = static_cast<int>(GetUint64Or(lavinmq, "heartbeat", 60));
    cfg.connection_name = ToString(GetStringOr(lavinmq, "connection_name", "cw-data-transfer"));
    cfg.tls = GetBoolOr(lavinmq, "tls", false);
    cfg.queue_name = ToString(GetStringOr(lavinmq, "queue_name", "data_transfer"));
    return cfg;
}

std::string LavinMQConnection::connection_uri() const {
    std::ostringstream uri;
    uri << (config_.tls ? "amqps://" : "amqp://");

    if (!config_.username.empty()) {
        uri << config_.username;
        if (!config_.password.empty()) {
            uri << ':' << config_.password;
        }
        uri << '@';
    }

    uri << config_.host << ':' << config_.port << config_.virtual_host;
    return uri.str();
}

std::vector<LavinMQMessage> LavinMQConnection::readAllMessages(const std::string &queue) {
    std::vector<LavinMQMessage> messages;

    if (!connection_) {
        std::cerr << "[LavinMQ] Cannot read messages: not connected" << std::endl;
        return messages;
    }

    amqp_bytes_t queue_bytes = amqp_cstring_bytes(queue.c_str());

    amqp_queue_declare_ok_t *declare_ok = amqp_queue_declare(connection_,
                                                             channel_,
                                                             queue_bytes,
                                                             true,   // passive check
                                                             false,  // durable (ignored when passive)
                                                             false,  // exclusive
                                                             false,  // auto-delete
                                                             amqp_empty_table);
    if (!declare_ok) {
        LogOnError(amqp_get_rpc_reply(connection_), "queue declare");
        return messages;
    }

    amqp_basic_consume_ok_t *consume_ok = amqp_basic_consume(connection_,
                                                             channel_,
                                                             queue_bytes,
                                                             amqp_empty_bytes,
                                                             false,  // no-local
                                                             false,  // no-ack (manual ack expected later)
                                                             false,  // exclusive
                                                             amqp_empty_table);
    if (!consume_ok) {
        LogOnError(amqp_get_rpc_reply(connection_), "basic consume");
        return messages;
    }

    amqp_bytes_t consumer_tag = consume_ok->consumer_tag;
    while (true) {
        amqp_envelope_t envelope;
        amqp_maybe_release_buffers(connection_);
        timeval timeout{0, 0};
        amqp_rpc_reply_t reply = amqp_consume_message(connection_, &envelope, &timeout, 0);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
            if (reply.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION &&
                reply.library_error == AMQP_STATUS_TIMEOUT) {
                break; // No more messages available right now
            }
            LogOnError(reply, "consume message");
            break;
        }

        const auto *raw = static_cast<const char *>(envelope.message.body.bytes);
        std::string body(raw, raw + envelope.message.body.len);

        LavinMQMessage message;
        message.delivery_tag = envelope.delivery_tag;
        message.body = std::move(body);
        messages.push_back(std::move(message));

        // TODO: Acknowledge (ack) or reject (nack) the message here once processing succeeds.
        amqp_destroy_envelope(&envelope);
    }

    amqp_basic_cancel_ok_t *cancel_ok = amqp_basic_cancel(connection_, channel_, consumer_tag);
    if (!cancel_ok) {
        LogOnError(amqp_get_rpc_reply(connection_), "basic cancel");
    }

    return messages;
}

std::vector<LavinMQMessage> LavinMQConnection::readConfiguredQueue() {
    return readAllMessages(config_.queue_name);
}

bool LavinMQConnection::ackMessage(uint64_t delivery_tag, bool multiple) {
    if (!connection_) {
        std::cerr << "[LavinMQ] Cannot ack message: not connected" << std::endl;
        return false;
    }
    const int status = amqp_basic_ack(connection_, channel_, delivery_tag, multiple ? 1 : 0);
    return LogOnError(status, "basic ack");
}

bool LavinMQConnection::nackMessage(uint64_t delivery_tag, bool requeue, bool multiple) {
    if (!connection_) {
        std::cerr << "[LavinMQ] Cannot nack message: not connected" << std::endl;
        return false;
    }
    const int status = amqp_basic_nack(connection_,
                                       channel_,
                                       delivery_tag,
                                       multiple ? 1 : 0,
                                       requeue ? 1 : 0);
    return LogOnError(status, "basic nack");
}
