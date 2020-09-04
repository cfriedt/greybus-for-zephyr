.. _cc1352r_sensortag:

********************************
Greybus on the CC1352R SensorTag
********************************

To build the ``samples/subsys/greybus/net`` application for the
``cc1352r_sensortag`` the ``west build`` command line must be
slightly adjusted as shown below.

.. code-block:: bash

    west build \
      -b cc1352r_sensortag \
      -t flash \
      ../greybus-for-zephyr.git/samples/subsys/greybus/net \
      -- \
      -DOVERLAY_CONFIG=overlay-802154.conf \
      -DCONFIG_NET_CONFIG_IEEE802154_DEV_NAME="\"IEEE802154_0\"" \
      -DCONFIG_IEEE802154_CC13XX_CC26XX=y \
      -DCONFIG_IEEE802154_CC13XX_CC26XX_SUB_GHZ=n

This requirement is temporary but will be necessary until Zephyr is able to
autoconfigure network addresses for more than one network interface.

There is an issue in Zephyr tracking this problem
`here <https://github.com/zephyrproject-rtos/zephyr/issues/29750>`_.

The ``cc1352r_sensortag`` actually has 3 network interfaces to configure:

* 2.4 GHz IEEE 802.15.4
* Sub-GHz IEEE 802.15.4, and
* Bluetooth Low Energy
 