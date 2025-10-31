#include "lavinmq_publisher.h"
#include <iostream>
#include <cstring>
#include <array>

namespace
{

    amqp_table_t makeDeduplicationArguments(std::array<amqp_table_entry_t, 1> &storage)
    {
        amqp_table_t table{};
        storage[0].key = amqp_cstring_bytes("x-message-deduplication");
        storage[0].value.kind = AMQP_FIELD_KIND_BOOLEAN;
        storage[0].value.value.boolean = 1;
        table.num_entries = 1;
        table.entries = storage.data();
        return table;
    }

}

LavinMQPublisher::LavinMQPublisher(const std::string &host, int port, const std::string &user, const std::string &pass, const std::string &vhost, const std::string &exchange, const std::string &routing_key)
    : exchange(exchange), routing_key(routing_key), connected(false)
{
    conn = amqp_new_connection();
    if (!conn)
    {
        std::cerr << "💥 Failed to create AMQP connection" << std::endl;
        return;
    }
    amqp_socket_t *socket = amqp_tcp_socket_new(conn);
    if (!socket)
    {
        std::cerr << "💥 Failed to create socket" << std::endl;
        return;
    }
    int status = amqp_socket_open(socket, host.c_str(), port);
    if (status)
    {
        std::cerr << "💥 Failed to open socket" << std::endl;
        return;
    }
    amqp_rpc_reply_t reply = amqp_login(conn, vhost.c_str(), 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, user.c_str(), pass.c_str());
    if (reply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        std::cerr << "💥 Failed to login" << std::endl;
        return;
    }
    amqp_channel_open(conn, 1);
    reply = amqp_get_rpc_reply(conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL)
    {
        std::cerr << "Failed to open channel" << std::endl;
        return;
    }
    connected = true;
    std::cout << "Connected to LavinMQ" << std::endl;
}

LavinMQPublisher::~LavinMQPublisher()
{
    amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn);
}

void LavinMQPublisher::publish(const std::string &body, const std::string &message_id)
{
    if (!connected)
    {
        std::cerr << "Not connected\n";
        return;
    }

    // 1) Build headers: x-deduplication-header = message_id
    amqp_table_entry_t header_entries[1];
    header_entries[0].key = amqp_cstring_bytes("x-deduplication-header");
    header_entries[0].value.kind = AMQP_FIELD_KIND_UTF8;
    header_entries[0].value.value.bytes = amqp_cstring_bytes(message_id.c_str());

    amqp_table_t headers;
    headers.num_entries = 1;
    headers.entries = header_entries;

    // 2) Properties (include HEADERS flag!)
    amqp_basic_properties_t props;
    memset(&props, 0, sizeof(props));
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG | AMQP_BASIC_MESSAGE_ID_FLAG | AMQP_BASIC_HEADERS_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2;
    props.message_id = amqp_cstring_bytes(message_id.c_str());
    props.headers = headers;

    int rc = amqp_basic_publish(conn, 1,
                                amqp_cstring_bytes(exchange.c_str()),
                                amqp_cstring_bytes(routing_key.c_str()),
                                0, 0, &props, amqp_cstring_bytes(body.c_str()));

    if (rc != AMQP_STATUS_OK)
        std::cerr << "publish failed\n";
}

bool LavinMQPublisher::setupQueues(const std::vector<std::pair<std::string, std::string>> &queueBindings)
{
    if (!connected)
    {
        std::cerr << "Not connected to LavinMQ, cannot declare queues" << std::endl;
        return false;
    }

    bool all_ok = true;
    for (const auto &qb : queueBindings)
    {
        const std::string &queue = qb.first;
        const std::string &bind_routing_key = qb.second.empty() ? routing_key : qb.second;

        // Declare durable queue (non-passive, non-exclusive, non-auto-delete)
        std::array<amqp_table_entry_t, 1> dedup_storage;
        amqp_queue_declare_ok_t *r = amqp_queue_declare(
            conn,
            1,
            amqp_cstring_bytes(queue.c_str()),
            0, // passive
            1, // durable
            0, // exclusive
            0, // auto-delete
            makeDeduplicationArguments(dedup_storage));
        amqp_rpc_reply_t reply = amqp_get_rpc_reply(conn);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL || r == nullptr)
        {
            std::cerr << "💥 Queue declare failed for " << queue << std::endl;
            all_ok = false;
            continue; // try other queues
        }

        // Bind queue to exchange with routing key
        amqp_queue_bind_ok_t *bind_ok = amqp_queue_bind(
            conn,
            1,
            amqp_cstring_bytes(queue.c_str()),
            amqp_cstring_bytes(exchange.c_str()),
            amqp_cstring_bytes(bind_routing_key.c_str()),
            amqp_empty_table);
        reply = amqp_get_rpc_reply(conn);
        if (reply.reply_type != AMQP_RESPONSE_NORMAL || bind_ok == nullptr)
        {
            std::cerr << "💥 Queue bind failed for " << queue << " with routing key " << bind_routing_key << std::endl;
            all_ok = false;
            continue;
        }

        std::cout << "🔗 Queue '" << queue << "' bound to exchange '" << exchange << "' with routing key '" << bind_routing_key << "'" << std::endl;
    }
    return all_ok;
}
