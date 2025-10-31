#pragma once

#include <simdjson.h>

#include <string>
#include <vector>

#if defined(HAVE_AMQP_H)
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <amqp_ssl_socket.h>
#elif defined(HAVE_RABBITMQ_C_AMQP_H)
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>
#include <rabbitmq-c/ssl_socket.h>
#else
#endif
#include <amqp.h>

struct LavinMQMessage
{
    uint64_t delivery_tag{0};
    std::string body;
};

struct LavinMQConfig
{
    std::string host;
    int port{5672};
    std::string username;
    std::string password;
    std::string virtual_host;
    int heartbeat{60};
    std::string connection_name;
    bool tls{false};
    std::string queue_name;
};

class LavinMQConnection
{
public:
    explicit LavinMQConnection(LavinMQConfig cfg);
    ~LavinMQConnection();

    LavinMQConnection(const LavinMQConnection &) = delete;
    LavinMQConnection &operator=(const LavinMQConnection &) = delete;
    LavinMQConnection(LavinMQConnection &&) = delete;
    LavinMQConnection &operator=(LavinMQConnection &&) = delete;

    bool connect();
    void disconnect();
    bool is_connected() const;

    std::vector<LavinMQMessage> readAllMessages(const std::string &queue);
    std::vector<LavinMQMessage> readConfiguredQueue();

    bool ackMessage(uint64_t delivery_tag, bool multiple = false);
    bool nackMessage(uint64_t delivery_tag, bool requeue = true, bool multiple = false);

    const LavinMQConfig &config() const { return config_; }

    static LavinMQConfig FromJson(const simdjson::dom::object &lavinmq);
    std::string connection_uri() const;

private:
    static bool LogOnError(int status, const char *context);
    static bool LogOnError(const amqp_rpc_reply_t &reply, const char *context);
    static std::string RpcReplyMessage(const amqp_rpc_reply_t &reply);

    LavinMQConfig config_;
    amqp_connection_state_t connection_{nullptr};
    amqp_channel_t channel_{1};
};
