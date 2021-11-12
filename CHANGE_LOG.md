## Change Log

The change log of the Aegis 7" handheld.

Version Number Details:<br/>
x.0.0 - A new version number represents major changes to any and all aspects of the project. A new case and circuit board would be required.<br/>
0.x.0 - A new sub-version number represents changes to the circuit board and potentially software but no changes to the case<br/>
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

