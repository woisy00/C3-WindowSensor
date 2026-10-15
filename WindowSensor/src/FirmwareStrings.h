#pragma once

#include <Arduino.h>

namespace FirmwareStrings {

namespace Mqtt {
constexpr char TopicPrefix[] = "aha/";
constexpr char InstallTopicSuffix[] = "/update/install";
constexpr char StateTopicSuffix[] = "/update/state";
constexpr char DiscoveryTopicPrefix[] = "homeassistant/update/";
constexpr char DiscoveryTopicSuffix[] = "/firmware/config";
constexpr char ClientIdPrefix[] = "WindowSensor-";
constexpr char EntityName[] = "Firmware";
constexpr char UniqueIdPrefix[] = "bws01_";
constexpr char InstallPayload[] = "install";
constexpr char LatestVersionTopic[] = "aha/firmware/BWS-01/state";
constexpr char LatestVersionTemplate[] = "{{ value }}";
constexpr char MacConnection[] = "mac";
constexpr char BluetoothConnection[] = "bluetooth";

inline String UpdateTopic(const String& deviceId) {
	return String(TopicPrefix) + deviceId + InstallTopicSuffix;
}

inline String StateTopic(const String& deviceId) {
	return String(TopicPrefix) + deviceId + StateTopicSuffix;
}

inline String DiscoveryTopic(const String& deviceId) {
	return String(DiscoveryTopicPrefix) + deviceId + DiscoveryTopicSuffix;
}
}

namespace Http {
constexpr char UrlPrefix[] = "http";
constexpr char PlainUrlPrefix[] = "http://";
constexpr char SecureUrlPrefix[] = "https://";
constexpr char UserAgentPrefix[] = "WindowSensor/";
constexpr char AcceptHeader[] = "Accept";
constexpr char GitHubMediaType[] = "application/vnd.github+json";
constexpr char FirmwareAssetName[] = "firmware.bin";
}

namespace Json {
constexpr char TagName[] = "tag_name";
constexpr char Assets[] = "assets";
constexpr char Name[] = "name";
constexpr char BrowserDownloadUrl[] = "browser_download_url";
constexpr char Version[] = "version";
constexpr char Url[] = "url";
constexpr char UniqueId[] = "unique_id";
constexpr char CommandTopic[] = "command_topic";
constexpr char PayloadInstall[] = "payload_install";
constexpr char StateTopic[] = "state_topic";
constexpr char LatestVersionTopic[] = "latest_version_topic";
constexpr char LatestVersionTemplate[] = "latest_version_template";
constexpr char Device[] = "device";
constexpr char ConfigurationUrl[] = "configuration_url";
constexpr char Model[] = "model";
constexpr char Manufacturer[] = "manufacturer";
constexpr char Identifiers[] = "identifiers";
constexpr char Connections[] = "connections";
constexpr char SoftwareVersion[] = "sw_version";
constexpr char InstalledVersion[] = "installed_version";
constexpr char LatestVersion[] = "latest_version";
constexpr char InProgress[] = "in_progress";
constexpr char UpdatePercentage[] = "update_percentage";
}

namespace Format {
constexpr char DeviceId[] = "%02x%02x%02x%02x%02x%02x";
constexpr char MacAddress[] = "%02X:%02X:%02X:%02X:%02X:%02X";
constexpr char ErrorSeparator[] = " - ";
}

namespace Log {
constexpr char MqttConnected[] = "Connected to MQTT broker";
constexpr char MqttConnectionFailed[] = "MQTT connection failed: ";
constexpr char ManifestUrlMissing[] = "Firmware manifest URL is not configured";
constexpr char ManifestInitFailed[] = "Could not initialize firmware manifest request";
constexpr char ManifestRequestFailed[] = "Firmware manifest request failed: ";
constexpr char FirmwareAssetMissing[] = "Latest GitHub release does not contain a firmware.bin asset";
constexpr char ManifestFieldsMissing[] = "Firmware manifest must contain version/url fields or GitHub tag_name/assets";
constexpr char ManifestFieldsEmpty[] = "Firmware manifest contains an empty version or download URL";
constexpr char PublishTopic[] = "MQTT publish: topic=";
constexpr char PublishResult[] = ", retained=true, result=";
constexpr char PublishSuccess[] = "success";
constexpr char PublishFailed[] = "failed";
constexpr char PublishPayload[] = "MQTT payload: ";
constexpr char UpdatePayloadInvalid[] = "Invalid firmware update payload: expected an HTTP(S) url and a non-empty version string";
constexpr char UpdateStarting[] = "Starting firmware update from: ";
constexpr char UpdateSucceeded[] = "Firmware update successful. Restarting...";
constexpr char UpdateFailed[] = "Firmware update failed: ";
}

}