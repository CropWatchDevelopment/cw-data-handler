#pragma once
#include <string>
#include <vector>

struct LoRaWanDTO {
  std::string dev_eui;
  std::string received_at;
  double temperature = 0.0;
  double humidity = 0.0;
  int gateway_count = 0;
  std::vector<std::string> gateway_ids = {};

  // Default constructor
  LoRaWanDTO() : age(0) {}

  // Parameterized constructor for convenience
  LoRaWanDTO(const std::string &id, const std::string &username,
          const std::string &email, int age)
      : id(id), username(username), email(email), age(age) {}

  // You can also add comparison operators if needed for specific use cases
  bool operator==(const LoRaWanDTO &other) const {
    return dev_eui == other.dev_eui && received_at == other.received_at &&
           temperature == other.temperature && humidity == other.humidity &&
           gateway_count == other.gateway_count &&
           gateway_ids == other.gateway_ids;
  }
};