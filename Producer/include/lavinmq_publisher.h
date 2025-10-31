#ifndef LAVINMQ_PUBLISHER_H
#define LAVINMQ_PUBLISHER_H

#include <string>
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <vector>
#include <utility>

class LavinMQPublisher {
private:
    amqp_connection_state_t conn;
    std::string exchange;
    std::string routing_key;
    bool connected;
public:
    LavinMQPublisher(const std::string& host, int port, const std::string& user, const std::string& pass, const std::string& vhost = "/", const std::string& exchange = "amq.topic", const std::string& routing_key = "things.data");
    ~LavinMQPublisher();
    void publish(const std::string& message, const std::string& message_id);
    // Declare (durable) queues and bind them to the exchange/routing key pairs.
    // Each pair: first = queue name, second = routing key to bind with.
    // Returns true if all declarations & bindings succeeded; logs individual failures.
    bool setupQueues(const std::vector<std::pair<std::string,std::string>>& queueBindings);
};

#endif // LAVINMQ_PUBLISHER_H
