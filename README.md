# 🔋 ESP32-C3 Window / Door Sensor (Battery Powered)

A compact, battery‑powered Wi‑Fi window/door sensor built around the ESP32‑C3. It wakes on reed‑switch changes, reports the state via MQTT to Home Assistant, and immediately returns to deep sleep. Designed for multi‑year operation from two AAA cells.

---

## ✅ Features

- 🪟 Door/window state detection via reed switch
- 🔋 Battery voltage monitoring
- ⚡ Ultra‑low‑power deep sleep (~10‑20 µA)
- 📡 MQTT integration with Home Assistant (auto discovery via ArduinoHA)
- 🔄 OTA firmware updates over HTTP
- 🌐 Wi‑Fi configuration portal
- 🔘 Boot button for manual configuration entry
- 💤 Wake on both open and close events

---

## 🔧 Hardware

### Components

| Component              | Notes                                |
|------------------------|--------------------------------------|
| ESP32‑C3 module        | Sparkle IoT XH-C3F             |
| Reed switch            | NO/NC magnetic 2.5x14switch                |
| 2× AAA battery holder  | Alkaline                             |
| 1 MΩ resistor          | Pull‑ups / voltage divider           |
| 470 kΩ resistor        | Battery divider                      |
| 100 nF capacitor       | ADC stabilization                    |
| Tactile button         | Enter configuration mode             |

#### Battery voltage measurement

A high‑impedance divider feeds GPIO4 (ADC) with the battery voltage. A 100 nF cap stabilizes the reading.

```
Battery+
   |
 1M
   |
   +---- ADC (GPIO4)
   |
470k
   |
 GND

ADC
 |
 +----||---- GND
      100nF
```

- Divider ratio: **V_adc = V_bat × 0.319**
- Firmware conversion: **V_bat = V_adc × 3.13**
- Divider current: ≈ 2 µA (negligible in deep sleep)

#### GPIO assignment

| GPIO  | Function                     |
|-------|------------------------------|
| GPIO3 | Reed switch NO               |
| GPIO4 | Battery ADC                  |
| GPIO5 | Configuration button         |
| GPIO8 | Reed switch NC (optional)    |

> Pins may vary depending on the module.

---

## 🧠 Firmware

The code is now organized into several reusable C++ classes (`Config`, `WiFiConnector`, `Hardware`, `HAManager`, `SleepManager`, and utility helpers) so the core behaviour can be shared with other sensors or projects. The main `.ino` file merely wires the objects together and keeps the logic minimal.

The sketch uses the Arduino framework and the following libraries:

- **ArduinoHA** – automatic MQTT discovery for Home Assistant
- **WiFiManager** – captive‑portal for setup
- **PubSubClient** – lightweight MQTT client

### Power saving strategy

Normal wake cycle:

1. Wake on reed interrupt
2. Connect to Wi‑Fi
3. Establish MQTT connection
4. Publish window state and battery voltage
5. Check for pending OTA update command
6. Enter deep sleep

Typical awake time: **0.8–1.5 seconds**.

---

## 📮 MQTT Topics

Base topic: `aha/<device_id>/`

| Purpose            | Topic                                    | Payload            |
|--------------------|------------------------------------------|--------------------|
| Availability       | `aha/…/avty_t`                           | `online`/`offline` |
| Window state       | `aha/…/window/stat_t`                   | `OPEN`/`CLOSED`    |
| Battery voltage    | `aha/…/battery`                          | e.g. `3.01`        |
| OTA update command | `aha/…/update`                           | HTTP URL (URL is fetched by device for OTA) |

Example command to trigger OTA:

```sh
mosquitto_pub \
  -t "aha/48f6eee1dad8/update" \
  -m "http://server/fw/window_sensor_v1.1.0.bin" \
  -r
```

The device stores the URL so it can download on the next wake.

---

## 🏠 Home Assistant Integration

Discovery is automatic via ArduinoHA; the sensor appears as a device with the following entities:

- **binary_sensor.window** – Window/door state (OPEN/CLOSED)
- **sensor.battery_voltage** – Battery voltage in volts (V)
- **sensor.battery_percent** – Battery level in percentage (%)

The device name is configurable via the Wi-Fi portal and will appear in Home Assistant with that name.

### Manual Configuration (Optional)

If auto-discovery fails, you can manually add the sensor in your `configuration.yaml`:

```yaml
mqtt:
  binary_sensor:
    - name: "Window Sensor"
      state_topic: "aha/<device_id>/window/stat_t"
      payload_on: "OPEN"
      payload_off: "CLOSED"
      device_class: window
      availability_topic: "aha/<device_id>/avty_t"
      payload_available: "online"
      payload_not_available: "offline"

  sensor:
    - name: "Battery Voltage"
      state_topic: "aha/<device_id>/battery"
      unit_of_measurement: "V"
      device_class: voltage
      availability_topic: "aha/<device_id>/avty_t"
      payload_available: "online"
      payload_not_available: "offline"

    - name: "Battery Percent"
      state_topic: "aha/<device_id>/battery_percent"
      unit_of_measurement: "%"
      device_class: battery
      availability_topic: "aha/<device_id>/avty_t"
      payload_available: "online"
      payload_not_available: "offline"
```

Replace `<device_id>` with your device's unique ID (ESP32 MAC address).

---

## 🤖 Home Assistant Automations

The repository includes several Home Assistant automations for firmware management and updates. These can be found in the root directory as YAML files. Below are the key automations with their configurations.

### Firmware Update Automations

#### 1. Auto-create Firmware Update Entities (`autocreate-update-entites.yaml`)

This automation automatically creates firmware update entities in Home Assistant when a device publishes its firmware status via MQTT.

```yaml
alias: Auto-create firmware update entities
description: ""
triggers:
  - trigger: mqtt
    options:
      topic: aha/+/firmware/stat_t
      value_template: "{{ value_json }}"
conditions: []
actions:
  - variables:
      device_id: "{{ trigger.payload_json['device_id'] }}"
      state_topic: aha/{{ trigger.payload_json['device_id'] }}/firmware_version/stat_t
      update_topic: "{{ trigger.payload_json['update_topic'] }}"
      model: "{{ trigger.payload_json['model'] }}"
      manufacturer: "{{ trigger.payload_json['manufacturer'] }}"
      in_progress: "{{ trigger.payload_json['in_progress'] }}"
      installed_version: "{{ trigger.payload_json['installed_version'] }}"
      latest_version: "{{ trigger.payload_json['latest_version'] }}"
      install_payload_tpl: |
        {{
          {
            'device_id': device_id ,
            'model': model,
            'update_topic': update_topic,
            'state_topic': state_topic
          } | tojson | replace('"', '\\"')
        }}  
  - if:
      - condition: template
        value_template: "{{ trigger.payload_json['in_progress'] == false }}"
    then:
      - data:
          topic: homeassistant/update/{{ device_id }}/firmware/config
          retain: true
          payload: |
            {
              "name": "Firmware Update",
              "unique_id": "{{ device_id }}_fw_update",

              "state_topic": "{{ state_topic }}",

              "latest_version_topic": "aha/firmware-server/{{model}}/stat_t",
              "latest_version_template": "{{ '{{ value }}' }}",

              "command_topic": "aha/firmware-server/update_device",
              "payload_install": "{{ install_payload_tpl }}",

              "device": {
                "identifiers": "{{ device_id }}"
              }
            }
        action: mqtt.publish
  - action: mqtt.publish
    metadata: {}
    data:
      topic: "{{ state_topic }}"
      payload: |-
        {
          "installed_version": "{{ installed_version }}",
          "in_progress": {{ (in_progress)|lower }}
        }
mode: queued
```

#### 2. Update Window Sensors (`update-window-sensors.yaml`)

This automation triggers firmware updates for window sensors when a command is received via MQTT.

```yaml
alias: Update Window Sensors
description: ""
triggers:
  - trigger: mqtt
    options:
      topic: aha/firmware-server/update_device
conditions: []
actions:
  - variables:
      device_id: "{{ trigger.payload_json['device_id'] }}"
      update_topic: "{{ trigger.payload_json['update_topic'] }}"
      state_topic: "{{ trigger.payload_json['state_topic'] }}"
      model: "{{ trigger.payload_json['model'] }}"
      model_identifier: "{{ trigger.payload_json['model'] | lower | replace('-', '_') }}"
      version: >-
        {{ states('sensor.firmware_server_' ~ model_identifier ~
        '_firmware_version') }}
      url: >-
        {{ state_attr('sensor.firmware_server_' ~ model_identifier ~
        '_firmware_version','url') }}
  - action: mqtt.publish
    metadata: {}
    data:
      topic: "{{ update_topic }}"
      payload: |
        {
          "url": "{{ url }}",
          "version": "{{ version }}",
          "status": "requested"  
        }
      retain: true
  - action: mqtt.publish
    metadata: {}
    data:
      qos: 0
      retain: false
      topic: "{{ state_topic }}"
      payload: |-
        {
          "in_progress": true
        }
mode: single
```

#### 3. Load New Firmware Releases (`load-new-firmware.yaml`)

This automation runs daily to check for and load new firmware releases from a remote server.

```yaml
alias: Load Window Sensor Firmware releases
description: ""
triggers:
  - trigger: time_pattern
    hours: "1"
conditions: []
actions:
  - action: ssh.execute_command
    metadata: {}
    target:
      device_id: 9b937fc9ad72259fef5f89c240635546
    data:
      command: /home/homeassistant/firmware-server/window-sensor-firmware.sh
mode: single
```

### Additional Sensor Automations

In addition to firmware management, you can create custom automations for the window sensor itself. Here are some examples:

#### Window Opened Notification

```yaml
automation:
  - alias: "Window Opened Notification"
    trigger:
      - platform: state
        entity_id: binary_sensor.window
        to: "on"
    action:
      - service: notify.mobile_app_your_phone
        data:
          message: "Window has been opened!"
          title: "Window Alert"
```

#### Low Battery Warning

```yaml
automation:
  - alias: "Window Sensor Low Battery"
    trigger:
      - platform: numeric_state
        entity_id: sensor.battery_percent
        below: 20
    action:
      - service: notify.mobile_app_your_phone
        data:
          message: "Window sensor battery is low: {{ states('sensor.battery_percent') }}%"
          title: "Battery Alert"
```

To use these automations, copy the YAML content into your Home Assistant `automations.yaml` file or import the provided YAML files. Adjust entity IDs and device references to match your setup.

---

## ⚙ Configuration Mode

The Wi‑Fi/MQTT portal is started when:

- No Wi‑Fi configuration is saved, **or**
- Configuration button is held during boot.

### Options in the portal

Configuration parameters are now organized into sections for easier navigation:

- **General** – device name, Wi‑Fi credentials (SSID/password)
- **MQTT** – broker host/user/pass
- **Advanced** – sleep/loop timings and voltage divider values (collapsed by default; click to expand). The divider values are numeric-only inputs so letters cannot be entered.

Older versions showed a flat list; the new layout uses headings so you can quickly find what you need.

---

## 🔋 Battery Life

| Mode          | Current draw        |
|---------------|---------------------|
| Deep sleep    | ~10–20 µA           |
| Wi‑Fi active  | ~120 mA             |

With two AAA alkaline cells, expect **1–2 years** of operation depending on wake frequency.

---

## 🛠 Building the Firmware

A simple PlatformIO configuration works well:

```ini
[env:esp32-c3]
platform = espressif32
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
```

Upload with:

```sh
pio run -t upload
```

---

## 🎯 Project Goals

1. Simple hardware
2. Minimal component count
3. Reliable MQTT integration
4. Long battery life

---

## 📄 License

This project is released under the **MIT License**. See `LICENSE` for details.

---

## 🤝 Contributions

Pull requests and improvements are welcome! Please open an issue or submit a PR with your changes.
