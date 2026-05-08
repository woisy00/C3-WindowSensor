#!/bin/bash

ROOT="/home/homeassistant/firmware-server/firmware"
REPO="woisy00/C3-WindowSensor"
API="https://api.github.com/repos/$REPO/releases/latest"

TOKEN="<github_token>"
FW_SERVER=<server_ip>
MQTT_SERVER=<mqtt_server_ip>


MODEL="BWS-01"
CONFIG_TOPIC="homeassistant/sensor/firmware-server/${MODEL}/config"
FW_TOPIC="aha/firmware-server/${MODEL}/stat_t"
ATTR_TOPIC="aha/firmware-server/${MODEL}/json_attr_t"

CONFIG_MESSAGE=$(cat <<EOF
{
  "name":"${MODEL} firmware version",
  "uniq_id":"firmware-server-${MODEL}_firmware_version",
  "dev":{
    "ids":"firmware-server","name":"Firmware Server"
  },
  "stat_t":"${FW_TOPIC}",
  "json_attr_t":"${ATTR_TOPIC}"
}
EOF
)

FW_SERVER_URL="http://${FW_SERVER}:8000/${MODEL}"


FW_DIR="${ROOT}/${MODEL}"
INDEX="$FW_DIR/latest.json"

mkdir -p "$FW_DIR"

echo "[]" > "$INDEX.tmp"

# Get latest release (authenticated)
data=$(curl -s -H "Authorization: Bearer $TOKEN" -H "Accept: application/vnd.github+json" "$API")

VERSION=$(echo "$data" | jq -r '.tag_name')
ASSET_URL=$(echo "$data" | jq -r '.assets[0].url')
FILE="WindowSensor-$VERSION.bin"
LOCAL_PATH="$FW_DIR/$FILE"
LOCAL_URL="${FW_SERVER_URL}/${FILE}"

echo "Processing $VERSION"
echo $ASSET_URL

# Download firmware (curl only)
if [ ! -f "$LOCAL_PATH" ]; then
    curl -L \
      -H "Authorization: Bearer $TOKEN" \
      -H "Accept: application/octet-stream" \
      -o "$LOCAL_PATH" \
      "$ASSET_URL"

  jq -n --arg v "$VERSION" --arg url "$LOCAL_URL" '{"version":$v,"url":$url}' > "$INDEX"
fi
mosquitto_pub -h "$MQTT_SERVER" -t "$CONFIG_TOPIC" -m "$CONFIG_MESSAGE"
mosquitto_pub -h "$MQTT_SERVER" -t "$ATTR_TOPIC" -m "{ \"url\": \"$LOCAL_URL\" }" -r
#mosquitto_pub -h "$MQTT_SERVER" -t "$FW_TOPIC" -m "{ \"latest_version\": \"$VERSION\", \"release_url\": \"$LOCAL_URL\" }" -r
mosquitto_pub -h "$MQTT_SERVER" -t "$FW_TOPIC" -m "$VERSION" -r
