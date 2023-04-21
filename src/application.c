// Tower Kit documentation https://tower.hardwario.com/
// SDK API description https://sdk.hardwario.com/
// Forum https://forum.hardwario.com/

#include <application.h>

#define UPDATE_INTERVAL (10 * 60 * 1000)

//#define UPDATE_INTERVAL (10000)

// LED instance
twr_led_t led;

// Button instance
twr_button_t button;

// Thermometer instance
twr_tmp112_t tmp112;
float core_tmp112_value = 0;

twr_tag_humidity_t humidity_tag;
float tag_humidity = 0;

uint16_t button_click_count = 0;

twr_scheduler_task_id_t send_data_task_id;

struct tm datetime;

float lastTemperature = 0;
float lastHumidity = 0;

// Button event callback
void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    // Log button event
    twr_log_info("APP: Button event: %i", event);

    // Check event source
    if (event == TWR_BUTTON_EVENT_CLICK)
    {
        // Toggle LED pin state
        twr_led_set_mode(&led, TWR_LED_MODE_TOGGLE);

        // Publish message on radio
        button_click_count++;
        twr_radio_pub_push_button(&button_click_count);
    }
}

void humidity_tag_event_handler(twr_tag_humidity_t *self, twr_tag_humidity_event_t event, void *event_param)
{
    if (event == TWR_TAG_HUMIDITY_EVENT_UPDATE)
    {
        float value;
        if (twr_tag_humidity_get_humidity_percentage(self, &value))
        {
            tag_humidity = value;
            twr_log_debug("HUMIDITY: %.2f", tag_humidity);
        }
    }
}

void tmp112_event_handler(twr_tmp112_t *self, twr_tmp112_event_t event, void *event_param)
{
    if (event == TWR_TMP112_EVENT_UPDATE)
    {
        float value;
        if (twr_tmp112_get_temperature_celsius(self, &value))
        {
            core_tmp112_value = value;
            twr_log_debug("TEMPERATURE: %.2f", core_tmp112_value);
        }
    }
}

void send_data_task()
{
    twr_log_debug("SENDING DATA");

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "{\\\"temperature\\\":%.2f,\\\"humidity\\\":%.2f}", core_tmp112_value, tag_humidity);
    twr_log_debug("DATA: %s", buffer);
    twr_radio_pub_string("data", buffer);

    lastHumidity = tag_humidity;
    lastTemperature = core_tmp112_value;

    twr_scheduler_plan_current_relative(UPDATE_INTERVAL + 1000);
}

// Application initialization function which is called once after boot
void application_init(void)
{
    __HAL_RCC_CRC_CLK_ENABLE();

    /*struct tm datetime;

    datetime.tm_hour = 12;
    datetime.tm_min = 43;
    datetime.tm_sec = 00;

    datetime.tm_mon = 03;
    datetime.tm_mday = 20;
    datetime.tm_year = 123;

    twr_rtc_set_datetime(&datetime, 0);*/

    // Initialize logging
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, 0);
    twr_led_pulse(&led, 2000);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, 0);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize thermometer on core module
    twr_tmp112_init(&tmp112, TWR_I2C_I2C0, TWR_TAG_TEMPERATURE_I2C_ADDRESS_ALTERNATE);
    twr_tmp112_set_event_handler(&tmp112, tmp112_event_handler, NULL);
    twr_tmp112_set_update_interval(&tmp112, UPDATE_INTERVAL);

    twr_tag_humidity_init(&humidity_tag, TWR_TAG_HUMIDITY_REVISION_R3, TWR_I2C_I2C0, TWR_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);
    twr_tag_humidity_set_event_handler(&humidity_tag, humidity_tag_event_handler, NULL);
    twr_tag_humidity_set_update_interval(&humidity_tag, UPDATE_INTERVAL);

    // Initialize radio
    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);

    send_data_task_id = twr_scheduler_register(send_data_task, NULL, UPDATE_INTERVAL + 1000);

    // Send radio pairing request
    twr_radio_pairing_request("ai-data-collector", FW_VERSION);
}
