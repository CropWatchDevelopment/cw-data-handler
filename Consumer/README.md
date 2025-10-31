# cw-data-transfer

Modern C++17 starter project that pulls in [simdjson](https://github.com/simdjson/simdjson), [libpqxx](https://github.com/jtv/libpqxx), and [rabbitmq-c](https://github.com/alanxz/rabbitmq-c) automatically through CMake `FetchContent`. All `*.cpp` files in `src/` are detected automatically, so you can drop new translation units in without editing `CMakeLists.txt`.

## Prerequisites

- CMake **3.20+**
- A C++17-capable compiler (GCC 11+, Clang 12+, or MSVC 2019+)
- Git (required for `FetchContent` to clone dependencies)
- Development headers for PostgreSQL client library `libpq`
  - On Debian/Ubuntu: `sudo apt install libpq-dev`
- Optional but recommended on Linux: `pkg-config`, `libssl-dev`, and `zlib1g-dev` (improves RabbitMQ feature detection)

## Configuration

The sample executable expects a `config.json` file in the project root. It ships with sensible defaults—edit it before running anything that talks to real services.

```jsonc
{
  "postgres": {
    "host": "localhost",
    "port": 5432,
    "database": "cw_data",
    "user": "cw_user",
    "password": "change-me",
    "sslmode": "prefer",
    "connect_timeout": 5
  },
  "lavinmq": {
    "host": "localhost",
    "port": 5672,
    "username": "guest",
    "password": "guest",
    "virtual_host": "/",
    "heartbeat": 60,
    "connection_name": "cw-data-transfer",
    "tls": false
  },
  "mqtt_connections": [
    {
      "name": "ttn-eu1",
      "host": "eu1.cloud.thethings.network",
      "port": 8883,
      "username": "your-app-id@ttn",
      "password": "NNSXS.YOUR-TTN-API-KEY",
      "client_id": "cw-data-transfer-ttn",
      "keep_alive": 30,
      "use_tls": true,
      "clean_session": true,
      "ca_cert": "/etc/ssl/certs/ca-certificates.crt",
      "client_cert": "",
      "client_key": "",
      "tls_insecure": false,
      "topics": [
        {
          "filter": "v3/your-app-id@ttn/devices/+/up",
          "qos": 1
        }
      ]
    },
    {
      "name": "local-mqtt",
      "host": "localhost",
      "port": 1883,
      "username": "guest",
      "password": "guest",
      "client_id": "cw-data-transfer-local",
      "keep_alive": 60,
      "use_tls": false,
      "clean_session": true,
      "ca_cert": "",
      "client_cert": "",
      "client_key": "",
      "tls_insecure": false,
      "topics": [
        {
          "filter": "sensors/#",
          "qos": 0
        }
      ]
    }
  ]
}
```

- `mqtt_connections` is an array so you can fan in from multiple MQTT brokers at once. Each entry describes the broker address, credentials, TLS material, and topics (with QoS) to subscribe to. The sample shows how to wire The Things Network's EU cluster. Replace the placeholder API key with your **NNSXS** token and point `ca_cert` to a CA bundle that trusts TTN's server certificate (for Debian/Ubuntu the default bundle is `/etc/ssl/certs/ca-certificates.crt`).
- Set `tls_insecure` to `true` only when connecting to brokers with self-signed certificates in controlled environments.
- The executable keeps MQTT connections alive briefly (5 seconds) so you can verify connectivity quickly; expand this section to meet your ingestion needs.

> 💡 Avoid checking real credentials into version control—consider using environment-specific overrides, secret stores, or templated copies of this file as you move toward production.

## Configure & build

Presets make setup repeatable. From the workspace root:

```bash
cmake --preset debug
cmake --build --preset debug
```

Run the sample executable:

```bash
./build/debug/cw-data-transfer
```

Use the `release` preset for optimized builds:

```bash
cmake --preset release
cmake --build --preset release --target cw-data-transfer
```

## VS Code integration

- Open the folder in VS Code with the [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) and [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) extensions installed (already recommended in `.vscode/extensions.json`).
- CMake will auto-configure on open using `CMakePresets.json`.
- Hit <kbd>F5</kbd> to build (if needed) and launch the target under the `cppdbg` debug configuration defined in `.vscode/launch.json`.

## Project layout

```
.
├── CMakeLists.txt         # Project configuration with dependency fetching
├── CMakePresets.json      # Debug/release presets for CLI and VS Code
├── config.json            # Connection settings for PostgreSQL, LavinMQ, and MQTT brokers
├── include/               # Header files you add will be picked up automatically
├── src/
│   └── main.cpp           # Sample program wiring in simdjson, libpqxx, rabbitmq-c
└── .vscode/
    ├── launch.json        # F5 launch configuration
    ├── settings.json      # CMake Tools preferences
    └── extensions.json    # Helpful extension recommendations
```

## Next steps

- Add your own source files anywhere under `src/`—they are included automatically via CMake file globbing.
- Fill out `include/` with shared headers and mark them with include guards or `#pragma once`.
- Extend `main.cpp` or break the logic into separate modules as the project grows.

Happy hacking!
