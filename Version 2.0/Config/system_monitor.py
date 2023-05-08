#!/usr/bin/env python
from gpiozero import Button, DigitalOutputDevice
from signal import pause
from datetime import datetime
from statistics import median
from collections import deque
from enum import Enum
from time import sleep
import subprocess
import smbus
import subprocess
import os
import re
import logging
import logging.handlers
import smbus
import array
import alsaaudio
#import signal

bus = smbus.SMBus(1)
address = 0x6a
gauge_address = 0x36
misc_held = 0
shutdown_held = 0
dpi = 24
color = "white"
pmin = 2
vmin = 3.3
vindex = 0
wifi_index = 2
bt_index = 2
fan_count = 0
bar_count = 0
shutdown_count = 0
fan_on_temp = 60
voltage_init = 0
display_volume = 0
display_brightness = 0

pngview_path="/usr/local/bin/pngview"
pngview_call=[pngview_path, "-d", "0", "-b", "0x0000", "-n", "-l", "15000", "-y", "0", "-x"]

iconpath = os.path.dirname(os.path.realpath(__file__)) + "/overlay_icons/"
logfile = "/home/pi/overlay.log"

env_icons = {
	"under-voltage": iconpath + "flash_" + str(dpi) + "dp.png",
	"freq-capped":   iconpath + "thermometer_" + str(dpi) + "dp.png",
	"throttled":     iconpath + "thermometer-lines_" + str(dpi) + "dp.png"
}
wifi_icons = {
	"level4"       : iconpath + "twotone_signal_wifi_4_bar_" + color + "_" + str(dpi) + "dp.png",
	"level3"       : iconpath + "twotone_signal_wifi_3_bar_" + color + "_" + str(dpi) + "dp.png",
	"level2"       : iconpath + "twotone_signal_wifi_2_bar_" + color + "_" + str(dpi) + "dp.png",
	"level1"       : iconpath + "twotone_signal_wifi_1_bar_" + color + "_" + str(dpi) + "dp.png",
	"level0"       : iconpath + "twotone_signal_wifi_0_bar_" + color + "_" + str(dpi) + "dp.png",
	"disconnected" : iconpath + "twotone_signal_wifi_off_" + color + "_" + str(dpi) + "dp.png"
}
bt_icons = {
	"connected"    : iconpath + "twotone_bluetooth_" + color + "_" + str(dpi) + "dp.png",
	"disconnected" : iconpath + "twotone_bluetooth_disabled_" + color + "_" + str(dpi) + "dp.png"
}
bar_icons = {
	"vol_bar"      : iconpath + "volume_bar_" + color + "_" + str(dpi) + "dp.png",
	"bri_bar"      : iconpath + "brightness_bar_" + color + "_" + str(dpi) + "dp.png",
	"dot"          : iconpath + "dot_" + color + "_" + str(dpi) + "dp.png"
}
icon_battery_critical_shutdown = iconpath + "alert-outline-red.png"

wifi_carrier = "/sys/class/net/wlan0/carrier" # 1 when wifi connected, 0 when disconnected and/or ifdown
wifi_linkmode = "/sys/class/net/wlan0/link_mode" # 1 when ifup, 0 when ifdown
bt_devices_dir="/sys/class/bluetooth"
env_cmd="vcgencmd get_throttled"

fbfile="tvservice -s"

levels = [5, 10, 20, 30, 50, 60, 80, 90, 100]
#levels = [3.3, 3.4, 3.47, 3.5, 3.52, 3.55, 3.6, 3.68, 3.75, 3.95, 4.0]
#icons = { "discharging": [ "alert_red", "alert", "20", "30", "50", "50", "60", "60", "80", "90", "full" ],
icons = { "discharging": [ "alert_red", "alert", "20", "30", "50", "60", "80", "90", "full" ],
          "charging"   : [ "charging" ]}
voltages = array.array('d', [0, 0, 0, 0, 0])

overlay_processes = {}
bar_processes = {}
wifi_state = None
bt_state = None
battery_level = None
env = None

class InterfaceState(Enum):
	DISABLED = 0
	ENABLED = 1
	CONNECTED = 2

# ******************* BATTERY ***********************
def get_battery_voltage():
	battery = bus.read_byte_data(address, 0x0E)
	voltage = ((battery >> 6) & 1) * 1280
	voltage += ((battery >> 5) & 1) * 640
	voltage += ((battery >> 4) & 1) * 320
	voltage += ((battery >> 3) & 1) * 160
	voltage += ((battery >> 2) & 1) * 80
	voltage += ((battery >> 1) & 1) * 40
	voltage += (battery & 1) * 20 + 2304
	voltage /= 1000
	if voltage <= 0:
		return 3.0
	return voltage

def get_charging_status():
	status = bus.read_byte_data(address, 0x0B)
	charging = ((status >> 3) & 3)
	if charging == 0:
		return "discharging"
	else:
		return "charging"

def get_battery_level(percent):
	# Figure out how 'wide' each range is
	status = get_charging_status()

	if status == "discharging":
		length = len(levels)
		index = length - 1
		for i in range(length):
			if percent <= levels[i]:
				index = i
				break

		iconSpan = len(icons[status]) - 1
		if index > iconSpan:
			index = iconSpan
		elif index < 0:
			index = 0

		# Convert the 0-1 range into a value in the right range.
		return icons[status][index]
	else:
		return icons[status][0]

def battery():
	global battery_level, overlay_processes, wifi_index, bt_index, vindex, voltages, shutdown_count, voltage_init
	status = get_charging_status()

	#if voltage_init == 0:
	#	voltages[0] = get_battery_voltage()
	#	voltages[1] = voltages[0]
	#	voltages[2] = voltages[0]
	#	voltages[3] = voltages[0]
	#	voltages[4] = voltages[0]
	#	voltage_init = 1
	#else:
	#	voltages[vindex] = get_battery_voltage()
	#voltage_avg = (voltages[0] + voltages[1] + voltages[2] + voltages[3] + voltages[4]) / 5.0
	percent = get_battery_percent()
	level = get_battery_level(percent)
	#level = get_battery_level(voltage_avg)

	#vindex += 1
	#if vindex >= 5:
	#	vindex = 0

	if percent <= pmin and status == "discharging":
	#if voltage_avg <= vmin:
		message='Battery voltage at or below ' + str(pmin) + '%. Initiating shutdown within 1 minute'
		my_logger.warn(message)

		subprocess.Popen(pngview_call + [str(int(resolution[0]) / 2 - 64), "-y", str(int(resolution[1]) / 2 - 64), icon_battery_critical_shutdown])
		shutdown_count = 1

	if level != battery_level:
		if "bat" in overlay_processes:
			overlay_processes["bat"].kill()
			del overlay_processes["bat"]

		icon='twotone_battery_' + level + "_" + color + "_" + str(dpi) + "dp.png"
		overlay_processes["bat"] = subprocess.Popen(pngview_call + [ str(int(resolution[0]) - dpi), iconpath + icon])
	return level


# ******************* BAR ************************
def clear_vol_bri():
	global display_volume, display_brightness
	for k in bar_processes.keys():
		bar_processes[k].kill()
	bar_processes.clear()
	#bar_processes = {}
	display_brightness = 0
	display_volume = 0


# ****************** VOLUME **********************
def get_volume_level():
	m = alsaaudio.Mixer(control='PCM')
	vol = m.getvolume()
	return float(vol[0])

def display_volume_bar():
	global bar_processes, bar_count, display_volume
	volume = get_volume_level()
	volume_percent = volume / 100.0 * (4.0 * dpi - dpi / 3.0)

	if display_volume == 1:
		bar_processes["dot"].kill()
		del(bar_processes["dot"])
		bar_processes["dot"] = subprocess.Popen(pngview_call + [str(dpi + volume_percent), bar_icons["dot"]])
	else:
		clear_vol_bri()
		bar_processes["dot"] = subprocess.Popen(pngview_call + [str(dpi + volume_percent), bar_icons["dot"]])
		bar_processes["vol_bar"] = subprocess.Popen(pngview_call + ["0", bar_icons["vol_bar"]])
	bar_count = 1
	display_volume = 1

def display_brightness_bar(brightness):
	global bar_processes, bar_count, display_brightness
	brightness_percent = brightness / 200.0 * (4.0 * dpi - dpi / 3.0)

	if display_brightness == 1:
		bar_processes["dot"].kill()
		del(bar_processes["dot"])
		bar_processes["dot"] = subprocess.Popen(pngview_call + [str(dpi + brightness_percent), bar_icons["dot"]])
	else:
		clear_vol_bri()
		bar_processes["dot"] = subprocess.Popen(pngview_call + [str(dpi + brightness_percent), bar_icons["dot"]])
		bar_processes["bri_bar"] = subprocess.Popen(pngview_call + ["0", bar_icons["bri_bar"]])
	bar_count = 1
	display_brightness = 1

# ******************* WIFI ***********************
def get_wifi_strength():
	result = subprocess.run("iwconfig wlan0 | grep -i -w signal", shell=True, stdout=subprocess.PIPE)
	output = result.stdout.decode('utf-8')
	return float(output[output.rindex('=') + 1:output.rindex("d")])

def get_wifi_level(strength):
	if strength > -50:
		return 4
	elif strength > -65:
		return 3
	elif strength > -70:
		return 2
	elif strength > -80:
		return 1
	else:
		return 0

def wifi():
	global wifi_state, wifi_level, wifi_index, bt_index

	new_wifi_state = InterfaceState.DISABLED
	new_wifi_level = 0
	try:
		f = open(wifi_carrier, "r")
		carrier_state = int(f.read().rstrip())
		f.close()
		if carrier_state == 1:
			# ifup and connected to AP
			new_wifi_state = InterfaceState.CONNECTED
			new_wifi_level = get_wifi_level(get_wifi_strength())
		elif carrier_state == 0:
			f = open(wifi_linkmode, "r")
			linkmode_state = int(f.read().rstrip())
			f.close()
			if linkmode_state == 1:
				# ifup but not connected to any network
 				new_wifi_state = InterfaceState.ENABLED
				# else - must be ifdown

	except IOError:
		pass

	if new_wifi_state != wifi_state or new_wifi_level != wifi_level:
		if "wifi" in overlay_processes:
			overlay_processes["wifi"].kill()
			del overlay_processes["wifi"]

		if new_wifi_state == InterfaceState.ENABLED:
			bt_index = wifi_index + 1
			overlay_processes["wifi"] = subprocess.Popen(pngview_call + [str(int(resolution[0]) - dpi * wifi_index), wifi_icons["disconnected"]])
		elif new_wifi_state == InterfaceState.CONNECTED:
			bt_index = wifi_index + 1
			if new_wifi_level == 0:
				overlay_processes["wifi"] = subprocess.Popen(pngview_call + [str(int(resolution[0]) - dpi * wifi_index), wifi_icons["level0"]])
			elif new_wifi_level == 1:
				overlay_processes["wifi"] = subprocess.Popen(pngview_call + [str(int(resolution[0]) - dpi * wifi_index), wifi_icons["level1"]])
			elif new_wifi_level == 2:
				overlay_processes["wifi"] = subprocess.Popen(pngview_call + [str(int(resolution[0]) - dpi * wifi_index), wifi_icons["level2"]])
			elif new_wifi_level == 3:
				overlay_processes["wifi"] = subprocess.Popen(pngview_call + [str(int(resolution[0]) - dpi * wifi_index), wifi_icons["level3"]])
			elif new_wifi_level == 4:
				overlay_processes["wifi"] = subprocess.Popen(pngview_call + [str(int(resolution[0]) - dpi * wifi_index), wifi_icons["level4"]])
		else:
			bt_index = wifi_index

	return (new_wifi_state, new_wifi_level)


# ******************* BLUETOOTH ***********************
def bluetooth():
	global bt_state, bt_index

	new_bt_state = InterfaceState.DISABLED
	try:
		p1 = subprocess.Popen('hciconfig', stdout = subprocess.PIPE)
		p2 = subprocess.Popen(['awk', 'FNR == 3 {print tolower($1)}'], stdin = p1.stdout, stdout=subprocess.PIPE)
		state=p2.communicate()[0].decode().rstrip()
		if state == "up":
			new_bt_state = InterfaceState.ENABLED
	except IOError:
		pass

	try:
		devices=os.listdir(bt_devices_dir)
		if len(devices) > 1:
			new_bt_state = InterfaceState.CONNECTED
	except OSError:
		pass

	if new_bt_state != bt_state:
		if "bt" in overlay_processes:
			overlay_processes["bt"].kill()
			del overlay_processes["bt"]

		if new_bt_state == InterfaceState.CONNECTED:
			overlay_processes["bt"] = subprocess.Popen(pngview_call + [str(int(resolution[0]) - dpi * bt_index), bt_icons["connected"]])
		elif new_bt_state == InterfaceState.ENABLED:
			overlay_processes["bt"] = subprocess.Popen(pngview_call + [str(int(resolution[0]) - dpi * bt_index), bt_icons["disconnected"]])
	return new_bt_state


# **************** ENVIRONMENT ********************
def environment():
	val=int(re.search("throttled=(0x\d+)", subprocess.check_output(env_cmd.split()).decode().rstrip()).groups()[0], 16)
	env = {
		"under-voltage": bool(val & 0x01),
		"freq-capped": bool(val & 0x02),
		"throttled": bool(val & 0x04)
	}
	for k,v in env.items():
		if v and not k in overlay_processes:
			overlay_processes[k] = subprocess.Popen(pngview_call + [str(int(resolution[0]) - dpi * (len(overlay_processes)+1)), env_icons[k]])
		elif not v and k in overlay_processes:
			overlay_processes[k].kill()
			del(overlay_processes[k])
	#return env # too much data
	return val


# ******************* CPU ***********************
def get_cpu_temp():
	result = subprocess.run("vcgencmd measure_temp", shell=True, stdout=subprocess.PIPE)
	output = result.stdout.decode('utf-8')
	return float(output[output.index('=') + 1:output.rindex("'")])

# ******************* BRIGHTNESS ***********************
def get_brightness():
	file = open("/sys/class/backlight/rpi_backlight/brightness","r")
	bright_str = file.read()
	file.close()
	return int(bright_str)

def set_brightness(brightness):
	file = open("/sys/class/backlight/rpi_backlight/brightness","w")
	file.write(str(brightness))
	file.close()


# ******************* BUTTONS ***********************
def increase():
	global misc_held
	if button_misc.is_pressed:
		brightness = get_brightness()
		brightness += 10
		if brightness > 200:
			brightness = 200
		set_brightness(brightness)
		misc_held = 1
		display_brightness_bar(brightness)
	else:
		subprocess.run("amixer -q -M set PCM 2.5%+", shell=True)
		display_volume_bar()

def increase_held():
	while button_up.is_pressed:
		increase()
		sleep(0.2)

def decrease():
	global misc_held
	if button_misc.is_pressed:
		brightness = get_brightness()
		brightness -= 10
		if brightness < 0:
			brightness = 0
		set_brightness(brightness)
		misc_held = 1
		display_brightness_bar(brightness)
	else:
		subprocess.run("amixer -q -M set PCM 2.5%-", shell=True)
		display_volume_bar()

def decrease_held():
	while button_down.is_pressed:
		decrease()
		sleep(0.2)

def misc_released():
	global misc_held
	if misc_held == 0:
		subprocess.run("sudo ifconfig wlan0 down", shell=True)
		subprocess.run("sudo ifconfig wlan0 up", shell=True)
	misc_held = 0


# ******************* SHUTDOWN ***********************
def restart_pi():
	global shutdown_held
	if shutdown_held == 0:
		result = subprocess.run("sudo /usr/local/bin/multi_switch.sh --es-pid", shell=True, stdout=subprocess.PIPE)
		output = result.stdout.decode('utf-8')
		if float(output) == 0:
			subprocess.run("sudo reboot", shell=True)
		else:
			subprocess.run("sudo /usr/local/bin/multi_switch.sh --es-reboot", shell=True)
	shutdown_held = 0

def shutdown_pi():
	global shutdown_held
	shutdown_held = 1
	vbus_status = bus.read_byte_data(address, 0x0B)
	vbus_status = vbus_status >> 5
	if vbus_status == 0:
		bus.write_byte_data(address, 0x09, 0x6c)

	result = subprocess.run("sudo /usr/local/bin/multi_switch.sh --es-pid", shell=True, stdout=subprocess.PIPE)
	output = result.stdout.decode('utf-8')
	if float(output) == 0:
		subprocess.run("sudo shutdown -h now", shell=True)
	else:
		subprocess.run("sudo /usr/local/bin/multi_switch.sh --es-poweroff", shell=True)

# ************** CHARGING CIRCUIT ********************
def set_battery_charge_current():
	bus.write_byte_data(address, 0x04, 0x18)

def set_battery_min_sys_volt():
	bus.write_byte_data(address, 0x03, 0x30)

def enable_battery_read():
	bus.write_byte_data(address, 0x02, 0x7D)

def disable_watchdog_timer():
	bus.write_byte_data(address, 0x07, 0x8D)

# ************** FUEL GAUGE ********************
def set_battery_capacity():
	bus.write_i2c_block_data(gauge_address, 0x18, [0x70, 0x17])
	bus.write_i2c_block_data(gauge_address, 0x45, [0x77, 0x01])
	bus.write_i2c_block_data(gauge_address, 0xDB, [0x00, 0x80])
	sleep(5)

def get_battery_percent():
	msb, lsb = bus.read_i2c_block_data(gauge_address, 0x06, 2)
	return lsb


# ************** SETUP GPIO ********************
button_down = Button(10)
button_down.when_pressed = decrease
button_down.when_held = decrease_held
button_up = Button(9)
button_up.when_pressed = increase
button_up.when_held = increase_held
button_misc = Button(8)
button_misc.when_released = misc_released
button_shutdown = Button(7)
button_shutdown.when_released = restart_pi
button_shutdown.when_held = shutdown_pi
fan = DigitalOutputDevice(12)


# ************** SETUP ********************
# Set the battery charging circuit settings
disable_watchdog_timer()
enable_battery_read()
set_battery_charge_current()
set_battery_min_sys_volt()
set_battery_capacity()

# Setup the logger
my_logger = logging.getLogger('MyLogger')
my_logger.setLevel(logging.INFO)
handler = logging.handlers.RotatingFileHandler(logfile, maxBytes=102400, backupCount=1)
my_logger.addHandler(handler)
#console = logging.StreamHandler()
#my_logger.addHandler(console)

# Get Framebuffer resolution
resolution=re.search("(\d{3,}x\d{3,})", subprocess.check_output(fbfile.split()).decode().rstrip()).group().split('x')
my_logger.info(resolution)

# ************** MAIN LOOP ********************
while True:
	# Get the system information
	level = battery()
	battery_level = level
	(wifi_state, wifi_level) = wifi()
	bt_state = bluetooth()
	env = environment()
	cpu_temp = get_cpu_temp()

	# Log information
	#my_logger.info("%s, voltage: %.2f, icon: %s, wifi: %s, bt: %s, throttle: %#0x" % (
	#	datetime.now(),
	#	voltage,
	#	level,
	#	wifi_state.name,
	#	bt_state.name,
	#	env
	#))

	# Turn on Fan if the temp is too high
	if cpu_temp > fan_on_temp:
		fan_count = 0
		fan.on()
	elif fan.value == 1 and fan_count == 0:
		fan_count = 1

	# If the fan has been on for 1 min after the temp
	# drops down below fan_on_temp then turn it off
	if fan_count > 60:
		fan_count = 0
		fan.off()
	elif fan_count > 0:
		fan_count = fan_count + 1

	# Check if the shutdown timer has started
	if shutdown_count > 0:
		shutdown_count = shutdown_count + 1
	if shutdown_count > 60:
		shutdown_pi()

	# Check if the bar timer has started
	if bar_count > 0:
		bar_count = bar_count + 1
	if bar_count > 3:
		bar_count = 0
		clear_vol_bri()

	sleep(1)
