Standalone executables of esptool can be downloaded from https://github.com/espressif/esptool/releases

Versions of esptool for Linux, Mac, and Windows are already included in the esptool folder

Connect your device to the computer via a USB cable.

Linux: give execute permission to esptool/linux/esptool and upload_linux.sh, then run upload_linux.sh to program your device.
Mac: give execute permission to esptool/mac/esptool and upload_mac.sh, then run upload_mac.sh to program your device.
Windows: double click upload_win.bat to program your device.


The program may appear to hang after you see text similar to the snippet below.
It is still working. It takes about a minute for it to erase the flash.
After that you will start getting output again.
---snip--
Wrote 367232 bytes (214663 compressed) at 0x00010000 in 8.0 seconds (effective 366.3 kbit/s)...
Hash of data verified.
Erasing flash...
Compressed 16187392 bytes to 8865443...
---snip--
After it has finished writing everything (takes about 6 minutes).
The program will appear to hang again.
After about 30 seconds, it will output a message and then exit.

The device should restart automatically. If not, press the black reset button on the side.
You should see text on the screen.
Press the left button to enter File Manager mode or wait and the images will start to play.

If you enter File Manager mode and have not yet entered your network info into the device, then a WiFi access point named JiftBox will be created.
The password is 123456789.
Once you have connected to the access point, open your web browser and go to http://jiftbox.local or http://192.168.4.1
Enter __your__ WiFi network's SSID and password, and if you like, a new mDNS address.
Click Save.
Restart the device.
Click the left button again to enter File Manager mode.
Now your device should be connected to your WiFi network.
Reconnect your computer to your WiFi.
Now when you browse to http://jiftbox.local you will be able to upload and delete images from your web browser.
Once you have finished file management, restart the device, wait a few seconds, and the images will play automatically.

