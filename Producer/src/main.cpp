#include <iostream>
#include <csignal>
#include <fstream>
#include <sstream>
#include "mqtt_handler.h"
#include "lavinmq_publisher.h"
#include "config_loader.h"
#include <simdjson/dom.h>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <cstdint>

using namespace simdjson;

volatile sig_atomic_t stop = 0;

void signal_handler(int signum) {
    stop = 1;
}

/**
 * Filter JSON message to include only the required data points
 */
std::string filterMessageData(const std::string& json_str) {
    try {
        dom::parser parser;
        auto doc_result = parser.parse(json_str);
        if (doc_result.error()) {
            std::cerr << "JSON parse error in filter: " << doc_result.error() << std::endl;
            return json_str; // Return original on error
        }
        auto doc = doc_result.value();

        // Create filtered JSON object
        std::string filtered_json = "{";

        // 1. Extract device EUI from end_device_ids.device_id
        auto end_device_ids = doc["end_device_ids"];
        if (!end_device_ids.error()) {
            auto device_id = end_device_ids["dev_eui"];
            if (!device_id.error() && device_id.is_string()) {
                filtered_json += "\"dev_eui\":\"" + std::string(device_id) + "\",";
            }
        }

        // 2. Extract entire decoded_payload object
        auto uplink_message = doc["uplink_message"];
        if (!uplink_message.error()) {
            auto decoded_payload = uplink_message["decoded_payload"];
            if (!decoded_payload.error()) {
                // Extract individual fields from decoded_payload
                std::string decoded_json = "{";
                bool has_fields = false;


                /***************************************************************************************
                    MULTI SENSOR TYPE VALUES, BASICALLY VALUES THAT MAY COME FROM ANY SENSOR AND HAVE
                    THE SAME SEMANTIC MEANING
                ***************************************************************************************/
                auto temperature_c = decoded_payload["temperature_c"];
                if (!temperature_c.error()) {
                    decoded_json += "\"temperature_c\":" + std::to_string(double(temperature_c)) + ",";
                    has_fields = true;
                }

                auto last_updated = decoded_payload["last_updated"];
                if (!last_updated.error() && last_updated.is_string()) {
                    decoded_json += "\"last_updated\":\"" + std::string(last_updated) + "\",";
                    has_fields = true;
                }
                

                /***************************************************************************************
                    AIR RELATED NUMBERS, BASICALLY STUFF ABOVE GROUND LEVEL IN THE SKY
                ***************************************************************************************/
                auto humidity = decoded_payload["humidity"];
                if (!humidity.error()) {
                    decoded_json += "\"humidity\":" + std::to_string(double(humidity)) + ",";
                    has_fields = true;
                }

                auto co2 = decoded_payload["co2"];
                if (!co2.error()) {
                    decoded_json += "\"co2\":" + std::to_string(int64_t(co2)) + ",";
                    has_fields = true;
                }

                auto pressure = decoded_payload["pressure"];
                if (!pressure.error()) {
                    decoded_json += "\"pressure\":" + std::to_string(double(pressure)) + ",";
                    has_fields = true;
                }

                auto rainfall = decoded_payload["rainfall"];
                if (!rainfall.error()) {
                    decoded_json += "\"rainfall\":" + std::to_string(double(rainfall)) + ",";
                    has_fields = true;
                }

                auto wind_speed = decoded_payload["wind_speed"];
                if (!wind_speed.error()) {
                    decoded_json += "\"wind_speed\":" + std::to_string(double(wind_speed)) + ",";
                    has_fields = true;
                }

                auto wind_direction = decoded_payload["wind_direction"];
                if (!wind_direction.error()) {
                    decoded_json += "\"wind_direction\":" + std::to_string(int64_t(wind_direction)) + ",";
                    has_fields = true;
                }

                auto lux = decoded_payload["lux"];
                if (!lux.error()) {
                    decoded_json += "\"lux\":" + std::to_string(int64_t(lux)) + ",";
                    has_fields = true;
                }

                auto uv_index = decoded_payload["uv_index"];
                if (!uv_index.error()) {
                    decoded_json += "\"uv_index\":" + std::to_string(double(uv_index)) + ",";
                    has_fields = true;
                }


                /***************************************************************************************
                    RELAY STUFF, BASICALLY STUFF THAT IS ABOUT SWITCHING THINGS ON/OFF
                ***************************************************************************************/
                auto relay_1 = decoded_payload["relay_1"];
                if (!relay_1.error()) {
                    decoded_json += "\"relay_1\":" + std::to_string(int64_t(relay_1)) + ",";
                    has_fields = true;
                }
                auto relay_2 = decoded_payload["relay_2"];
                if (!relay_2.error()) {
                    decoded_json += "\"relay_2\":" + std::to_string(int64_t(relay_2)) + ",";
                    has_fields = true;
                }

                /***************************************************************************************
                    UNDER WATER STUFF, BASICALLY STUFF THAT IS BELOW WATER
                ***************************************************************************************/

                auto deapth_cm = decoded_payload["depth_cm"];
                if (!deapth_cm.error()) {
                    decoded_json += "\"depth_cm\":" + std::to_string(double(deapth_cm)) + ",";
                    has_fields = true;
                }

                auto spo2 = decoded_payload["spo2"];
                if (!spo2.error()) {
                    decoded_json += "\"spo2\":" + std::to_string(double(spo2)) + ",";
                    has_fields = true;
                }


                /////////////////////////////////////////////////////////////////////////////////////////
                // Remove trailing comma if present
                if (decoded_json.back() == ',') {
                    decoded_json.pop_back();
                }

                if (has_fields) {
                    decoded_json += "}";
                    filtered_json += "\"decoded_payload\":" + decoded_json + ",";
                }
            }
        }

        // 3. Extract gateway EUIs from rx_metadata
        /*
        rx_metadata example:
        "rx_metadata": [
        {
          "gateway_ids": {
            "gateway_id": "sg-pt-in01",
            "eui": "24E124FFFEF865F0"
          },
          "time": "2025-10-24T10:45:40.914Z",
          "timestamp": 3251840015,
          "rssi": -94,
          "channel_rssi": -94,
          "snr": 9.2,
          "frequency_offset": "227",
          "uplink_token": "ChgKFgoKc2ctcHQtaW4wMRIIJOEk//74ZfAQj8jMjgwaCwjVse3HBhDQ694GIJi1k4bSXioMCNSx7ccGEICR6rMD",
          "channel_index": 4,
          "gps_time": "2025-10-24T10:45:40.914Z",
          "received_at": "2025-10-24T10:45:41.014136784Z"
        },
        {
          "gateway_ids": {
            "gateway_id": "eui-24e124fffef24825",
            "eui": "24E124FFFEF24825"
          },
          "time": "2025-10-24T10:45:40.987Z",
          "timestamp": 3804092458,
          "rssi": -118,
          "channel_rssi": -118,
          "snr": -8,
          "frequency_offset": "-466",
          "location": {
            "latitude": 31.9584376849043,
            "longitude": 131.46944714182,
            "altitude": 10,
            "source": "SOURCE_REGISTRY"
          },
          "uplink_token": "CiIKIAoUZXVpLTI0ZTEyNGZmZmVmMjQ4MjUSCCThJP/+8kglEKqw95UOGgsI1bHtxwYQooubNiCQyNGs24UjKgwI1LHtxwYQwNnR1gM=",
          "channel_index": 4,
          "gps_time": "2025-10-24T10:45:40.987Z",
          "received_at": "2025-10-24T10:45:40.996074457Z"
        }
      ],
        */
        if (!uplink_message.error()) {
            auto rx_metadata = uplink_message["rx_metadata"];
            if (!rx_metadata.error() && rx_metadata.is_array()) {
                auto rx_array = rx_metadata.get_array();
                filtered_json += "\"gateway_ids\":[";
                bool first_gateway = true;
                for (size_t i = 0; i < rx_array.size(); ++i) {
                    auto rx_item = rx_array.at(i);
                    if (!rx_item.error()) {
                        auto gateway_ids = rx_item["gateway_ids"];
                        if (!gateway_ids.error()) {
                            auto eui = gateway_ids["eui"];
                            if (!eui.error() && eui.is_string()) {
                                if (!first_gateway) filtered_json += ",";

                                std::string gateway_entry = "{\"eui\":\"" + std::string(eui) + "\"";
                                auto rssi_result = rx_item["rssi"];
                                if (!rssi_result.error()) {
                                    auto rssi_element = rssi_result.value();
                                    if (rssi_element.is_int64()) {
                                        gateway_entry += ",\"rssi\":" + std::to_string(rssi_element.get_int64().value());
                                    } else if (rssi_element.is_uint64()) {
                                        gateway_entry += ",\"rssi\":" + std::to_string(rssi_element.get_uint64().value());
                                    } else if (rssi_element.is_double()) {
                                        gateway_entry += ",\"rssi\":" + std::to_string(rssi_element.get_double().value());
                                    }
                                }

                                auto snr_result = rx_item["snr"];
                                if (!snr_result.error()) {
                                    auto snr_element = snr_result.value();
                                    if (snr_element.is_int64()) {
                                        gateway_entry += ",\"snr\":" + std::to_string(snr_element.get_int64().value());
                                    } else if (snr_element.is_uint64()) {
                                        gateway_entry += ",\"snr\":" + std::to_string(snr_element.get_uint64().value());
                                    } else if (snr_element.is_double()) {
                                        gateway_entry += ",\"snr\":" + std::to_string(snr_element.get_double().value());
                                    }
                                }
                                gateway_entry += "}";

                                filtered_json += gateway_entry;
                                first_gateway = false;
                            }
                        }
                    }
                }
                filtered_json += "],";
            }
        }

        // 4. Extract battery percentage
        auto last_battery = doc["last_battery_percentage"];
        if (!last_battery.error()) {
            auto battery_value = last_battery["value"];
            if (!battery_value.error()) {
                // Convert to string representation
                if (battery_value.is_double()) {
                    filtered_json += "\"battery_percentage\":" + std::to_string(double(battery_value)) + ",";
                } else if (battery_value.is_int64()) {
                    filtered_json += "\"battery_percentage\":" + std::to_string(int64_t(battery_value)) + ",";
                }
            }
        }

        // 5. Extract received_at timestamp
        auto received_at = doc["received_at"];
        if (!received_at.error() && received_at.is_string()) {
            filtered_json += "\"received_at\":\"" + std::string(received_at) + "\",";
        }

        // Remove trailing comma if present
        if (filtered_json.back() == ',') {
            filtered_json.pop_back();
        }

        filtered_json += "}";
        return filtered_json;

    } catch (const std::exception& e) {
        std::cerr << "Error filtering message: " << e.what() << std::endl;
        return json_str; // Return original on error
    }
}

/**
 * Extract dev_eui and timestamp from JSON message for clean display
 */
std::string computeDeterministicId(const std::string& payload) {
    const uint64_t fnv_offset_basis = 1469598103934665603ULL;
    const uint64_t fnv_prime = 1099511628211ULL;
    uint64_t hash = fnv_offset_basis;
    for (unsigned char c : payload) {
        hash ^= static_cast<uint64_t>(c);
        hash *= fnv_prime;
    }
    std::ostringstream oss;
    oss << "hash:" << std::hex << hash;
    return oss.str();
}

/**
 * Extract dev_eui and timestamp from JSON message for clean display
 */
std::pair<std::string, std::string> extractMessageInfo(const std::string& json_str) {
    try {
        dom::parser parser;
        auto doc_result = parser.parse(json_str);
        if (doc_result.error()) {
            return {"unknown", "unknown"};
        }
        auto doc = doc_result.value();

        std::string dev_eui = "unknown";
        std::string timestamp = "unknown";

        // Extract dev_eui from end_device_ids.device_id
        auto end_device_ids = doc["end_device_ids"];
        if (!end_device_ids.error()) {
            auto device_id = end_device_ids["dev_eui"];
            if (!device_id.error() && device_id.is_string()) {
                dev_eui = std::string(device_id);
            }
        }

        // Extract timestamp from received_at
        auto received_at = doc["received_at"];
        if (!received_at.error() && received_at.is_string()) {
            timestamp = std::string(received_at);
        }

        return {dev_eui, timestamp};
    } catch (const std::exception& e) {
        return {"unknown", "unknown"};
    }
}

int main() {
    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);

    // Load MQTT sources from config
    std::vector<MqttSource> sources;
    LavinMQConfig lavinConfig;
    if (!loadMqttSources("config.json", sources, &lavinConfig)) {
        std::cerr << "Failed to load configuration from config.json." << std::endl;
        return 1;
    }

    // Create LavinMQ publisher from config
    LavinMQPublisher publisher(
        lavinConfig.host,
        lavinConfig.port,
        lavinConfig.username,
        lavinConfig.password,
        lavinConfig.vhost,
        lavinConfig.exchange,
        lavinConfig.routing_key);

    // Declare & bind required durable queues for fan-out (storage + alert processing)
    publisher.setupQueues({
        {"uplink_data_queue", lavinConfig.routing_key.empty() ? "things.data" : lavinConfig.routing_key},
    });

    // Vector to hold handlers
    std::vector<std::unique_ptr<MqttHandler>> handlers;

    // Create and start handlers for each source
    for (const auto& source : sources) {
        auto handler = std::make_unique<MqttHandler>(source.host, source.port, source.username, source.password, source.topic);
        
        // Set callback for each handler
        handler->set_message_callback([&publisher](const std::string& json_str) {
            // Extract clean info for display
            auto [dev_eui, timestamp] = extractMessageInfo(json_str);
            std::cout << "⬇️ [" << timestamp << "] Device " << dev_eui << " sent data" << std::endl;
            
            // Filter the message to include only required data points
            std::string filtered_message = filterMessageData(json_str);
            
            // Parse JSON to get message_id for deduplication
            try {
                dom::parser parser;
                auto doc_result = parser.parse(json_str);
                if (doc_result.error()) {
                    std::cerr << "JSON parse error: " << doc_result.error() << std::endl;
                    publisher.publish(filtered_message, "parse_error");
                    return;
                }
                auto doc = doc_result.value();
                
                std::string message_id = "unknown";
                
                // Try correlation_ids first (most common location)
                auto correlation_result = doc["correlation_ids"];
                if (!correlation_result.error() && correlation_result.is_array()) {
                    auto correlation_array = correlation_result.get_array();
                    if (correlation_array.size() > 0) {
                        auto first_corr = correlation_array.at(0);
                        if (!first_corr.error() && first_corr.is_string()) {
                            message_id = std::string(first_corr);
                        }
                    }
                }
                
                // If not found, try uplink_token in rx_metadata
                if (message_id == "unknown") {
                    auto uplink_result = doc["uplink_message"];
                    if (!uplink_result.error()) {
                        auto uplink = uplink_result.value();
                        auto rx_result = uplink["rx_metadata"];
                        if (!rx_result.error() && rx_result.is_array()) {
                            auto rx = rx_result.get_array();
                            if (rx.size() > 0) {
                                auto first_result = rx.at(0);
                                if (!first_result.error()) {
                                    auto first = first_result.value();
                                    auto token_result = first["uplink_token"];
                                    if (!token_result.error() && token_result.is_string()) {
                                        message_id = std::string(token_result);
                                    }
                                }
                            }
                        }
                    }
                }
                
                // If still not found, try the original packet_broker location
                if (message_id == "unknown") {
                    auto uplink_result = doc["uplink_message"];
                    if (!uplink_result.error()) {
                        auto uplink = uplink_result.value();
                        auto rx_result = uplink["rx_metadata"];
                        if (!rx_result.error() && rx_result.is_array()) {
                            auto rx = rx_result.get_array();
                            if (rx.size() > 0) {
                                auto first_result = rx.at(0);
                                if (!first_result.error()) {
                                    auto first = first_result.value();
                                    auto pb_result = first["packet_broker"];
                                    if (!pb_result.error()) {
                                        auto pb = pb_result.value();
                                        auto mid_result = pb["message_id"];
                                        if (!mid_result.error()) {
                                            auto val = mid_result.value();
                                            if (val.is_string()) {
                                                message_id = std::string(val);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if (message_id == "unknown") {
                    message_id = computeDeterministicId(json_str);
                }

                publisher.publish(filtered_message, message_id);
            } catch (const std::exception& e) {
                std::cerr << "JSON parsing error: " << e.what() << std::endl;
                publisher.publish(filtered_message, "exception");
            }
        });
        
        handler->connect_and_subscribe();
        handler->loop();
        handlers.push_back(std::move(handler));
    }

    std::cout << "All producers started. Press Ctrl+C to quit..." << std::endl;

    // Wait for stop signal
    while (!stop) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cleanup
    handlers.clear();  // Handlers will disconnect in destructor

    return 0;
}
