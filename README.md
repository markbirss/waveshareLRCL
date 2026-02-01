# esp32-s3-rlcd-42

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
