#pragma once

#include <pqxx/pqxx>
#include <simdjson.h>

#include <optional>
#include <string>

class PostgresClient {
public:
    struct Config {
        std::string host{"localhost"};
        int port{5432};
        std::string database{"postgres"};
        std::string user{"postgres"};
        std::string password;
        std::string sslmode{"prefer"};
        int connect_timeout{5};
    };

    static PostgresClient::Config FromJson(const simdjson::dom::object &postgres);

    explicit PostgresClient(Config cfg);

    bool connect();
    bool is_connected() const;
    pqxx::connection &connection();
    const Config &config() const { return config_; }
    std::string connection_uri() const;

private:
    Config config_;
    std::optional<pqxx::connection> connection_;
};
