#include <Settings.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <IntParsing.h>
#include <algorithm>
#include <JsonHelpers.h>

#define PORT_POSITION(s) ( s.indexOf(':') )

GatewayConfig::GatewayConfig(uint16_t deviceId, uint16_t port, uint8_t protocolVersion)
  : deviceId(deviceId)
  , port(port)
  , protocolVersion(protocolVersion)
{ }

bool Settings::hasAuthSettings() {
  return adminUsername.length() > 0 && adminPassword.length() > 0;
}

bool Settings::isAutoRestartEnabled() {
  return _autoRestartPeriod > 0;
}

size_t Settings::getAutoRestartPeriod() {
  if (_autoRestartPeriod == 0) {
    return 0;
  }

  return std::max(_autoRestartPeriod, static_cast<size_t>(MINIMUM_RESTART_PERIOD));
}

void Settings::updateDeviceIds(JsonArray& arr) {
  if (arr.success()) {
    this->deviceIds.clear();

    for (size_t i = 0; i < arr.size(); ++i) {
      this->deviceIds.push_back(arr[i]);
    }
  }
}

void Settings::updateGatewayConfigs(JsonArray& arr) {
  if (arr.success()) {
    gatewayConfigs.clear();

    for (size_t i = 0; i < arr.size(); i++) {
      JsonArray& params = arr[i];

      if (params.success() && params.size() == 3) {
        std::shared_ptr<GatewayConfig> ptr = std::make_shared<GatewayConfig>(parseInt<uint16_t>(params[0]), params[1], params[2]);
        gatewayConfigs.push_back(std::move(ptr));
      } else {
        Serial.print(F("Settings - skipped parsing gateway ports settings for element #"));
        Serial.println(i);
      }
    }
  }
}

void Settings::patch(JsonObject& parsedSettings) {
  if (parsedSettings.success()) {
    this->setIfPresent<String>(parsedSettings, "admin_username", adminUsername);
    this->setIfPresent(parsedSettings, "admin_password", adminPassword);
    this->setIfPresent(parsedSettings, "ce_pin", cePin);
    this->setIfPresent(parsedSettings, "csn_pin", csnPin);
    this->setIfPresent(parsedSettings, "reset_pin", resetPin);
    this->setIfPresent(parsedSettings, "led_pin", ledPin);
    this->setIfPresent(parsedSettings, "packet_repeats", packetRepeats);
    this->setIfPresent(parsedSettings, "http_repeat_factor", httpRepeatFactor);
    this->setIfPresent(parsedSettings, "auto_restart_period", _autoRestartPeriod);
    this->setIfPresent(parsedSettings, "mqtt_server", _mqttServer);
    this->setIfPresent(parsedSettings, "mqtt_username", mqttUsername);
    this->setIfPresent(parsedSettings, "mqtt_password", mqttPassword);
    this->setIfPresent(parsedSettings, "mqtt_topic_pattern", mqttTopicPattern);
    this->setIfPresent(parsedSettings, "mqtt_update_topic_pattern", mqttUpdateTopicPattern);
    this->setIfPresent(parsedSettings, "mqtt_state_topic_pattern", mqttStateTopicPattern);
    this->setIfPresent(parsedSettings, "mqtt_client_status_topic", mqttClientStatusTopic);
    this->setIfPresent(parsedSettings, "discovery_port", discoveryPort);
    this->setIfPresent(parsedSettings, "listen_repeats", listenRepeats);
    this->setIfPresent(parsedSettings, "state_flush_interval", stateFlushInterval);
    this->setIfPresent(parsedSettings, "mqtt_state_rate_limit", mqttStateRateLimit);
    this->setIfPresent(parsedSettings, "packet_repeat_throttle_threshold", packetRepeatThrottleThreshold);
    this->setIfPresent(parsedSettings, "packet_repeat_throttle_sensitivity", packetRepeatThrottleSensitivity);
    this->setIfPresent(parsedSettings, "packet_repeat_minimum", packetRepeatMinimum);
    this->setIfPresent(parsedSettings, "enable_automatic_mode_switching", enableAutomaticModeSwitching);
    this->setIfPresent(parsedSettings, "led_mode_packet_count", ledModePacketCount);
    this->setIfPresent(parsedSettings, "hostname", hostname);
    this->setIfPresent(parsedSettings, "wifi_static_ip", wifiStaticIP);
    this->setIfPresent(parsedSettings, "wifi_static_ip_gateway", wifiStaticIPGateway);
    this->setIfPresent(parsedSettings, "wifi_static_ip_netmask", wifiStaticIPNetmask);

    if (parsedSettings.containsKey("rf24_channels")) {
      JsonArray& arr = parsedSettings["rf24_channels"];
      rf24Channels = JsonHelpers::jsonArrToVector<RF24Channel, String>(arr, RF24ChannelHelpers::valueFromName);
    }

    if (parsedSettings.containsKey("rf24_listen_channel")) {
      this->rf24ListenChannel = RF24ChannelHelpers::valueFromName(parsedSettings["rf24_listen_channel"]);
    }

    if (parsedSettings.containsKey("rf24_power_level")) {
      this->rf24PowerLevel = RF24PowerLevelHelpers::valueFromName(parsedSettings["rf24_power_level"]);
    }

    if (parsedSettings.containsKey("led_mode_wifi_config")) {
      this->ledModeWifiConfig = LEDStatus::stringToLEDMode(parsedSettings["led_mode_wifi_config"]);
    }

    if (parsedSettings.containsKey("led_mode_wifi_failed")) {
      this->ledModeWifiFailed = LEDStatus::stringToLEDMode(parsedSettings["led_mode_wifi_failed"]);
    }

    if (parsedSettings.containsKey("led_mode_operating")) {
      this->ledModeOperating = LEDStatus::stringToLEDMode(parsedSettings["led_mode_operating"]);
    }

    if (parsedSettings.containsKey("led_mode_packet")) {
      this->ledModePacket = LEDStatus::stringToLEDMode(parsedSettings["led_mode_packet"]);
    }

    if (parsedSettings.containsKey("radio_interface_type")) {
      this->radioInterfaceType = Settings::typeFromString(parsedSettings["radio_interface_type"]);
    }

    if (parsedSettings.containsKey("device_ids")) {
      JsonArray& arr = parsedSettings["device_ids"];
      updateDeviceIds(arr);
    }
    if (parsedSettings.containsKey("gateway_configs")) {
      JsonArray& arr = parsedSettings["gateway_configs"];
      updateGatewayConfigs(arr);
    }
    if (parsedSettings.containsKey("group_state_fields")) {
      JsonArray& arr = parsedSettings["group_state_fields"];
      groupStateFields = JsonHelpers::jsonArrToVector<GroupStateField, const char*>(arr, GroupStateFieldHelpers::getFieldByName);
    }
  }
}

void Settings::load(Settings& settings) {
  if (SPIFFS.exists(SETTINGS_FILE)) {
    // Clear in-memory settings
    settings = Settings();

    File f = SPIFFS.open(SETTINGS_FILE, "r");

    DynamicJsonBuffer jsonBuffer;
    JsonObject& parsedSettings = jsonBuffer.parseObject(f);
    settings.patch(parsedSettings);

    f.close();
  } else {
    settings.save();
  }
}

String Settings::toJson(const bool prettyPrint) {
  String buffer = "";
  StringStream s(buffer);
  serialize(s, prettyPrint);
  return buffer;
}

void Settings::save() {
  File f = SPIFFS.open(SETTINGS_FILE, "w");

  if (!f) {
    Serial.println(F("Opening settings file failed"));
  } else {
    serialize(f);
    f.close();
  }
}

void Settings::serialize(Stream& stream, const bool prettyPrint) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  root["admin_username"] = this->adminUsername;
  root["admin_password"] = this->adminPassword;
  root["ce_pin"] = this->cePin;
  root["csn_pin"] = this->csnPin;
  root["reset_pin"] = this->resetPin;
  root["led_pin"] = this->ledPin;
  root["radio_interface_type"] = typeToString(this->radioInterfaceType);
  root["packet_repeats"] = this->packetRepeats;
  root["http_repeat_factor"] = this->httpRepeatFactor;
  root["auto_restart_period"] = this->_autoRestartPeriod;
  root["mqtt_server"] = this->_mqttServer;
  root["mqtt_username"] = this->mqttUsername;
  root["mqtt_password"] = this->mqttPassword;
  root["mqtt_topic_pattern"] = this->mqttTopicPattern;
  root["mqtt_update_topic_pattern"] = this->mqttUpdateTopicPattern;
  root["mqtt_state_topic_pattern"] = this->mqttStateTopicPattern;
  root["mqtt_client_status_topic"] = this->mqttClientStatusTopic;
  root["discovery_port"] = this->discoveryPort;
  root["listen_repeats"] = this->listenRepeats;
  root["state_flush_interval"] = this->stateFlushInterval;
  root["mqtt_state_rate_limit"] = this->mqttStateRateLimit;
  root["packet_repeat_throttle_sensitivity"] = this->packetRepeatThrottleSensitivity;
  root["packet_repeat_throttle_threshold"] = this->packetRepeatThrottleThreshold;
  root["packet_repeat_minimum"] = this->packetRepeatMinimum;
  root["enable_automatic_mode_switching"] = this->enableAutomaticModeSwitching;
  root["led_mode_wifi_config"] = LEDStatus::LEDModeToString(this->ledModeWifiConfig);
  root["led_mode_wifi_failed"] = LEDStatus::LEDModeToString(this->ledModeWifiFailed);
  root["led_mode_operating"] = LEDStatus::LEDModeToString(this->ledModeOperating);
  root["led_mode_packet"] = LEDStatus::LEDModeToString(this->ledModePacket);
  root["led_mode_packet_count"] = this->ledModePacketCount;
  root["hostname"] = this->hostname;
  root["rf24_power_level"] = RF24PowerLevelHelpers::nameFromValue(this->rf24PowerLevel);
  root["rf24_listen_channel"] = RF24ChannelHelpers::nameFromValue(rf24ListenChannel);
  root["wifi_static_ip"] = this->wifiStaticIP;
  root["wifi_static_ip_gateway"] = this->wifiStaticIPGateway;
  root["wifi_static_ip_netmask"] = this->wifiStaticIPNetmask;

  JsonArray& channelArr = root.createNestedArray("rf24_channels");
  JsonHelpers::vectorToJsonArr<RF24Channel, String>(channelArr, rf24Channels, RF24ChannelHelpers::nameFromValue);

  JsonArray& deviceIdsArr = root.createNestedArray("device_ids");
  deviceIdsArr.copyFrom<uint16_t>(this->deviceIds.data(), this->deviceIds.size());

  JsonArray& gatewayConfigsArr = root.createNestedArray("gateway_configs");
  for (size_t i = 0; i < this->gatewayConfigs.size(); i++) {
    JsonArray& elmt = jsonBuffer.createArray();
    elmt.add(this->gatewayConfigs[i]->deviceId);
    elmt.add(this->gatewayConfigs[i]->port);
    elmt.add(this->gatewayConfigs[i]->protocolVersion);
    gatewayConfigsArr.add(elmt);
  }

  JsonArray& groupStateFieldArr = root.createNestedArray("group_state_fields");
  JsonHelpers::vectorToJsonArr<GroupStateField, const char*>(groupStateFieldArr, groupStateFields, GroupStateFieldHelpers::getFieldName);

  if (prettyPrint) {
    root.prettyPrintTo(stream);
  } else {
    root.printTo(stream);
  }
}

String Settings::mqttServer() {
  int pos = PORT_POSITION(_mqttServer);

  if (pos == -1) {
    return _mqttServer;
  } else {
    return _mqttServer.substring(0, pos);
  }
}

uint16_t Settings::mqttPort() {
  int pos = PORT_POSITION(_mqttServer);

  if (pos == -1) {
    return DEFAULT_MQTT_PORT;
  } else {
    return atoi(_mqttServer.c_str() + pos + 1);
  }
}

RadioInterfaceType Settings::typeFromString(const String& s) {
  if (s.equalsIgnoreCase("lt8900")) {
    return LT8900;
  } else {
    return nRF24;
  }
}

String Settings::typeToString(RadioInterfaceType type) {
  switch (type) {
    case LT8900:
      return "LT8900";

    case nRF24:
    default:
      return "nRF24";
  }
}
