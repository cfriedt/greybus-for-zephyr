.. _gpio:

****************
GPIO via Greybus
****************

After running ``gbridge``, if the board in use exposes GPIO via Greybus, then
the Linux kernel will create ``/dev/gpiochipN`` entries for you.

Just to make sure that we're interacting with the Greybus GPIO device, run
the following (toggling random GPIO can sometimes have negative consequences). 

.. code-block:: bash

    sudo gpiodetect | grep greybus_gpio
    gpiochip0 [greybus_gpio] (32 lines)

Most GPIO controllers expose a nubmer of GPIO, so it's important to know which
physical pin number should be toggled and what wiill happen when we do so.

In this case, we will toggle a number of pins on the ``cc1352r_sensortag``
that are each connected to separate red, green, and blue LEDs.

.. code-block:: bash

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

The :file:`blinky.sh` results can be seen on `YouTube <https://youtu.be/Nhoic1WIehA>`_. 
