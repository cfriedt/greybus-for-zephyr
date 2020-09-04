.. _spi:

***************
SPI via Greybus
***************

After running ``gbridge``, if the board in use exposes SPI via Greybus, and
the user has run ``modprobe spidev``, then the Linux kernel will create
``/dev/spidevN.N`` entries for you.

Just to make sure that we're interacting with the Greybus SPI device, run
the following (reading and writing to random I2C devices can sometimes have
negative consequences). 

.. code-block:: bash

    ls -la /sys/bus/spi/devices/* | grep "greybus"
    lrwxrwxrwx 1 root root 0 Dec 11 15:01 /sys/bus/spi/devices/spi0.0 -> ../../../devices/virtual/gb_netlink/gb_netlink/greybus1/1-2/1-2.2/1-2.2.3/gbphy1/spi_master/spi0/spi0.0

In this case, we will read values a sensor on the ``cc1352r_sensortag``.

Since we don't yet have generic userspace SPI tools like we do for I2C,
a small application was written specifically to read this sensor.

The source code for the application can be found
`here <https://github.com/cfriedt/cc1352r-sensortag-adxl362-greybus-demo>`_.

To clone and build the application, simply run the following:

.. code-block:: bash

    git clone https://github.com/cfriedt/cc1352r-sensortag-adxl362-greybus-demo.git adxl362
    cd adxl362
    make -j$(nproc --all)

Next, we will stream accelerometer values from the ``adxl362``.

.. code-block:: bash

    sudo ./adxl362
    (x,y,z,°C) =   -0.823758,   0.147099,  11.169774, -28.990000
    (x,y,z,°C) =   -0.823758,   0.147099,  11.169774, -28.990000
    (x,y,z,°C) =   -0.804145,   0.156906,  11.140354, -28.990000

