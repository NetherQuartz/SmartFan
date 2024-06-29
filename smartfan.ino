#include "Matter.h"
#include <app/server/OnboardingCodesUtil.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>

using namespace chip;
using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::endpoint;

/*
States switch logic:

Fan 1 -> Rotation 0
Fan 0 -> Rotation 0

Rotation 0 & Fan 1 -> Fan 1
Rotation 1 & Fan 1 -> Fan 1

Rotation 0 & Fan 0 -> Fan 0
Rotation 1 & Fan 0 -> Fan 1

*/

// Please configure your PINs
const int LED_PIN_1 = 22;
const int LED_PIN_2 = 21;
const int LED_PIN_3 = 2;

const int TOGGLE_BUTTON_PIN_1 = 0;
const int TOGGLE_BUTTON_PIN_2 = 21;

// Debounce for toggle button
const int DEBOUNCE_DELAY = 500;
int last_toggle;

// Cluster and attribute ID used by Matter plugin unit device
const uint32_t ROTATION_CLUSTER_ID = OnOff::Id;
const uint32_t ROTATION_ATTRIBUTE_ID = OnOff::Attributes::OnOff::Id;

const uint32_t FAN_CLUSTER_ID = FanControl::Id;
const uint32_t FAN_SPEED_ATTRIBUTE_ID = FanControl::Attributes::PercentSetting::Id;
const uint32_t FAN_MODE_ATTRIBUTE_ID = FanControl::Attributes::FanMode::Id;

// Endpoint and attribute ref that will be assigned to Matter device
uint16_t plugin_unit_endpoint_id_on_off = 0;
uint16_t plugin_unit_endpoint_id_fan = 0;
attribute_t *attribute_ref_on_off;
attribute_t *attribute_ref_2;


enum class FanSpeed { Zero = 0, Low = 33, Medium = 66, High = 100 };

struct FanState {

  private:
    bool isOn, isRotating;
    FanSpeed speed;

  public:
    FanState() {
      this->isOn = false;
      this->isRotating = false;
      this->speed = FanSpeed::Zero;
    }

    void set_speed(int speed) {
      if (speed == 0) {
        this->speed = FanSpeed::Zero;
      } else if (speed > 0 && speed < 34) {
        this->speed = FanSpeed::Low;
      } else if (speed > 33 && speed < 67) {
        this->speed = FanSpeed::Medium;
      } else if (speed > 66) {
        this->speed = FanSpeed::High;
      }

      if (speed > 0) {
        this->isOn = true;
      } else {
        this->isOn = false;
        this->isRotating = false;
      }
    }

    void set_speed(FanSpeed speed) {
      this->speed = speed;
      if (speed == FanSpeed::Zero) {
        this->isOn = false;
        this->isRotating = false;
      } else {
        this->isOn = true;
      }
    }

  void set_rotation(bool isRotating) {
    this->isRotating = isRotating;
    if (isRotating) {
      this->isOn = true;
      if (this->isRotating)

    } else {

    }
  }

  void set_enabled(bool isEnabled) {
    this->isOn = isEnabled;
    if (!isEnabled) {
      this->isRotating = false;
      this->speed = FanSpeed::Zero;
    }
  }

  bool get_rotation() {
    return this->isRotating;
  }

  bool get_enabled() {
    return this->isOn;
  }

  int get_speed() {
    return this->speed;
  }
};

FanState fanState = FanState();

static void on_device_event(const ChipDeviceEvent *event, intptr_t arg) {}

static esp_err_t on_identification(identification::callback_type_t type, uint16_t endpoint_id,
                                   uint8_t effect_id, uint8_t effect_variant, void *priv_data) {
  return ESP_OK;
}

static esp_err_t on_attribute_update(attribute::callback_type_t type,
                                     uint16_t endpoint_id, uint32_t cluster_id,
                                     uint32_t attribute_id,
                                     esp_matter_attr_val_t *val,
                                     void *priv_data) {
  if (type == attribute::PRE_UPDATE) {
    Serial.print("Update on endpoint: ");
    Serial.print(endpoint_id);
    Serial.print(" cluster: ");
    Serial.print(cluster_id);
    Serial.print(" attribute: ");
    Serial.println(attribute_id);
  }

  if (type == attribute::PRE_UPDATE && cluster_id == ROTATION_CLUSTER_ID && attribute_id == ROTATION_ATTRIBUTE_ID) {
    // We got an plugin unit on/off attribute update!
    bool rot_new_state = val->val.b;
    fanState.set_rotation(rot_new_state);
    digitalWrite(LED_PIN_1, rot_new_state);
    // digitalWrite(LED_PIN_2, rot_new_state);
  }

  if (type == attribute::PRE_UPDATE && cluster_id == FAN_CLUSTER_ID && attribute_id == FAN_SPEED_ATTRIBUTE_ID) {
    auto fan_new_state = val->val.i;
    fanState.set_speed(fan_new_state);
    Serial.print("FAN VALUE: ");
    Serial.println(fan_new_state);
    digitalWrite(LED_PIN_2, fan_new_state > 0);
  }

  if (type == attribute::PRE_UPDATE && cluster_id == FAN_CLUSTER_ID && attribute_id == FAN_MODE_ATTRIBUTE_ID) {
    auto fan_mode_new_state = val->val.b;
    Serial.print("FAN MODE: ");
    Serial.println(fan_mode_new_state);
    digitalWrite(LED_PIN_3, fan_mode_new_state);
  }

  return ESP_OK;
}

void print_endpoint_info(String clusterName, endpoint_t *endpoint) {
  uint16_t endpoint_id = endpoint::get_id(endpoint);
  Serial.print(clusterName);
  Serial.print(" has endpoint: ");
  Serial.println(endpoint_id);
}

void setup() {
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);

  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  pinMode(LED_PIN_3, OUTPUT);

  node::config_t node_config;
  node_t *node = node::create(&node_config, on_attribute_update, on_identification);

  endpoint_t *on_off_endpoint;
  endpoint_t *fan_endpoint;
  cluster_t *cluster;

  on_off_plugin_unit::config_t on_off_plugin_unit_config;
  on_off_plugin_unit_config.on_off.on_off = true;
  on_off_endpoint = on_off_plugin_unit::create(node, &on_off_plugin_unit_config, ENDPOINT_FLAG_NONE, NULL);
  print_endpoint_info("on_off_plugin_unit", on_off_endpoint);

  fan::config_t fan_config;
  fan_endpoint = fan::create(node, &fan_config, ENDPOINT_FLAG_NONE, NULL);
  print_endpoint_info("fan", fan_endpoint);

  // Save on/off attribute reference. It will be used to read attribute value later.
  attribute_ref_on_off = attribute::get(cluster::get(on_off_endpoint, ROTATION_CLUSTER_ID), ROTATION_ATTRIBUTE_ID);
  // attribute_ref_2 = attribute::get(cluster::get(endpoint_2, CLUSTER_ID), ATTRIBUTE_ID);

  // Save generated endpoint id
  plugin_unit_endpoint_id_on_off = endpoint::get_id(on_off_endpoint);
  plugin_unit_endpoint_id_fan = endpoint::get_id(fan_endpoint);

  esp_matter::set_custom_dac_provider(chip::Credentials::Examples::GetExampleDACProvider());
  esp_matter::start(on_device_event);
  PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
}

// Reads plugin unit on/off attribute value
esp_matter_attr_val_t get_onoff_attribute_value(esp_matter::attribute_t *attribute_ref) {
  esp_matter_attr_val_t onoff_value = esp_matter_invalid(NULL);
  attribute::get_val(attribute_ref, &onoff_value);
  return onoff_value;
}

// Sets plugin unit on/off attribute value
void set_onoff_attribute_value(esp_matter_attr_val_t *onoff_value, uint16_t plugin_unit_endpoint_id) {
  attribute::update(plugin_unit_endpoint_id, ROTATION_CLUSTER_ID, ROTATION_ATTRIBUTE_ID, onoff_value);
}

void loop() {
  // esp_matter_attr_val_t onoff_value = get_onoff_attribute_value(attribute_ref_1);
  // set_onoff_attribute_value(&onoff_value, plugin_unit_endpoint_id_1);
}
