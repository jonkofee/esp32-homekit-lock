# Homekit lock with doorbell

<b>REPRODUCTION STEPS</b>

Open a terminal window on your mac.

```
docker pull espressif/idf:latest
```
- At this point idf (ESP-IDF v5.3-dev-2032-g4d90eedb6e)
```
git clone --recursive https://github.com/jonkofee/esp32-homekit-lock.git
```
```
docker run -it -v ./:/project -w /project espressif/idf:latest
```
```
idf.py set-target esp32
```
```
idf.py menuconfig
```
- Select `Serial flasher config` and then `Flash size (2MB)` set to `4MB`
- Select `Partition table` and then `Partition Table(Single factory app, no OTA)` Set to `Custom partition table CSV`
- Select `StudioPieters` and then `(mysid) WIFI SSID` and fill in your Wi-Fi Network name, then Select `(mypassword) WiFI Password` and fill in your Wi-Fi Network password.
- Then press `ESC` until you are asked `Save Configuration?` and select `(Y)es`
```
idf.py build
```
Open a new terminal window on your mac.
```
esptool.py -p /dev/tty.wchusbserial110 erase_flash
```
```
esptool.py -p /dev/tty.wchusbserial110 -b 460800 --before default_reset --after hard_reset --chip esp32  write_flash --flash_mode dio --flash_size detect --flash_freq 40m 0x1000 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/main.bin
```
- Replace `/dev/tty.usbserial-01FD1166` with your USB port.
```
screen /dev/tty.wchusbserial110 115200
```
- Replace `/dev/tty.usbserial-01FD1166` with your USB port.