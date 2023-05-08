#!/usr/bin/env python

import os

os.system("speaker-test -l3 -c2 -t wav")
os.system("sed -i \"s|^python /usr/local/bin/volume_check.py$||\" /etc/rc.local")
os.system("reboot")
