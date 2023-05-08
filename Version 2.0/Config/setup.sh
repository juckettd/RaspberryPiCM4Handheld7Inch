#!/bin/bash

: <<'DISCLAIMER'

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

This script is licensed under the terms of the MIT license.
Unless otherwise noted, code reproduced herein
was written for this script.

DISCLAIMER

# control variables

productname="Aegis" # the name of the product to install
scriptname="setup.sh" # the name of this script
debugmode="no" # whether the script should use debug routines
forcesudo="no" # whether the script requires to be ran with root privileges

BOOTCMD=/boot/cmdline.txt
CONFIG=/boot/config.txt
RCLOCAL=/etc/rc.local
BLACKLIST=/etc/modprobe.d/raspi-blacklist.conf
LOADMOD=/etc/modules
PROFILE=/etc/profile
BRIGHTNESS=/sys/class/backlight/rpi_backlight/brightness
DTBODIR=/boot/overlays
REBOOT_REQUIRED=false
AUDIO_TEST_REQUIRED=false

# functions

prompt() {
        read -r -p "$1 [y/N] " response < /dev/tty
        if [[ $response =~ ^(yes|y|Y)$ ]]; then
            true
        else
            false
        fi
}

success() {
    echo -e "$(tput setaf 2)$1$(tput sgr0)"
}

inform() {
    echo -e "$(tput setaf 6)$1$(tput sgr0)"
}

warning() {
    echo -e "$(tput setaf 1)$1$(tput sgr0)"
}

newline() {
    echo ""
}

progress() {
    count=0
    until [ $count -eq $1 ]; do
        echo -n "..." && sleep 1
        ((count++))
    done
    echo
}

sudocheck() {
    if [ $(id -u) -ne 0 ]; then
        echo -e "Install must be run as root. Try 'sudo ./$scriptname'\n"
        exit 1
    fi
}

sysreboot() {
    warning "Some changes made to your system require"
    warning "your computer to reboot to take effect."
    newline
    if prompt "Would you like to reboot now?"; then
        sync && sudo reboot
    fi
}

os_check() {
    IS_SUPPORTED=false
	IS_RASPBIAN=false

    if [ -f /etc/os-release ]; then
        if cat /etc/os-release | grep "Raspbian" > /dev/null; then
            IS_RASPBIAN=true
        fi
    fi

	if $IS_RASPBIAN; then
		if [ -f /etc/os-release ]; then
			if cat /etc/os-release | grep "/sid" > /dev/null; then
				IS_SUPPORTED=false
			elif cat /etc/os-release | grep "buster" > /dev/null; then
				IS_SUPPORTED=true
			elif cat /etc/os-release | grep "stretch" > /dev/null; then
				IS_SUPPORTED=false
			elif cat /etc/os-release | grep "jessie" > /dev/null; then
				IS_SUPPORTED=true
			elif cat /etc/os-release | grep "wheezy" > /dev/null; then
				IS_SUPPORTED=false
			else
				IS_SUPPORTED=false
			fi
		fi
	fi
}

kernel_get() {
	KERNEL="$(uname -r)"
}

: <<'MAINSTART'

Perform all global variables declarations as well as function definition
above this section for clarity, thanks!

MAINSTART

# checks and inittab

#os_check
kernel_get

if [ $debugmode != "no" ]; then
	echo "USER_HOME is $USER_HOME" &&  newline
	echo "IS_SUPPORTED is $IS_SUPPORTED"
	newline

fi

if ! $IS_SUPPORTED; then
        warning "Your operating system is not supported, sorry. At this time only Raspbian-Buster is supported!"
        newline && exit 1
fi


newline
echo "This script will install everything needed to use"
echo $productname
newline
warning "--- Warning ---"
newline
echo "Always be careful when running scripts and commands"
echo "copied from the internet. Ensure they are from a"
echo "trusted source."
newline

if prompt "Do you wish to continue?"; then
	newline

	# AUDIO

	# Adding device tree entries
	echo "Audio - Adding Device Tree Entries to $CONFIG"
	if [ -e $CONFIG ] && grep -q -E "^dtoverlay=hifiberry-dac$" $CONFIG; then
		echo "  dtoverlay hifiberry-dac already active"
	else
		if [ -e $CONFIG ] && grep -q -E "^#dtoverlay=hifiberry-dac$" $CONFIG; then
			sudo sed -i "s|^#dtoverlay=hifiberry-dac$|dtoverlay=hifiberry-dac|" $CONFIG
		else
			echo "dtoverlay=hifiberry-dac" | sudo tee -a $CONFIG
		fi
		AUDIO_TEST_REQUIRED=true
		REBOOT_REQUIRED=true
	fi

	if [ -e $CONFIG ] && grep -q -E "^dtoverlay=i2s-mmap$" $CONFIG; then
		echo "  dtoverlay i2s-mmap already active"
	else
		if [ -e $CONFIG ] && grep -q -E "^#dtoverlay=i2s-mmap$" $CONFIG; then
			sudo sed -i "s|^#dtoverlay=i2s-mmap$|dtoverlay=i2s-mmap|" $CONFIG
		else
			echo "dtoverlay=i2s-mmap" | sudo tee -a $CONFIG
		fi
		AUDIO_TEST_REQUIRED=true
		REBOOT_REQUIRED=true
	fi

	if [ -e $CONFIG ] && grep -q -E "^#dtparam=audio=on$" $CONFIG; then
		echo "  dtparam audio=on already disabled"
	else
		if [ -e $CONFIG ] && grep -q -E "^dtparam=audio=on$" $CONFIG; then
			sudo sed -i "s|^dtparam=audio=on$|#dtparam=audio=on|" $CONFIG
		else
			echo "#dtparam=audio=on" | sudo tee -a $CONFIG
		fi
		AUDIO_TEST_REQUIRED=true
		REBOOT_REQUIRED=true
	fi

	echo "Audio - Adding gpio enable to $CONFIG"
	if [ -e $CONFIG ] && grep -q -E "^gpio=11=op,dh$" $CONFIG; then
		echo "  gpios 11 is already active"
	else
		if [ -e $CONFIG ] && grep -q -E "^#gpio=11=.*$" $CONFIG; then
			sudo sed -i "s|^#gpio=11=.*$|gpio=11=op,dh|" $CONFIG
		else
			echo "gpio=11=op,dh" | sudo tee -a $CONFIG
		fi
		REBOOT_REQUIRED=true
	fi

	# Editing modules for Audio
	echo "Audio - Remove the default sound option from $LOADMOD"
	if [ -e $LOADMOD ] && grep -q "^snd-bcm2835" $LOADMOD; then
		sudo sed -i "s|^snd-bcm2835|#snd-bcm2835|" $LOADMOD
		AUDIO_TEST_REQUIRED=true
	fi

	# Editing Blacklist for Audio
	echo "Audio - Remove the blacklist options from $BLACKLIST"
	if [ -e $BLACKLIST ]; then
		if grep -q -E "^blacklist[[:space:]]*i2c-bcm2708.*" $BLACKLIST; then
			sudo sed -i -e "s|^blacklist[[:space:]]*i2c-bcm2708.*|#blacklist i2c-bcm2708|" $BLACKLIST
			AUDIO_TEST_REQUIRED=true
		fi
		if grep -q -E "^blacklist[[:space:]]*snd-soc-pcm512x.*" $BLACKLIST; then
			sudo sed -i -e "s|^blacklist[[:space:]]*snd-soc-pcm512x.*|#blacklist snd-soc-pcm512x|" $BLACKLIST
			AUDIO_TEST_REQUIRED=true
		fi
		if grep -q -E "^blacklist[[:space:]]*snd-soc-wm8804.*" $BLACKLIST; then
			sudo sed -i -e "s|^blacklist[[:space:]]*snd-soc-wm8804.*|#blacklist snd-soc-wm8804|" $BLACKLIST
			AUDIO_TEST_REQUIRED=true
		fi
	fi

	# Configuring Sound Output
	echo "Audio - Configuring sound output"
	if [ -e ~/asound.conf.hdmi ]; then
		sudo rm -f ~/asound.conf.hdmi
	fi
	if [ -e ~/asound.conf.lcd ]; then
		sudo rm -f ~/asound.conf.lcd
	fi
	if [ -e /etc/asound.conf ]; then
		sudo rm -f /etc/asound.conf
	fi

	sudo touch ~/asound.conf.hdmi
    	sudo cat > ~/asound.conf.lcd << 'EOL'
pcm.speakerbonnet {
   type hw card 0
}

pcm.dmixer {
   type dmix
   ipc_key 1024
   ipc_perm 0666
   slave {
     pcm "speakerbonnet"
     period_time 0
     period_size 1024
     buffer_size 8192
     rate 44100
     channels 2
   }
}

ctl.dmixer {
    type hw card 0
}

pcm.softvol {
    type softvol
    slave.pcm "dmixer"
    control.name "PCM"
    control.card 0
}

ctl.softvol {
    type hw card 0
}

pcm.!default {
    type             plug
    slave.pcm       "softvol"
}
EOL

	sudo cp ~/asound.conf.lcd /etc/asound.conf

	# Configuring the aplay service to remove popping sounds
    echo "Audio - Installing aplay systemd unit"
    sudo sh -c 'cat > /etc/systemd/system/aplay.service' << 'EOL'
[Unit]
Description=Invoke aplay from /dev/zero at system start.

[Service]
ExecStart=/usr/bin/aplay -D default -t raw -r 44100 -c 2 -f S16_LE /dev/zero

[Install]
WantedBy=multi-user.target
EOL

	#sudo systemctl daemon-reload
	#STATUS="$(systemctl is-active aplay.service)"
	#if [ "${STATUS}" != "active" ];
	#then
	#	REBOOT_REQUIRED=true
	#fi
	#sudo systemctl enable aplay

	#newline

	# SCREEN SETUP

	echo "Screen - Adding Device Tree Entry to $CONFIG"
	if [ -e $CONFIG ] && grep -q -E "^#ignore_lcd=1$" $CONFIG; then
		echo "  lcd screen already setup"
	else
		if [ -e $CONFIG ] && grep -q -E "^ignore_lcd=1$" $CONFIG; then
			sudo sed -i "s|^ignore_lcd=1$|#ignore_lcd=1|" $CONFIG
		else
			echo "#ignore_lcd=1" | sudo tee -a $CONFIG
		fi
		REBOOT_REQUIRED=true
	fi

	if [ -e $CONFIG ] && grep -q -E "^cec_osd_name=.*" $CONFIG; then
		echo "  CEC name already setup"
	else
		if [ -e $CONFIG ] && grep -q -E "^#cec_osd_name=.*$" $CONFIG; then
			sudo sed -i "s|^#cec_osd_name=.*$|cec_osd_name=Aegis|" $CONFIG
		else
			echo "cec_osd_name=Aegis" | sudo tee -a $CONFIG
		fi
		REBOOT_REQUIRED=true
	fi

	echo "Screen - Set Brightness Level"
	sudo chmod 666 $BRIGHTNESS
	if [ -e $BRIGHTNESS ] && grep -q -E "^100$" $BRIGHTNESS; then
		echo "  screen brightness already setup"
	else
		sudo echo "100" > $BRIGHTNESS
		REBOOT_REQUIRED=true
	fi

	echo "Screen - Remove 4K output as an option when using HDMI"
	if [ -e $CONFIG ] && grep -q -E "^hdmi_max_pixel_freq:0=.*" $CONFIG; then
		echo "  HDMI max pixel freq:0 already setup"
	else
		if [ -e $CONFIG ] && grep -q -E "^#hdmi_max_pixel_freq:0=.*$" $CONFIG; then
			sudo sed -i "s|^#hdmi_max_pixel_freq:0=.*$|hdmi_max_pixel_freq:0=200000000|" $CONFIG
		else
			echo "hdmi_max_pixel_freq:0=200000000" | sudo tee -a $CONFIG
		fi
		REBOOT_REQUIRED=true
	fi
	if [ -e $CONFIG ] && grep -q -E "^hdmi_max_pixel_freq:1=.*" $CONFIG; then
		echo "  HDMI max pixel freq:1 already setup"
	else
		if [ -e $CONFIG ] && grep -q -E "^#hdmi_max_pixel_freq:1=.*$" $CONFIG; then
			sudo sed -i "s|^#hdmi_max_pixel_freq:1=.*$|hdmi_max_pixel_freq:1=200000000|" $CONFIG
		else
			echo "hdmi_max_pixel_freq:1=200000000" | sudo tee -a $CONFIG
		fi
		REBOOT_REQUIRED=true
	fi

	newline

	# VOLUME CONTROL GPIO

	echo "Volume Control - Adding Device Tree Entry to $CONFIG"
	if [ -e $CONFIG ] && grep -q -E "^gpio=9,10=ip,pu$" $CONFIG; then
		echo "  gpios 9 and 10 are already active"
	else
		if [ -e $CONFIG ] && grep -q -E "^#gpio=9,10=.*$" $CONFIG; then
			sudo sed -i "s|^#gpio=9,10=.*$|gpio=9,10=ip,pu|" $CONFIG
		else
			echo "gpio=9,10=ip,pu" | sudo tee -a $CONFIG
		fi
		REBOOT_REQUIRED=true
	fi

	newline

	# POWER/MISC BUTTON GPIO

	echo "Power Button - Adding Device Tree Entry to $CONFIG"
	if [ -e $CONFIG ] && grep -q -E "^gpio=7,8=ip,pu$" $CONFIG; then
		echo "  gpio 7 and 8 is already active"
	else
		if [ -e $CONFIG ] && grep -q -E "^#gpio=7,8=.*$" $CONFIG; then
			sudo sed -i "s|^#gpio=7,8=.*$|gpio=7,8=ip,pu|" $CONFIG
		else
			echo "gpio=7,8=ip,pu" | sudo tee -a $CONFIG
		fi
		REBOOT_REQUIRED=true
	fi

	newline

	# FAN GPIO

	echo "Power Button - Adding Device Tree Entry to $CONFIG"
	if [ -e $CONFIG ] && grep -q -E "^gpio=12=op,dl$" $CONFIG; then
		echo "  gpio 12 is already active"
	else
		if [ -e $CONFIG ] && grep -q -E "^#gpio=12=.*$" $CONFIG; then
			sudo sed -i "s|^#gpio=12=.*$|gpio=12=op,dl|" $CONFIG
		else
			echo "gpio=12=op,dl" | sudo tee -a $CONFIG
		fi
		REBOOT_REQUIRED=true
	fi

	newline

	# BATTERY COMMUNICATION

	echo "Battery Communication - Adding Device Tree Entry to $CONFIG"
	if [ -e $CONFIG ] && grep -q -E "^dtoverlay=i2c1,pins_2_3$" $CONFIG; then
		echo "  battery comms is already active"
	else
		if [ -e $CONFIG ] && grep -q -E "^#dtoverlay=i2c1,pins_2_3$" $CONFIG; then
			sudo sed -i "s|^#dtoverlay=i2c1,pins_2_3$|dtoverlay=i2c1,pins_2_3|" $CONFIG
		else
			echo "dtoverlay=i2c1,pins_2_3" | sudo tee -a $CONFIG
		fi
		REBOOT_REQUIRED=true
	fi

	echo "Battery Communication - Enable I2C in $LOADMOD"
	if [ -e $LOADMOD ] && grep -q -E "^i2c-dev$" $LOADMOD; then
		echo "  i2c-dev is already included"
	else
		if [ -e $LOADMOD ] && grep -q -E "^#i2c-dev$" $LOADMOD; then
			sudo sed -i "s|^#i2c-dev$|i2c-dev|" $LOADMOD
		else
			echo "i2c-dev" | sudo tee -a $LOADMOD
		fi
		REBOOT_REQUIRED=true
	fi

	if [ -e $LOADMOD ] && grep -q -E "^i2c-bcm2708$" $LOADMOD; then
		echo "  i2c-bcm2708 is already included"
	else
		if [ -e $LOADMOD ] && grep -q -E "^#i2c-bcm2708$" $LOADMOD; then
			sudo sed -i "s|^#i2c-bcm2708$|i2c-bcm2708|" $LOADMOD
		else
			echo "i2c-bcm2708" | sudo tee -a $LOADMOD
		fi
		REBOOT_REQUIRED=true
	fi

	newline

	# OVERLAY FILES

	echo "Overlay - Installing overlay icons"
	sudo cp -r ./overlay_icons /usr/local/bin/

	newline

	echo "PNGVIEW - Building pngview and copying to destination"
	sudo cp -r ./raspidmx /home/pi/
	curdir=$(pwd)
	cd /home/pi/raspidmx/
	make all
	sudo cp /home/pi/raspidmx/pngview/pngview /usr/local/bin/
	sudo cp /home/pi/raspidmx/monitor/monitor /usr/local/bin/
	sudo chmod +x /usr/local/bin/pngview
	sudo chmod +x /usr/local/bin/monitor
	sudo rm -rf /home/pi/raspidmx
	cd $curdir

	newline

	# STARTUP SCRIPT

	echo "Startup - Adding startup script to init.d"
	sudo cp ./startup.sh /etc/init.d/
	sudo chmod +x /etc/init.d/startup.sh
	sudo update-rc.d startup.sh defaults

	newline

	# SYSTEM MONITOR SCRIPT

	echo "System Monitor (deprecated) - Adding system monitor script"
	sudo cp ./system_monitor.py /usr/local/bin/
	sudo chmod +x /usr/local/bin/system_monitor.py

	newline

	# MULTI SWITCH SCRIPT

	echo "System Shutdown - Adding multi_switch script for shutdown"
	sudo cp ./multi_switch.sh /usr/local/bin/
	sudo chmod +x /usr/local/bin/multi_switch.sh

	newline

	# SPEAKER TEST

	if $AUDIO_TEST_REQUIRED; then
		# Install Volume Check script
		echo "Volume Check - Adding volume check script to rc.local"
		sudo cp ./volume_check.py /usr/local/bin/
		sudo chmod +x /usr/local/bin/volume_check.py

		if grep -q -E "^python /usr/local/bin/volume_check.py$" $RCLOCAL; then
			echo "  volume check already setup"
		else
			sudo sed -i "/^exit 0$/ipython /usr/local/bin/volume_check.py" $RCLOCAL
		fi

		newline 

		REBOOT_REQUIRED=true
	fi

	if prompt "Do you want to apply overclock settings?"; then
		echo "Overclock - Adding Overclock settings to $CONFIG"
		if [ -e $CONFIG ] && grep -q -E "^over_voltage=.*$" $CONFIG; then
			echo "  over_volrtage is already present"
		else
			if [ -e $CONFIG ] && grep -q -E "^#over_voltage=.*$" $CONFIG; then
				sudo sed -i "s|^#over_voltage=.*$|over_voltage=6|" $CONFIG
			else
				echo "over_voltage=6" | sudo tee -a $CONFIG
			fi
			REBOOT_REQUIRED=true
		fi

		if [ -e $CONFIG ] && grep -q -E "^arm_freq=.*$" $CONFIG; then
			echo "  arm_freq is already present"
		else
			if [ -e $CONFIG ] && grep -q -E "^#arm_freq=.*$" $CONFIG; then
				sudo sed -i "s|^#arm_freq=.*$|arm_freq=1800|" $CONFIG
			else
				echo "arm_freq=2000" | sudo tee -a $CONFIG
			fi
			REBOOT_REQUIRED=true
		fi

		if [ -e $CONFIG ] && grep -q -E "^gpu_freq=.*$" $CONFIG; then
			echo "  gpu_freq is already present"
		else
			if [ -e $CONFIG ] && grep -q -E "^#gpu_freq=.*$" $CONFIG; then
				sudo sed -i "s|^#gpu_freq=.*$|gpu_freq=600|" $CONFIG
			else
				echo "gpu_freq=600" | sudo tee -a $CONFIG
			fi
			REBOOT_REQUIRED=true
		fi

		newline 
	fi

	# ask for reboot
	if $REBOOT_REQUIRED; then
		sysreboot
	fi
else
    newline
    echo "Aborting..."
    newline
fi

exit 0
