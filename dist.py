#!/usr/bin/python3
import shutil

import os.path
from os import path

Import("env")


def dist(source, target, env):
  build_dir = env.subst("$BUILD_DIR")

  bootloader = f'{build_dir}/bootloader.bin'
  partitions = f'{build_dir}/partitions.bin'
  boot_app0 = env.get("FLASH_EXTRA_IMAGES")[2][1]
  firmware = f'{build_dir}/firmware.bin'
  littlefs = f'{build_dir}/littlefs.bin'


  if path.exists(bootloader):
    shutil.copy(bootloader, './dist/')
  else:
    print(f'{bootloader} not found.')
    
  if path.exists(partitions):
    shutil.copy(partitions, './dist/')
  else:
    print(f'{partitions} not found.')

  if path.exists(boot_app0):
    shutil.copy(boot_app0, './dist/')
  else:
    print(f'{boot_app0} not found.')

  if path.exists(firmware):
    shutil.copy(firmware, './dist/')
  else:
    print(f'{firmware} not found.')
    
  if path.exists(littlefs):
    shutil.copy(littlefs, './dist/')
  else:
    print(f'{littlefs} not found.')


  with open('./dist/README.TXT', 'w') as fr:
    print("""Standalone executables of esptool can be downloaded from https://github.com/espressif/esptool/releases

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
""", file=fr)

  upload_cmd = ""
  uf = env.subst(env.get("UPLOADERFLAGS"))
  i = 0
  while i < len(uf):
    # do not include port argument
    # counting on port being autodiscovered esptool. port names vary between OSes.
    if uf[i] == "--port":
      i += 2
      continue
    if uf[i].startswith("--"):
      upload_cmd += f" {uf[i]} {uf[i+1]}"
      i += 2
    else:
      break

  while i < len(uf):
    upload_cmd += f" {uf[i]} {os.path.basename(uf[i+1])}"
    i += 2

  upload_cmd += f" {env.get('ESP32_APP_OFFSET')} {env.get('PROGNAME')}.bin"
  upload_cmd += f" {env.get('FS_START')} {env.get('ESP32_FS_IMAGE_NAME')}.bin"

  linux_upload_cmd = "#!/bin/bash\n"
  linux_upload_cmd += r"./esptool/linux/esptool --no-stub" + upload_cmd
  mac_upload_cmd = "#!/bin/bash\n"
  mac_upload_cmd += r"./esptool/mac/esptool --no-stub" + upload_cmd  
  win_upload_cmd = r".\esptool\win\esptool.exe --no-stub" + upload_cmd

  with open('./dist/upload_linux.sh', 'w') as fl:
    print(linux_upload_cmd, file=fl, end='')
  with open('./dist/upload_mac.sh', 'w') as fm:
    print(mac_upload_cmd, file=fm, end='')
  with open('./dist/upload_win.bat', 'w') as fw:
    print(win_upload_cmd, file=fw, end='')




  """  
  with open('./dist/upload.bat', 'w') as f:
    print(r'.\esptool.exe --no-stub', file=f, end='')
    uf = env.subst(env.get("UPLOADERFLAGS"))
    i = 0
    while i < len(uf):
      # do not include port argument
      # counting on port being autodiscovered esptool. port names vary between OSes.
      if uf[i] == "--port":
        i += 2
        continue
      if uf[i].startswith("--"):
        print(f' {uf[i]} {uf[i+1]}', file=f, end='')
        i += 2
      else:
        break
        
    while i < len(uf):
      print(f' {uf[i]} {os.path.basename(uf[i+1])}', file=f, end='')
      i += 2

    print(f' {env.get("ESP32_APP_OFFSET")} {env.get("PROGNAME")}.bin', file=f, end='')
    print(f' {env.get("FS_START")} {env.get("ESP32_FS_IMAGE_NAME")}.bin', file=f, end='')
    """



#if "$BUILD_DIR/littlefs.bin" is used as the target then dist is called for both Build Filesystem Image and Upload Filesytem Image
# however Upload Filesytem Image does not set all of the same UPLOADERFLAGS that Build Filesystem Image, which crashes the python script
# therefore use "buildfs" as the target
#env.AddPostAction("$BUILD_DIR/littlefs.bin", dist)
env.AddPostAction("buildfs", dist)

