#!/bin/bash
### BEGIN INIT INFO
# Provides:          startup.sh
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Start daemon at boot time
# Description:       Enable service provided by daemon.
### END INIT INFO

# script control variables

REBOOT_REQUIRED=false
CONFIG=/boot/config.txt
LOADMOD=/etc/modules
HDMI_GROUP=0
HDMI_MODE=1

#. /lib/lsb/init-functions

startMonitor() {
	/usr/local/bin/monitor &
	#python3 /usr/local/bin/system_monitor.py &
}

stopMonitor() {
	kill $(ps aux | grep "/usr/local/bin/monitor" | awk '{print $2}')
	#kill $(ps aux | grep "python3 /usr/local/bin/system_monitor.py" | awk '{print $2}')
}

screenCheck() {
	# detect which video output is plugged in and setup the config.txt file accordingly
	DISPLAYS=$(tvservice -l)
	if [[ "${DISPLAYS}" == *"HDMI"* ]]
	then
		# Use ignore_lcd in the /boot/config.txt as the indicator that HDMI is setup
		if [ -e $CONFIG ] && grep -q -E "^#ignore_lcd=1$" $CONFIG
		then
			# Setup HDMI
			sudo sed -i "s|^dtoverlay=hifiberry-dac$|#dtoverlay=hifiberry-dac|" $CONFIG
			sudo sed -i "s|^dtoverlay=i2s-mmap$|#dtoverlay=i2s-mmap|" $CONFIG
			sudo sed -i "s|^#dtparam=audio=on$|dtparam=audio=on|" $CONFIG
			sudo sed -i "s|^#ignore_lcd=1$|ignore_lcd=1|" $CONFIG

			sudo cp ~/asound.conf.hdmi /etc/asound.conf
			if [ -e $LOADMOD ] && grep -q "^#snd-bcm2835" $LOADMOD; then
				sudo sed -i "s|^#snd-bcm2835|snd-bcm2835|" $LOADMOD
			fi
						
			REBOOT_REQUIRED=true
		fi
	else
		if [ -e $CONFIG ] && grep -q -E "^ignore_lcd=1$" $CONFIG
		then
			# Setup LCD
			sudo sed -i "s|^#dtoverlay=hifiberry-dac$|dtoverlay=hifiberry-dac|" $CONFIG
			sudo sed -i "s|^#dtoverlay=i2s-mmap$|dtoverlay=i2s-mmap|" $CONFIG
			sudo sed -i "s|^dtparam=audio=on$|#dtparam=audio=on|" $CONFIG
			sudo sed -i "s|^ignore_lcd=1$|#ignore_lcd=1|" $CONFIG

			sudo cp ~/asound.conf.lcd /etc/asound.conf
			if [ -e $LOADMOD ] && grep -q "^snd-bcm2835" $LOADMOD; then
				sudo sed -i "s|^snd-bcm2835|#snd-bcm2835|" $LOADMOD
			fi
			REBOOT_REQUIRED=true
		fi
	fi
}

start() {
	screenCheck

	if $REBOOT_REQUIRED 
	then
		reboot
	fi
	startMonitor
}

stop() {
	stopMonitor
}

case "$1" in
	start)
		start
		;;
	stop)
		stop
		;;
esac

exit 0
