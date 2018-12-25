ARDUINO_PATH=~/arduino-1.8.5

flash:
    cd $ARDUINO_PATH
    ./arduino --port /dev/ttyACM0 --board  arduino:avr:uno --upload ../pi-scripts/arduino/sensors-management/Weather_1/Weather_1.ino
    cd -