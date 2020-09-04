.. _i2c:

***************
I2C via Greybus
***************

After running ``gbridge``, if the board in use exposes I2C via Greybus, then
the Linux kernel will create ``/dev/i2c-N`` entries for you.

Just to make sure that we're interacting with the Greybus I2C device, run
the following (reading and writing to random I2C devices can sometimes have
negative consequences). 

.. code-block:: bash

    ls -la /sys/bus/i2c/devices/* | grep "greybus"
    lrwxrwxrwx 1 root root 0 Aug 15 11:24 /sys/bus/i2c/devices/i2c-8 -> ../../../devices/virtual/gb_nl/gn_nl/greybus1/1-2/1-2.2/1-2.2.2/gbphy2/i2c-8

In this case, we will read values a number of sensors on the ``cc1352r_sensortag``.

First, read the ID registers (at the i2c register address ``0x7e``) of the
``opt3001`` sensor (at i2c bus address ``0x44``) as shown below:

.. code-block:: bash

    i2cget -y 8 0x44 0x7e w
    0x4954
    

Next, read the ID registers (at the i2c register address ``0xfc``) of the
``hdc2080`` sensor (at i2c bus address ``0x41``) as shown below:

.. code-block:: bash

    i2cget -y 8 0x41 0xfc w
    0x5449

Lastly, we are going to probe the Linux kernel driver for the ``opt3001``
and use the Linux IIO subsystem to read ambient light values.

.. code-block:: bash

    echo opt3001 0x44 | sudo tee /sys/bus/i2c/devices/i2c-8/new_device
    cd /sys/bus/iio/devices/iio:device0
    cat in_illuminance_input
    163

What is very interesting about the last example is that we are using the
local Linux driver to interact with the remote I2C PHY. In other words,
our microcontroller does not even require a specific driver for the sensor
and we can maintain the driver code in the Linux kernel!
 