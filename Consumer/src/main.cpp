#include "LavinMQ.h"
#include "PostgresClient.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <simdjson.h>
#include <string>
#include <thread>
#include <vector>

struct gateway_info {
  std::string gateway_id;
  int rssi;
  double snr;
};

int main() {
  try {
    std::cout << "Loading configuration from config.json" << std::endl;

    simdjson::dom::parser parser;
    simdjson::dom::element config;
    if (auto error = parser.load("config.json").get(config); error) {
      std::cerr << "Unable to load config.json: "
                << simdjson::error_message(error) << std::endl;
      return EXIT_FAILURE;
    }

    // Load LavinMQ configuration & initialize the connection
    auto lavinmq_obj = config["lavinmq"].get_object();
    if (lavinmq_obj.error()) {
      std::cerr << "Missing 'lavinmq' section in config.json" << std::endl;
      return EXIT_FAILURE;
    }
    LavinMQConnection lavinmq{LavinMQConnection::FromJson(lavinmq_obj.value())};
    if (!lavinmq.connect()) {
      return EXIT_FAILURE;
    }
    std::cout << "LavinMQ URI: " << lavinmq.connection_uri() << std::endl;
    std::cout << "Listening for LavinMQ messages on queue '"
              << lavinmq.config().queue_name << "'" << std::endl;

    // Load PostgreSQL configuration & initialize the connection
    auto postgres_obj = config["postgres"].get_object();
    if (postgres_obj.error()) {
      std::cerr << "Missing 'postgres' section in config.json" << std::endl;
      return EXIT_FAILURE;
    }
    PostgresClient pg{PostgresClient::FromJson(postgres_obj.value())};
    if (!pg.connect()) {
      return EXIT_FAILURE;
    }

    while (true) {
      auto messages = lavinmq.readConfiguredQueue();

      if (messages.empty()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        continue;
      }

      for (const auto &message : messages) {
        // std::cout << "[LavinMQ] delivery_tag=" << message.delivery_tag
        //           << " payload=" << message.body << std::endl;

        // convert message.body to a JSON object
        simdjson::dom::element json_msg;
        if (auto error = parser.parse(message.body).get(json_msg); error) {
          std::cerr << "[LavinMQ] Invalid JSON message: "
                    << simdjson::error_message(error) << std::endl;
          lavinmq.nackMessage(
              message.delivery_tag); // or nack with requeue if supported
          continue;
        }

        std::string dev_eui;
        std::string received_at;
        double temperature = 0.0;
        double humidity = 0.0;
        std::vector<gateway_info> gateway_ids = {};

        // Get the dev_eui off of the root object:
        if (auto dev_eui_val = json_msg["dev_eui"].get_string();
            !dev_eui_val.error()) {
          dev_eui = dev_eui_val.value();
        }

        if (dev_eui.empty()) {
          std::cerr << "[LavinMQ] Missing 'dev_eui' property in JSON message"
                    << std::endl;
          lavinmq.ackMessage(message.delivery_tag);
          continue;
        }

        // get the gateway IDs from uplink_message.rx_metadata array
        // Gateway IDs array is now on the root object too
        // Our object looks like this:
        //{"dev_eui":"0025CA000000100A","decoded_payload":{"humidity":61.790000},"gateway_ids":[{"eui":"24E124FFFEF866DE","rssi":-92,"snr":8.750000}],"received_at":"2025-10-24T11:01:25.907863705Z"}

        if (auto gateway_ids_array_res = json_msg["gateway_ids"].get_array();
            !gateway_ids_array_res.error()) {
          auto gateway_ids_array = gateway_ids_array_res.value();
          std::cout << "[LavinMQ] Found gateway_ids array with "
                    << gateway_ids_array.size() << " entries."
                    << std::endl;
          for (simdjson::dom::element item : gateway_ids_array) {
            auto item_obj_res = item.get_object();
            if (item_obj_res.error()) {
              std::cerr << "[LavinMQ] gateway_ids entry is not an object" << std::endl;
              continue;
            }

            auto item_obj = item_obj_res.value();
            auto eui_res = item_obj["eui"].get_string();
            if (eui_res.error()) {
              eui_res = item_obj["gateway_id"].get_string();
            }
            if (eui_res.error()) {
              std::cerr << "[LavinMQ] gateway_ids entry missing 'eui' field" << std::endl;
              continue;
            }

            gateway_info gw_info{};
            gw_info.gateway_id = std::string(eui_res.value());

            if (auto rssi_val = item_obj["rssi"].get_int64();
                !rssi_val.error()) {
              gw_info.rssi = static_cast<int>(rssi_val.value());
            }

            if (auto snr_val = item_obj["snr"].get_double();
                !snr_val.error()) {
              gw_info.snr = snr_val.value();
            }

            gateway_ids.emplace_back(std::move(gw_info));
          }
        }

        // Get the temperature and humidity from uplink_message.decoded_payload
        // decoded_payload is now on the root object too
        if (auto decoded_payload = json_msg["decoded_payload"].get_object();
            !decoded_payload.error()) {
          if (auto temp_val = decoded_payload["temperature_c"].get_double();
              !temp_val.error()) {
            temperature = temp_val.value();
          }
          if (auto hum_val = decoded_payload["humidity"].get_double();
              !hum_val.error()) {
            humidity = hum_val.value();
          }
        }

        // Get the json received_at property from root of object
        if (auto received_at_val = json_msg["received_at"].get_string();
            !received_at_val.error()) {
          received_at = received_at_val.value();
        } else {
          std::cerr
              << "[LavinMQ] Missing 'received_at' property in JSON message"
              << std::endl;
          lavinmq.ackMessage(message.delivery_tag);
          continue;
        }

        try {

          /****************************************************************
           *
           *Here we will grab the device info from cw_devices and cw_device_type
           *For use in inserting into the correct data table
           ****************************************************************/
          pqxx::work txn{pg.connection()}; // Start the transaction
          pqxx::result deviceAndType = txn.exec_params(
              "SELECT * FROM cw_devices as d join cw_device_type as dt on "
              "d.type = dt.id WHERE dev_eui = $1",
              dev_eui);
          txn.commit();
          if (deviceAndType.empty()) {
            std::cerr << "[Postgres] No device found with dev_eui: " << dev_eui
                      << std::endl;
            lavinmq.ackMessage(message.delivery_tag);
            continue;
          }
          pqxx::row_size_type dataTableCol =
              deviceAndType.column_number("data_table_v2");
          auto dataTable =
              "cw_air_data_duplicate"; // deviceAndType[0][dataTableCol].c_str();
                                       // // Here is the data_table_v2 value
                                       // (use this to insert in a real life
                                       // situation)
          if (dataTable == nullptr) {
            std::cerr << "[Postgres] Device with dev_eui: " << dev_eui
                      << " has no data_table_v2 defined" << std::endl;
            lavinmq.ackMessage(message.delivery_tag);
            continue;
          }
          std::cout << "[Postgres] Inserted message into database " << dataTable
                    << std::endl;

          /***************************************************************************
           *
           *
           * Here we will insert the data into the appropriate data table
           *
           ***************************************************************************/
          pqxx::work insert_txn{pg.connection()};
          pqxx::result res = insert_txn.exec_params(
              "INSERT INTO " + std::string(dataTable) +
                  " (dev_eui, created_at, temperature_c, humidity) VALUES "
                  "($1, "
                  "$2, $3, $4)",
              dev_eui, received_at, temperature, humidity);
          insert_txn.commit();
          std::size_t rows = res.affected_rows(); // 1 for one row inserted
          std::cout << "[Postgres] Inserted " << rows << " row(s) into "
                    << dataTable << std::endl;

          // We FAILED to insert the data successfully, NACK the message and
          // continue to something new
          if (rows == 0) {
            std::cerr << "[Postgres] Failed to insert data into " << dataTable
                      << std::endl;
            lavinmq.nackMessage(message.delivery_tag);
            continue;
          }

          /***************************************************************************
           * Here we will update the CW_Devices table with:
           * - last_data_updated_at
           * - primary_data
           * - secondary_data
           ***************************************************************************/
          pqxx::work update_cw_device_txn{pg.connection()};
          pqxx::result update_res = update_cw_device_txn.exec_params(
              "UPDATE cw_devices SET last_data_updated_at = $1, "
              "primary_data = $2, secondary_data = $3 WHERE dev_eui = $4",
              received_at, temperature, humidity, dev_eui);
          update_cw_device_txn.commit();
          std::size_t cw_device_rows_results = update_res.affected_rows();
          std::cout << "[Postgres] Updated " << cw_device_rows_results
                    << " row(s) in cw_devices" << std::endl;

          /***************************************************************************
           * Here we will upsert the cw_device_gateway table
           *
           *
           ***************************************************************************/
          std::size_t total_gateway_rows = 0;
          for (const auto &gateway : gateway_ids) {
            pqxx::work upsert_txn{pg.connection()};
            pqxx::result upsert_res = upsert_txn.exec_params(
                "INSERT INTO cw_device_gateway (dev_eui, gateway_id, "
                "last_update, rssi, snr) "
                "VALUES ($1, $2, $3, $4, $5) "
                "ON CONFLICT (dev_eui, gateway_id) DO UPDATE SET last_update = "
                "EXCLUDED.last_update, rssi = EXCLUDED.rssi, snr = EXCLUDED.snr",
                dev_eui, gateway.gateway_id, received_at, gateway.rssi, gateway.snr);
            total_gateway_rows += upsert_res.affected_rows();
            upsert_txn.commit();
          }
          std::cout << "[Postgres] Upserted " << total_gateway_rows
                    << " row(s) into cw_device_gateway" << std::endl;
          lavinmq.ackMessage(message.delivery_tag);

        } catch (const std::exception &ex) {
          std::cerr << "[Postgres] Database operation failed: " << ex.what()
                    << std::endl;
          // Handle the error (e.g., log it, retry, etc.)
          // check if detail.what contains "already exists", if so, ack the
          // message as it is already there!!!!
          if (std::string(ex.what()).find("already exists") !=
              std::string::npos) {
            lavinmq.ackMessage(message.delivery_tag);
          } else {
            lavinmq.nackMessage(message.delivery_tag);
          }
        }
      }
    }
  } catch (const simdjson::simdjson_error &err) {
    std::cerr << "simdjson error: " << err.what() << std::endl;
    return EXIT_FAILURE;
  } catch (const std::exception &ex) {
    std::cerr << "Unexpected error: " << ex.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
