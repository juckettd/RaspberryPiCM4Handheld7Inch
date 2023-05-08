## Change Log

The change log of the Aegis 7" handheld.

Version Number Details:<br/>
x.0.0 - A new version number represents major changes to any and all aspects of the project. A new case and/or circuit board would be required.<br/>
0.x.0 - A new sub-version number represents changes to the circuit board and potentially software but no changes to the case that would require printing<br/>
0.0.x - A new stage represents only software changes where the previous version/sub-version will still work with the updated software. There will not be a separate folder for this update since it is only a software update.<br/>

### Version 1.0.0

Initial Release of the Aegis

### Version 1.0.1

Updated the controller.ino arduino code
1. Improved rumble feedback to better handle minimum and maximum vibrations
2. Changed the range of the joystick to better account for expected values received by the teensy
 
Updated the system_monitor.sh script
1. Changed the operation of the power button to be both a reset and power button. When pressed once it will reset, when held down for ~2sec it will power down.
2. Added correct use of the multi_switch.sh script

Updated startup.sh script to enable/disable the HDMI_MODE and HDMI_GROUP when detecting an HDMI connection<br/>
Updated the multi_switch.sh script to remove the extra endline characters that caused errors<br/>

### Version 1.1.0

Replaced the 1.8/3.3 V regulator on the circuit board with a 3.3V boost/buck regulator
Removed the Wifi/BT chip and associated components
Added a battery fuel gauge circuit
Updated the system_monitor.sh to use the new fuel gauge circuit for the battery indicator
Updated the startup.sh script so that it no longer selects HDMI groups and modes
Updated the setup.sh script to remove the HDMI 4k options as a choice when using HDMI
Removed the 1.8V to 3.3V convert for audio as GPIO_REF is now set to 3.3V
Removed the pull-up resistors on the LED's as they are not needed
Increased the screw hole wall thickness to prevent them from breaking as often
Increased the thickness of the inner structure to prevent flexing
Changed the teensy controller file to update inputs more frequently and to reduce rumble motor intensity

### Version 2.0.0

Updated the system_monitor.sh to a monitor application using C++ instead of Python and updated all associated code
Updated the startup.sh script so that it installs the new monitor application
Removed the bluetooth icon from the display as it is rarely used
Added assembly instructions
Increased the size of many supports and internal structure of the case to make it more rigid (The old 1.1 or 1.0 case will still work if you don't want to print the new one)