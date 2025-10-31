#include "PostgresClient.h"

#include <iostream>
#include <sstream>
#include <string_view>

namespace {
std::string_view get_string_or(const simdjson::dom::object &obj, std::string_view key, std::string_view fallback) {
    if (auto maybe = obj[key].get_string(); !maybe.error()) {
        return maybe.value();
    }
    return fallback;
}

int get_int_or(const simdjson::dom::object &obj, std::string_view key, int fallback) {
    if (auto maybe = obj[key].get_int64(); !maybe.error()) {
        return static_cast<int>(maybe.value());
    }
    return fallback;
}

std::string to_string(std::string_view value) {
    return std::string(value.begin(), value.end());
}
}

PostgresClient::Config PostgresClient::FromJson(const simdjson::dom::object &postgres) {
    Config cfg;
    cfg.host = to_string(get_string_or(postgres, "host", cfg.host));
    cfg.port = get_int_or(postgres, "port", cfg.port);
    cfg.database = to_string(get_string_or(postgres, "database", cfg.database));
    cfg.user = to_string(get_string_or(postgres, "user", cfg.user));
    cfg.password = to_string(get_string_or(postgres, "password", cfg.password));
    cfg.sslmode = to_string(get_string_or(postgres, "sslmode", cfg.sslmode));
    cfg.connect_timeout = get_int_or(postgres, "connect_timeout", cfg.connect_timeout);
    return cfg;
}

PostgresClient::PostgresClient(Config cfg) : config_(std::move(cfg)) {}

std::string PostgresClient::connection_uri() const {
    std::ostringstream connection;
    connection << "postgresql://";
    if (!config_.user.empty()) {
        connection << config_.user;
        if (!config_.password.empty()) {
            connection << ':' << config_.password;
        }
        connection << '@';
    }
    connection << config_.host << ':' << config_.port << '/' << config_.database;

    bool first_param = true;
    const auto append_query_param = [&](std::string_view key, std::string_view value) {
        if (value.empty()) {
            return;
        }
        connection << (first_param ? '?' : '&') << key << '=' << value;
        first_param = false;
    };

    append_query_param("sslmode", config_.sslmode);
    append_query_param("connect_timeout", std::to_string(config_.connect_timeout));

    return connection.str();
}

bool PostgresClient::connect() {
    if (connection_ && connection_->is_open()) {
        return true;
    }

    const auto uri = connection_uri();
    try {
        connection_.emplace(uri);
    } catch (const std::exception &ex) {
        std::cerr << "Unable to connect to PostgreSQL: " << ex.what() << std::endl;
        connection_.reset();
        return false;
    }

    if (!connection_->is_open()) {
        std::cerr << "Unable to open PostgreSQL connection" << std::endl;
        connection_.reset();
        return false;
    }

    std::cout << "Connected to PostgreSQL database: " << connection_->dbname() << std::endl;
    return true;
}

bool PostgresClient::is_connected() const {
    return connection_ && connection_->is_open();
}

pqxx::connection &PostgresClient::connection() {
    if (!connection_) {
        throw std::runtime_error("PostgreSQL connection not established");
    }
    return *connection_;
}
