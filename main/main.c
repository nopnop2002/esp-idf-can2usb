/* USB Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// DESCRIPTION:
// This example contains minimal code to make ESP32-S2 based device
// recognizable by USB-host devices as a USB Serial Device.

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_vfs.h"
#include "nvs_flash.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "sdkconfig.h"
#include "esp_spiffs.h" 
#include "driver/twai.h" // Update from V4.2

#include "cJSON.h"

#include "twai.h"

static const char *TAG = "MAIN";
//static uint8_t buf[CONFIG_USB_CDC_RX_BUFSIZE + 1];
static uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];

static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

#if CONFIG_CAN_BITRATE_25
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_25KBITS();
#define BITRATE "Bitrate is 25 Kbit/s"
#elif CONFIG_CAN_BITRATE_50
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_50KBITS();
#define BITRATE "Bitrate is 50 Kbit/s"
#elif CONFIG_CAN_BITRATE_100
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_100KBITS();
#define BITRATE "Bitrate is 100 Kbit/s"
#elif CONFIG_CAN_BITRATE_125
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
#define BITRATE "Bitrate is 125 Kbit/s"
#elif CONFIG_CAN_BITRATE_250
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
#define BITRATE "Bitrate is 250 Kbit/s"
#elif CONFIG_CAN_BITRATE_500
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
#define BITRATE "Bitrate is 500 Kbit/s"
#elif CONFIG_CAN_BITRATE_800
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_800KBITS();
#define BITRATE "Bitrate is 800 Kbit/s"
#elif CONFIG_CAN_BITRATE_1000
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();
#define BITRATE "Bitrate is 1 Mbit/s"
#endif

QueueHandle_t xQueue_usb;

TOPIC_t *publish;
int16_t	npublish;

bool isConnected = false;

void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
	/* initialization */
	size_t rx_size = 0;

	/* read */
	//esp_err_t ret = tinyusb_cdcacm_read(itf, buf, CONFIG_USB_CDC_RX_BUFSIZE, &rx_size);
	esp_err_t ret = tinyusb_cdcacm_read(itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
	if (ret == ESP_OK) {
		buf[rx_size] = '\0';
		ESP_LOGI(TAG, "Got data (%d bytes): %s", rx_size, buf);
	} else {
		ESP_LOGE(TAG, "Read error");
	}

#if 0
	/* write back */
	tinyusb_cdcacm_write_queue(itf, buf, rx_size);
	tinyusb_cdcacm_write_flush(itf, 0);
#endif
}

void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event)
{
	int dtr = event->line_state_changed_data.dtr;
	int rst = event->line_state_changed_data.rts;
	ESP_LOGI(TAG, "Line state changed! dtr:%d, rst:%d", dtr, rst);
	if (dtr == 1 && rst == 1) {
		isConnected = true;
	} else {
		isConnected = false;
	}
}

esp_err_t mountSPIFFS(char * partition_label, char * base_path) {
	ESP_LOGI(TAG, "Initializing SPIFFS file system");

	esp_vfs_spiffs_conf_t conf = {
		.base_path = base_path,
		.partition_label = partition_label,
		.max_files = 5,
		.format_if_mount_failed = true
	};

	// Use settings defined above to initialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
		DIR* dir = opendir(base_path);
		assert(dir != NULL);
		while (true) {
			struct dirent*pe = readdir(dir);
			if (!pe) break;
			ESP_LOGI(TAG, "d_name=%s d_ino=%d d_type=%x", pe->d_name,pe->d_ino, pe->d_type);
		}
		closedir(dir);
	}
	ESP_LOGI(TAG, "Mount SPIFFS filesystem");
	return ret;
}

esp_err_t build_table(TOPIC_t **topics, char *file, int16_t *ntopic)
{
	ESP_LOGI(TAG, "build_table file=%s", file);
	char line[128];
	int _ntopic = 0;

	FILE* f = fopen(file, "r");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return ESP_FAIL;
	}
	while (1){
		if ( fgets(line, sizeof(line) ,f) == 0 ) break;
		// strip newline
		char* pos = strchr(line, '\n');
		if (pos) {
			*pos = '\0';
		}
		ESP_LOGD(TAG, "line=[%s]", line);
		if (strlen(line) == 0) continue;
		if (line[0] == '#') continue;
		_ntopic++;
	}
	fclose(f);
	ESP_LOGI(TAG, "build_table _ntopic=%d", _ntopic);
	
	*topics = calloc(_ntopic, sizeof(TOPIC_t));
	if (*topics == NULL) {
		ESP_LOGE(TAG, "Error allocating memory for topic");
		return ESP_ERR_NO_MEM;
	}

	f = fopen(file, "r");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return ESP_FAIL;
	}

	char *ptr;
	int index = 0;
	while (1){
		if ( fgets(line, sizeof(line) ,f) == 0 ) break;
		// strip newline
		char* pos = strchr(line, '\n');
		if (pos) {
			*pos = '\0';
		}
		ESP_LOGD(TAG, "line=[%s]", line);
		if (strlen(line) == 0) continue;
		if (line[0] == '#') continue;

		// Frame type
		ptr = strtok(line, ",");
		ESP_LOGD(TAG, "ptr=%s", ptr);
		if (strcmp(ptr, "S") == 0) {
			(*topics+index)->frame = 0;
		} else if (strcmp(ptr, "E") == 0) {
			(*topics+index)->frame = 1;
		} else {
			ESP_LOGE(TAG, "This line is invalid [%s]", line);
			continue;
		}

		// CAN ID
		uint32_t canid;
		ptr = strtok(NULL, ",");
		if(ptr == NULL) continue;
		ESP_LOGD(TAG, "ptr=%s", ptr);
		canid = strtol(ptr, NULL, 16);
		if (canid == 0) {
			ESP_LOGE(TAG, "This line is invalid [%s]", line);
			continue;
		}
		(*topics+index)->canid = canid;

		// mqtt topic
		char *sp;
		ptr = strtok(NULL, ",");
		if(ptr == NULL) {
			ESP_LOGE(TAG, "This line is invalid [%s]", line);
			continue;
		}
		ESP_LOGD(TAG, "ptr=[%s] strlen=%d", ptr, strlen(ptr));
		sp = strstr(ptr,"#");
		if (sp != NULL) {
			ESP_LOGE(TAG, "This line is invalid [%s]", line);
			continue;
		}
		sp = strstr(ptr,"+");
		if (sp != NULL) {
			ESP_LOGE(TAG, "This line is invalid [%s]", line);
			continue;
		}
		(*topics+index)->topic = (char *)malloc(strlen(ptr)+1);
		strcpy((*topics+index)->topic, ptr);
		(*topics+index)->topic_len = strlen(ptr);
		index++;
	}
	fclose(f);
	*ntopic = index;
	return ESP_OK;
}

void dump_table(TOPIC_t *topics, int16_t ntopic)
{
	for(int i=0;i<ntopic;i++) {
		ESP_LOGI(pcTaskGetName(0), "topics[%d] frame=%d canid=0x%"PRIx32" topic=[%s] topic_len=%d",
		i, (topics+i)->frame, (topics+i)->canid, (topics+i)->topic, (topics+i)->topic_len);
	}

}

void twai_task(void *pvParameters);

void app_main(void)
{
	ESP_LOGI(TAG, "USB initialization");
	tinyusb_config_t tusb_cfg = {}; // the configuration using default values
	ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

	tinyusb_config_cdcacm_t amc_cfg = {
		.usb_dev = TINYUSB_USBDEV_0,
		.cdc_port = TINYUSB_CDC_ACM_0,
		.rx_unread_buf_sz = 64,
		.callback_rx = &tinyusb_cdc_rx_callback, // the first way to register a callback
		.callback_rx_wanted_char = NULL,
		.callback_line_state_changed = NULL,
		.callback_line_coding_changed = NULL
	};

	ESP_ERROR_CHECK(tusb_cdc_acm_init(&amc_cfg));

	/* the second way to register a callback */
	ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
						TINYUSB_CDC_ACM_0,
						CDC_EVENT_LINE_STATE_CHANGED,
						&tinyusb_cdc_line_state_changed_callback));
	ESP_LOGI(TAG, "USB initialization DONE");

	// Mount SPIFFS
	char *partition_label = "storage";
	char *base_path = "/spiffs"; 
	esp_err_t ret = mountSPIFFS(partition_label, base_path);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "mountSPIFFS fail");
		while(1) { vTaskDelay(1); }
	}

	// Install and start TWAI driver
	ESP_LOGI(TAG, "%s",BITRATE);
	ESP_LOGI(TAG, "CTX_GPIO=%d",CONFIG_CTX_GPIO);
	ESP_LOGI(TAG, "CRX_GPIO=%d",CONFIG_CRX_GPIO);

	static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CONFIG_CTX_GPIO, CONFIG_CRX_GPIO, TWAI_MODE_NORMAL);
	ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
	ESP_LOGI(TAG, "Driver installed");
	ESP_ERROR_CHECK(twai_start());
	ESP_LOGI(TAG, "Driver started");

	// Create Queue
	xQueue_usb = xQueueCreate( 10, sizeof(FRAME_t) );
	configASSERT( xQueue_usb );

	// build publish table
	ret = build_table(&publish, "/spiffs/can2usb.csv", &npublish);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "build publish table fail");
		while(1) { vTaskDelay(1); }
	}
	dump_table(publish, npublish);

	xTaskCreate(twai_task, "twai_rx", 1024*6, NULL, 2, NULL);

	FRAME_t frameBuf;
	while(1) {
		xQueueReceive(xQueue_usb, &frameBuf, portMAX_DELAY);
		ESP_LOGI(TAG, "isConnected=%d canid=0x%"PRIx32" ext=%d topic=[%s]",
			isConnected, frameBuf.canid, frameBuf.ext, frameBuf.topic);
		for(int i=0;i<frameBuf.data_len;i++) {
			ESP_LOGI(TAG, "DATA=%x", frameBuf.data[i]);
		}
	
		if (isConnected == false) continue;

		// build JSON string
		cJSON *root;
		root = cJSON_CreateObject();
		cJSON_AddNumberToObject(root, "canid", frameBuf.canid);
		if (frameBuf.ext == 0) {
			cJSON_AddStringToObject(root, "frame", "standard");
		} else {
			cJSON_AddStringToObject(root, "frame", "extended");
		}
		cJSON *dataArray;
		dataArray = cJSON_CreateArray();
		cJSON_AddItemToObject(root, "data", dataArray);
		for(int i=0;i<frameBuf.data_len;i++) {
			cJSON *dataItem = NULL;
			dataItem = cJSON_CreateNumber(frameBuf.data[i]);
			cJSON_AddItemToArray(dataArray, dataItem);
		}
		//char *json_string = cJSON_Print(root);
		char *json_string = cJSON_PrintUnformatted(root);
		ESP_LOGI(TAG, "json_string\n%s",json_string);
		cJSON_Delete(root);

		/* write to USB */
		uint8_t crlf[2] = { 0x0d, 0x0a };
		tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, (uint8_t *)json_string, strlen(json_string));
		tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, crlf, 2);
		tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
		
		cJSON_free(json_string);
	}
}
