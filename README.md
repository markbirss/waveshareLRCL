# esp32-s3-rlcd-42

## PlatformIO version of https://github.com/VolosR/waveshareLRCL

```
sudo apt -y install python3-full python3-virtualenv
virtualenv .
source bin/activate
pip3 install platformio

pip3 install --upgrade pip
python3 -m pip install --upgrade setuptools

git clone https://github.com/markbirss/waveshareLRCL.git
cd waveshareLRCL/
source ~/bin/activate
pio run -e esp32-s3-rlcd-42 -t upload --upload-port /dev/ttyACMx
```

```
esptool --port /dev/ttyACM1 erase_flash
Warning: Deprecated: Command 'erase_flash' is deprecated. Use 'erase-flash' instead.
esptool v5.1.0
Connected to ESP32-S3 on /dev/ttyACM1:
Chip type:          ESP32-S3 (QFN56) (revision v0.2)
Features:           Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, Embedded PSRAM 8MB (AP_3v3)
Crystal frequency:  40MHz
USB mode:           USB-Serial/JTAG
MAC:                20:6e:f1:b8:3d:c4

Stub flasher running.

Flash memory erased successfully in 7.6 seconds.

Hard resetting via RTS pin...

```

