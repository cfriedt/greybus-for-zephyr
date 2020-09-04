#!/bin/bash

# Blinky Demo for CC1352R SensorTag

# /dev/gpiochipN that Greybus created
CHIP="$(gpiodetect | grep greybus_gpio | head -n 1 | awk '{print $1}')"

# red, green, blue LED pin numbers
RED=6
GREEN=7
BLUE=21

# Bash array for pins and values
PINS=($RED $GREEN $BLUE)
NPINS=${#PINS[@]}

for ((count=0; count < $COUNT; count++)); do
	for i in ${!PINS[@]}; do
		# turn off previous pin
		if [ $i -eq 0 ]; then
			PREV=$((NPINS-1))
		else
			PREV=$((i-1))
		fi
		gpioset $CHIP ${PINS[$PREV]}=0

		# turn on current pin
		gpioset $CHIP ${PINS[$i]}=1
	done
done

