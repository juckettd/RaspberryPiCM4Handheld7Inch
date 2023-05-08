//-------------------------------------------------------------------------
//
// The MIT License (MIT)
//
// Copyright (c) 2013 Andrew Duncan
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//-------------------------------------------------------------------------

#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
//#include <pigpio.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <alsa/asoundlib.h>
#include <sys/reboot.h>

#include "backgroundLayer.h"
#include "imageLayer.h"
#include "key.h"
#include "loadpng.h"
#include "bcm_host.h"

#ifndef ICON_BUF
#define ICON_BUF 200
#endif

#ifndef MAX_BUF
#define MAX_BUF 400
#endif

//-------------------------------------------------------------------------

#define NDEBUG

//-------------------------------------------------------------------------

const char *program = NULL;
char iconpath[ICON_BUF] = "/usr/local/bin/overlay_icons/";
//char iconpath[ICON_BUF];

char env_icons[3][MAX_BUF];
char wifi_icons[6][MAX_BUF];
char bt_icons[2][MAX_BUF];
char bar_icons[3][MAX_BUF];
char critical_battery_icon[MAX_BUF];
char battery_icons[10][MAX_BUF];
char color[] = "white";
char wifi_carrier[] = "/sys/class/net/wlan0/carrier"; // 1 when wifi connected, 0 when disconnected and/or ifdown
char wifi_linkmode[] = "/sys/class/net/wlan0/link_mode"; // 1 when ifup, 0 when ifdown
char bt_devices_dir[] = "/sys/class/bluetooth";
char backlight[] = "/sys/class/backlight/rpi_backlight/brightness";
char env_cmd[] = "vcgencmd get_throttled";
char temp_cmd[] = "vcgencmd measure_temp";

int dpi = 24;
int fan_on_temp = 60;
int battery_index = -1;
int wifi_index = -1;
int bt_index = -1;
int under_voltage = 0;
int freq_capped = 0;
int throttled = 0;
int critical_shutdown = 0;
int percent_levels[9] = {5, 10, 20, 30, 50, 60, 80, 90, 100};
char battery_level[] = "blah";

int pmin = 2;
uint32_t displayNumber = 0;
uint32_t powerButtonTime = 2000000;
uint32_t volbriBarTime = 3000000;
uint32_t miscButtonTime = 2000000;
uint32_t incdecButtonTime = 500000;
uint32_t criticalShutdownTime = 60000000;
uint32_t fanMinTime = 60000000;

int battery_charger_handle = 0;
int fuel_gauge_handle = 0;

volatile int display_brightness = 0;
volatile int display_volume = 0;
volatile uint32_t display_tick = 0;

volatile int decrease_pressed = 0;
volatile uint32_t decrease_tick = 0;
volatile int increase_pressed = 0;
volatile uint32_t increase_tick = 0;
volatile int misc_pressed = 0;
volatile int misc_alt = 0;
volatile uint32_t misc_tick = 0;
volatile int power_pressed = 0;
volatile uint32_t power_tick = 0;
volatile uint32_t critical_shutdown_tick = 0;

IMAGE_LAYER_T critical_battery_img;
IMAGE_LAYER_T battery_img[10];
IMAGE_LAYER_T wifi_img[6];
IMAGE_LAYER_T bluetooth_img[2];
IMAGE_LAYER_T under_voltage_img;
IMAGE_LAYER_T freq_capped_img;
IMAGE_LAYER_T throttled_img;
IMAGE_LAYER_T volume_bar_img;
IMAGE_LAYER_T brightness_bar_img;
IMAGE_LAYER_T dot_img;

DISPMANX_DISPLAY_HANDLE_T display;
DISPMANX_UPDATE_HANDLE_T update;
DISPMANX_MODEINFO_T info;

snd_mixer_t* handle = NULL;
snd_mixer_elem_t* elem = NULL;

//-------------------------------------------------------------------------

volatile bool run = true;
volatile bool shutdown = false;
volatile bool restart = false;

void gpioISR7();
void gpioISR8();
void gpioISR9();
void gpioISR10();
//void gpioISR(int gpio, int level, uint32_t tick);

//-------------------------------------------------------------------------

static void
signalHandler(
	int signalNumber)
{
	switch (signalNumber)
	{
		case SIGINT:
		case SIGTERM:

			run = false;
			break;
	};
}

//-------------------------------------------------------------------------

void setupDisplay()
{
	if (signal(SIGINT, signalHandler) == SIG_ERR)
	{
		perror("installing SIGINT signal handler");
		exit(EXIT_FAILURE);
	}

	//---------------------------------------------------------------------

	if (signal(SIGTERM, signalHandler) == SIG_ERR)
	{
		perror("installing SIGTERM signal handler");
		exit(EXIT_FAILURE);
	}

	//---------------------------------------------------------------------

	bcm_host_init();

	//---------------------------------------------------------------------

	display = vc_dispmanx_display_open(displayNumber);
	assert(display != 0);

	//---------------------------------------------------------------------

	int result = vc_dispmanx_display_get_info(display, &info);
	assert(result == 0);
}

void setupFiles()
{
	char levels[9][MAX_BUF] = {"alert_red", "alert", "20", "30", "50", "60", "80", "90", "full" };

	//getcwd(iconpath, ICON_BUF);
	//strcat(iconpath, "/overlay_icons/");

	for (int i = 0; i < 9; i++)
		sprintf(battery_icons[i], "%s%s%s%s%s%s%d%s", iconpath, "twotone_battery_", levels[i], "_", color, "_",  dpi, "dp.png");
	sprintf(battery_icons[9], "%s%s%s%s%d%s", iconpath, "twotone_battery_charging_", color, "_",  dpi, "dp.png");

	sprintf(env_icons[0], "%s%s%d%s", iconpath, "flash_", dpi, "dp.png");
	sprintf(env_icons[1], "%s%s%d%s", iconpath, "thermometer_", dpi, "dp.png");
	sprintf(env_icons[2], "%s%s%d%s", iconpath, "thermometer-lines_", dpi, "dp.png");

	sprintf(wifi_icons[0], "%s%s%s%s%d%s", iconpath, "twotone_signal_wifi_off_", color, "_", dpi, "dp.png");
	sprintf(wifi_icons[1], "%s%s%s%s%d%s", iconpath, "twotone_signal_wifi_0_bar_", color, "_", dpi, "dp.png");
	sprintf(wifi_icons[2], "%s%s%s%s%d%s", iconpath, "twotone_signal_wifi_1_bar_", color, "_", dpi, "dp.png");
	sprintf(wifi_icons[3], "%s%s%s%s%d%s", iconpath, "twotone_signal_wifi_2_bar_", color, "_", dpi, "dp.png");
	sprintf(wifi_icons[4], "%s%s%s%s%d%s", iconpath, "twotone_signal_wifi_3_bar_", color, "_", dpi, "dp.png");
	sprintf(wifi_icons[5], "%s%s%s%s%d%s", iconpath, "twotone_signal_wifi_4_bar_", color, "_", dpi, "dp.png");

	sprintf(bt_icons[0], "%s%s%s%s%d%s", iconpath, "twotone_bluetooth_", color, "_", dpi, "dp.png");
	sprintf(bt_icons[1], "%s%s%s%s%d%s", iconpath, "twotone_bluetooth_disabled_", color, "_", dpi, "dp.png");

	sprintf(bar_icons[0], "%s%s%s%s%d%s", iconpath, "volume_bar_", color, "_", dpi, "dp.png");
	sprintf(bar_icons[1], "%s%s%s%s%d%s", iconpath, "brightness_bar_", color, "_", dpi, "dp.png");
	sprintf(bar_icons[2], "%s%s%s%s%d%s", iconpath, "dot_", color, "_", dpi, "dp.png");

	sprintf(critical_battery_icon, "%s%s", iconpath, "alert-outline-red.png");
}

void setupFuelGauge()
{
	//fuel_gauge_handle = i2cOpen(1, 0x36, 0);
	//i2cWriteWordData(fuel_gauge_handle, 0x18, 0x1770);
	//i2cWriteWordData(fuel_gauge_handle, 0x45, 0x0177);
	//i2cWriteWordData(fuel_gauge_handle, 0xDB, 0x8000);

	fuel_gauge_handle = wiringPiI2CSetup(0x36);
	wiringPiI2CWriteReg16(fuel_gauge_handle, 0x18, 0x1770);
	wiringPiI2CWriteReg16(fuel_gauge_handle, 0x45, 0x0177);
	wiringPiI2CWriteReg16(fuel_gauge_handle, 0xDB, 0x8000);
}

void setupBatteryCharger()
{
	//battery_charger_handle = i2cOpen(1, 0x6a, 0);
	//i2cWriteByteData(battery_charger_handle, 0x04, 0x18); // set charge current
	//i2cWriteByteData(battery_charger_handle, 0x03, 0x30); // set min sys voltage
	//i2cWriteByteData(battery_charger_handle, 0x02, 0x7D); // enable battery read
	//i2cWriteByteData(battery_charger_handle, 0x07, 0x8D); // disable watchdog timer

	battery_charger_handle = wiringPiI2CSetup(0x6a);
	wiringPiI2CWriteReg8(battery_charger_handle, 0x04, 0x18); // set charge current
	wiringPiI2CWriteReg8(battery_charger_handle, 0x03, 0x30); // set min sys voltage
	wiringPiI2CWriteReg8(battery_charger_handle, 0x02, 0x7D); // enable battery read
	wiringPiI2CWriteReg8(battery_charger_handle, 0x07, 0x8D); // disable watchdog timer
}

void setupAudio()
{
	snd_mixer_selem_id_t* sid;
	const char* card = "sysdefault";
	const char* selem_name = "PCM";

	snd_mixer_open(&handle, 0);
	snd_mixer_attach(handle, card);
	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, selem_name);

	elem = snd_mixer_find_selem(handle, sid);
}

void setupButtons()
{
	//wiringPiISR(7, INT_EDGE_BOTH, &gpioISR7);
	//wiringPiISR(8, INT_EDGE_BOTH, &gpioISR8);
	//wiringPiISR(9, INT_EDGE_BOTH, &gpioISR9);
	//wiringPiISR(10, INT_EDGE_BOTH, &gpioISR10);
	//gpioSetISRFunc(7, EITHER_EDGE, 0, &gpioISR);
	//gpioSetISRFunc(8, EITHER_EDGE, 0, &gpioISR);
	//gpioSetISRFunc(9, EITHER_EDGE, 0, &gpioISR);
	//gpioSetISRFunc(10, EITHER_EDGE, 0, &gpioISR);
}

//------------------------------------------------------------------------------

IMAGE_LAYER_T createImageLayer(char* fileName, int layer)
{
	IMAGE_LAYER_T image;

	// Load image from path
	if (loadPng(&(image.image), fileName) == false)
	{
		fprintf(stderr, "unable to load %s\n", fileName);
		exit(EXIT_FAILURE);
	}

	createResourceImageLayer(&image, layer);

	return image;
}

void addImage(IMAGE_LAYER_T* img, int x, int y)
{
	update = vc_dispmanx_update_start(displayNumber);
	assert(update != 0);

	addElementImageLayerOffset(img, x, y, display, update);

	int result = vc_dispmanx_update_submit_sync(update);
	assert(result == 0);
}

void removeImage(IMAGE_LAYER_T* img)
{
	if (img->element == 0)
		return;

	update = vc_dispmanx_update_start(displayNumber);
	assert(update != 0);

	int result = vc_dispmanx_element_remove(update, img->element);
	assert(result == 0);

	result = vc_dispmanx_update_submit_sync(update);
	assert(result == 0);
}

void moveImage(IMAGE_LAYER_T* img, int x, int y)
{
	update = vc_dispmanx_update_start(displayNumber);
	assert(update != 0);

	moveImageLayer(img, x, y, update);

	int result = vc_dispmanx_update_submit_sync(update);
	assert(result == 0);
}

//------------------------------------------------------------------------------

double getBatteryVoltage()
{
	//int batteryVolt = i2cReadByteData(battery_charger_handle, 0x0E);
	int batteryVolt = wiringPiI2CReadReg8(battery_charger_handle, 0x0E);
	double voltage = ((batteryVolt >> 6) & 1) * 1280;
	voltage += ((batteryVolt >> 5) & 1) * 640;
	voltage += ((batteryVolt >> 4) & 1) * 320;
	voltage += ((batteryVolt >> 3) & 1) * 160;
	voltage += ((batteryVolt >> 2) & 1) * 80;
	voltage += ((batteryVolt >> 1) & 1) * 40;
	voltage += (batteryVolt & 1) * 20 + 2304;
	voltage /= 1000;
	if(voltage <= 0)
		return 3.0;
	return voltage;
}

int getChargingStatus()
{
	//int status = i2cReadByteData(battery_charger_handle, 0x0B);
	int status = wiringPiI2CReadReg8(battery_charger_handle, 0x0B);
	int charging = ((status >> 3) & 3);
	if (charging == 0)
		return 0; // discharging;
	else
		return 1; // charging
}

int getBatteryIndex(int percent)
{
	int status = getChargingStatus();

	if(status == 0)
	{
		for(int i = 0; i < 9; i++)
		{
			if (percent <= percent_levels[i])
				return i;
		}
	}
	return 9;
}

int getBatteryPercent()
{
	//uint16_t percent = i2cReadWordData(fuel_gauge_handle, 0x06);
	uint16_t percent = wiringPiI2CReadReg16(fuel_gauge_handle, 0x06);
	return percent >> 8;
}

void displayBattery(uint32_t tick)
{
	int status = getChargingStatus();
	int percent = getBatteryPercent();
	int index = getBatteryIndex(percent);

	if (critical_shutdown == 0 && percent < pmin && status == 0)
	{
		addImage(&critical_battery_img,
			 (info.width - critical_battery_img.image.width) / 2,
			 (info.height - critical_battery_img.image.height) / 2);
		critical_shutdown = 1;
		critical_shutdown_tick = tick;
	}

	if (index != battery_index)
	{
		addImage(&battery_img[index], info.width - battery_img[index].image.width, 0);
		removeImage(&battery_img[battery_index]);
		battery_index = index;
	}
}

//-------------------------------------------------------------------------

int getVolumeLevel()
{
	long volume = 0;
	snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &volume);
	return volume;
}

void setVolumeLevel(int volume)
{
	snd_mixer_selem_set_playback_volume_all(elem, volume);
}

int getBrightnessLevel()
{
	char buf[4];
	FILE* fptr = fopen(backlight, "r");

	fgets(buf, 4, fptr);
	int brightness = atoi(buf);

	fclose(fptr);

	return brightness;
}

void setBrightnessLevel(int brightness)
{
	FILE* fptr = fopen(backlight, "w");

	fprintf(fptr, "%i", brightness);
	fclose(fptr);
}


void hideBar()
{
	if(display_volume == 1)
	{
		removeImage(&volume_bar_img);
		removeImage(&dot_img);
	}
	else if(display_brightness == 1)
	{
		removeImage(&brightness_bar_img);
		removeImage(&dot_img);
	}
	display_volume = 0;
	display_brightness = 0;
}

void displayVolumeBar(int volume)
{
	double dpi_d = dpi;
	int dotx = volume / 256.0 * (4.0 * dpi_d - dpi_d / 3.0) + dpi_d;

	if(display_volume == 1)
	{
		moveImage(&dot_img, dotx, 0);
	}
	else
	{
		if(display_brightness == 1)
		{
			removeImage(&brightness_bar_img);
			removeImage(&dot_img);
			display_brightness = 0;
		}
		addImage(&dot_img, dotx, 0);
		addImage(&volume_bar_img, 0, 0);
	}
	display_volume = 1;
	//display_tick = gpioTick();
	display_tick = micros();
}

void displayBrightnessBar(int brightness)
{
	double dpi_d = dpi;
	int dotx = brightness / 200.0 * (4.0 * dpi_d - dpi_d / 3.0) + dpi_d;

	if(display_brightness == 1)
	{
		moveImage(&dot_img, dotx, 0);
	}
	else
	{
		if(display_volume == 1)
		{
			removeImage(&volume_bar_img);
			removeImage(&dot_img);
			display_volume = 0;
		}
		addImage(&dot_img, dotx, 0);
		addImage(&brightness_bar_img, 0, 0);
	}
	display_brightness = 1;
	//display_tick = gpioTick();
	display_tick = micros();
}

//-------------------------------------------------------------------------

int getWifiLevel()
{
	FILE *fp = popen("iwconfig wlan0 | grep -i -w signal", "r");
	if (fp == NULL)
	{
		printf("failed to run command");
		exit(1);
	}

	char strength_str[6];
	char path[1000];
	int strength = 0;
	while (fgets(path, sizeof(path), fp) != NULL) {
		int index1 = rindex(path, '=') + 1 - path;
		int index2 = rindex(path, 'd') - path;
		memcpy(strength_str, &path[index1], index2 - index1);
	}
	pclose(fp);

	strength = atoi(strength_str);

	if (strength > -50)
		return 5;
	else if (strength > -65)
		return 4;
	else if (strength > -70)
		return 3;
	else if (strength > -80)
		return 2;
	else
		return 1;
}

void displayWifi()
{
	int index = -1;

	char buf[2];
	FILE* fptr = fopen(wifi_carrier, "r");
	fgets(buf, 2, fptr);
	fclose(fptr);

	int carrier_state = atoi(buf);
	if (carrier_state == 1)
	{
		index = getWifiLevel();
	}
	else
	{
		fptr = fopen(wifi_linkmode, "r");
		fgets(buf, 2, fptr);
		fclose(fptr);

		int linkmode = atoi(buf);
		if (linkmode == 1)
			index = 0;
	}

	if (index != wifi_index)
	{
		if (wifi_index != -1)
			removeImage(&wifi_img[wifi_index]);
		if (index != -1)
			addImage(&wifi_img[index], info.width - wifi_img[index].image.width - dpi, 0);

		wifi_index = index;
	}
}

//-------------------------------------------------------------------------

void displayBluetooth()
{

}

//-------------------------------------------------------------------------

int getCPUTemp()
{
	FILE *fp = popen(temp_cmd, "r");
	if (fp == NULL)
	{
		printf("failed to run command");
		exit(1);
	}

	char temp_str[6];
	char path[1000];
	while (fgets(path, sizeof(path), fp) != NULL) 
	{
		int index1 = rindex(path, '=') + 1 - path;
		int index2 = rindex(path, '\'') - path;
		memcpy(temp_str, &path[index1], index2 - index1);
	}
	pclose(fp);

	return atoi(temp_str);
}

void displayEnvironment()
{
	FILE *fp = popen(env_cmd, "r");
	if (fp == NULL)
	{
		printf("failed to run command");
		exit(1);
	}

	char env_str[6];
	char path[1000];
	while (fgets(path, sizeof(path), fp) != NULL) 
	{
		int index1 = rindex(path, 'x') + 1 - path;
		memcpy(env_str, &path[index1], 2);
	}
	pclose(fp);

	int env = atoi(env_str);
	int under_voltage_temp = env & 0x01;
	int freq_capped_temp = env & 0x02;
	int throttled_temp = env & 0x04;

	if (under_voltage != under_voltage_temp)
	{
		under_voltage = under_voltage_temp;
		if (under_voltage > 0)
			addImage(&under_voltage_img, info.width - under_voltage_img.image.width - dpi * 3, 0);
		else
			removeImage(&under_voltage_img);
	}

	if (freq_capped != freq_capped_temp)
	{
		freq_capped = freq_capped_temp;
		if (freq_capped > 0)
			addImage(&freq_capped_img, info.width - freq_capped_img.image.width - dpi * 4, 0);
		else
			removeImage(&freq_capped_img);
	}

	if (throttled != throttled_temp)
	{
		throttled = throttled_temp;
		if (throttled > 0)
			addImage(&throttled_img, info.width - throttled_img.image.width - dpi * 5, 0);
		else
			removeImage(&throttled_img);
	}
}

//-------------------------------------------------------------------------

void increase()
{
	if (misc_pressed == 1)
	{
		misc_alt = 1;
		int brightness = getBrightnessLevel();
		brightness = brightness + 10;
		if (brightness > 200)
			brightness = 200;
		setBrightnessLevel(brightness);
		displayBrightnessBar(brightness);
	}
	else
	{
		int volume = getVolumeLevel();
		volume = volume + 5;
		if(volume > 255)
			volume = 255;
		setVolumeLevel(volume);
		displayVolumeBar(volume);
	}
}

void decrease()
{
	if (misc_pressed == 1)
	{
		misc_alt = 1;
		int brightness = getBrightnessLevel();
		brightness = brightness - 10;
		if (brightness < 0)
			brightness = 0;
		setBrightnessLevel(brightness);
		displayBrightnessBar(brightness);
	}
	else
	{
		int volume = getVolumeLevel();
		volume = volume - 5;
		if(volume < 0)
			volume = 0;
		setVolumeLevel(volume);
		displayVolumeBar(volume);
	}
}

void restart_cmd()
{
	FILE *fp = popen("sudo /usr/local/bin/multi_switch.sh --es-pid", "r");
	if (fp == NULL)
	{
		printf("failed to run command");
		exit(1);
	}

	int pidRunning = 1;
	char path[1000];
	while (fgets(path, sizeof(path), fp) != NULL) {
		pidRunning = atoi(path);
	}
	pclose(fp);

	if(pidRunning == 0)
		system("sudo reboot");
	else
		system("sudo /usr/local/bin/multi_switch.sh --es-reboot");
}

void shutdown_cmd()
{
	//int status = i2cReadByteData(battery_charger_handle, 0x0B);
	int status = wiringPiI2CReadReg8(battery_charger_handle, 0x0B);
	status = status >> 5;
	if (status == 0)
		wiringPiI2CWriteReg8(battery_charger_handle, 0x09, 0x6c); // shut down power to system
		//i2cWriteByteData(battery_charger_handle, 0x09, 0x6c); // shut down power to system

	FILE *fp = popen("sudo /usr/local/bin/multi_switch.sh --es-pid", "r");
	if (fp == NULL)
	{
		printf("failed to run command");
		exit(1);
	}

	int pidRunning = 1;
	char path[1000];
	while (fgets(path, sizeof(path), fp) != NULL) {
		pidRunning = atoi(path);
	}
	pclose(fp);

	if(pidRunning == 0)
		system("sudo shutdown -h now");
	else
		system("sudo /usr/local/bin/multi_switch.sh --es-poweroff");
}

void gpioISR7()
{
	uint32_t tick = micros();
	uint32_t tick_diff = 0;
	if (power_pressed == 0)
	{
		power_pressed = 1;
		power_tick = tick;
	}
	else
	{
		power_pressed = 0;
		tick_diff = tick - power_tick;
		if(tick_diff < powerButtonTime)
		{
			run = false;
			restart = true;
		}
	}
}

void gpioISR8()
{
	uint32_t tick = micros();
	if(misc_pressed == 0)
	{
		misc_pressed = 1;
		misc_tick = tick;
	}
	else
	{
		misc_pressed = 0;
		misc_alt = 0;
		misc_tick = 0;
	}
}

void gpioISR9()
{
	uint32_t tick = micros();
	if(increase_pressed == 0)
	{
		increase_pressed = 1;
		increase_tick = tick;
		increase();
	}
	else
	{
		increase_pressed = 0;
		increase_tick = 0;
	}
}

void gpioISR10()
{
	uint32_t tick = micros();
	if (decrease_pressed == 0)
	{
		decrease_pressed = 1;
		decrease_tick = tick;
		decrease();
	}
	else
	{
		decrease_pressed = 0;
		decrease_tick = 0;
	}
}


void updateGPIO()
{
	uint32_t tick = micros();
	uint32_t tick_diff = 0;
	int gpio7State = digitalRead(7);
	int gpio8State = digitalRead(8);
	int gpio9State = digitalRead(9);
	int gpio10State = digitalRead(10);

	if (gpio7State == 0 && power_pressed == 0)
	{
		power_pressed = 1;
		power_tick = tick;
	}
	else if (gpio7State == 1 && power_pressed == 1)
	{
		power_pressed = 0;
		tick_diff = tick - power_tick;
		if(tick_diff < powerButtonTime)
		{
			run = false;
			restart = true;
		}
	}

	if (gpio8State == 0 && misc_pressed == 0)
	{
		misc_pressed = 1;
		misc_tick = tick;
	}
	else if (gpio8State == 1 && misc_pressed == 1)
	{
		misc_pressed = 0;
		misc_alt = 0;
		misc_tick = 0;
	}

	if (gpio10State == 0 && decrease_pressed == 0)
	{
		decrease_pressed = 1;
		decrease_tick = tick;
		decrease();
	}
	else if (gpio10State == 1 && decrease_pressed == 1)
	{
		decrease_pressed = 0;
		decrease_tick = 0;
	}

	if (gpio9State == 0 && increase_pressed == 0)
	{
		increase_pressed = 1;
		increase_tick = tick;
		increase();
	}
	else if (gpio9State == 1 && increase_pressed == 1)
	{
		increase_pressed = 0;
		increase_tick = 0;
	}
}

void loadInitialImages()
{
	critical_battery_img = createImageLayer(critical_battery_icon, 15000);

	for (int i = 0; i < 10; i++)
		battery_img[i] = createImageLayer(battery_icons[i], 15000);
	for (int i = 0; i < 6; i++)
		wifi_img[i] = createImageLayer(wifi_icons[i], 15000);
	for (int i = 0; i < 2; i++)
		bluetooth_img[i] = createImageLayer(bt_icons[i], 15000);

	volume_bar_img = createImageLayer(bar_icons[0], 15000);
	brightness_bar_img = createImageLayer(bar_icons[1], 15000);
	dot_img = createImageLayer(bar_icons[2], 15000);

	under_voltage_img = createImageLayer(env_icons[0], 15000);
	freq_capped_img = createImageLayer(env_icons[1], 15000);
	throttled_img = createImageLayer(env_icons[2], 15000);
}

//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
	//gpioInitialise();

	int result = 0;
	int cpuTemp = 0;
	int sleepMilliseconds = 100;
	uint32_t tick = 0;
	uint32_t previousTick = 0;
	uint32_t tickDiff = 0;
	int fanTick = 0;
	int fanState = 0;

	wiringPiSetupGpio();

	setupFiles();
	setupFuelGauge();
	setupBatteryCharger();
	setupDisplay();
	setupAudio();
	setupButtons();

	loadInitialImages();

	previousTick = micros();
	tick = previousTick;

	while(run)
	{
		cpuTemp = getCPUTemp();
		tick = micros();
		tickDiff = tick - previousTick;
		if (tickDiff > 1000000)
		{
			previousTick = tick;
			displayBattery(tick);
			displayWifi();
			displayEnvironment();

			if (fanTick > 0)
				fanTick = fanTick - tickDiff;
		}

		// check volume bar and hide if out for more then 3 seconds
		if ((display_volume == 1 || display_brightness == 1) && tick - display_tick >= volbriBarTime)
			hideBar();

		// check to see if the misc button has been held down but not being used for alternate increase and decrease buttons
		if (misc_pressed == 1 && misc_alt == 0 && tick - misc_tick >= miscButtonTime)
		{
			system("sudo ifconfig wlan0 down");
			system("sudo ifconfig wlan0 up");
			misc_pressed = 0;
			misc_tick = 0;
		}

		// check if the power button is held down for the power off
		if (power_pressed == 1 && tick - power_tick >= powerButtonTime)
		{
			power_pressed = 0;
			shutdown = true;
			break;
		}

		// check to see if the increase is held down
		if (increase_pressed == 1 && tick - increase_tick >= incdecButtonTime)
		{
			display_tick = tick;
			increase();
		}

		// check to see if the decrease is held down
		if (decrease_pressed == 1 && tick - decrease_tick >= incdecButtonTime)
		{
			display_tick = tick;
			decrease();
		}

		// check to see if the critical shutdown is going
		if (critical_shutdown == 1 && tick - critical_shutdown_tick >= criticalShutdownTime)
		{
			critical_shutdown = 0;
			shutdown = true;
			break;
		}

		// turn on fan if the cpuTemp is above the fan limit
		if (cpuTemp > fan_on_temp)
			fanTick = fanMinTime;

		if (fanState == 0 && fanTick > 0)
		{
			digitalWrite(12, 1);
			fanState = 1;
		}
		else if (fanState == 1 && fanTick <= 0)
		{
			digitalWrite(12, 0);
			fanState = 0;
		}

		updateGPIO();

		usleep(sleepMilliseconds * 1000);
	}


	destroyImageLayer(&critical_battery_img);
	for (int i = 0; i < 10; i++)
		destroyImageLayer(&battery_img[i]);
	destroyImageLayer(&volume_bar_img);
	destroyImageLayer(&brightness_bar_img);
	destroyImageLayer(&dot_img);

	result = vc_dispmanx_display_close(display);
	assert(result == 0);


 	snd_mixer_close(handle);

	if (shutdown == true)
		shutdown_cmd();
	else if (restart == true)
		restart_cmd();

	//gpioTerminate();
	return 0;
}
