.. _ble_setup:

**********************************
Bluetooth Low Energy Network Setup
**********************************

Thanks to the incredible amount of work that has gone into the Linux
networking stack, it is now fairly trivial to set up BLE connection
for 6LowPAN.

Simply run the following commands (substitute ``CA:27:C1:E9:6F:D0``
as appropriate for your device.

.. code-block:: bash

    sudo modprobe bluetooth_6lowpan
    echo 1 | sudo tee /sys/kernel/debug/bluetooth/6lowpan_enable
    echo "connect CA:27:C1:E9:6F:D0 2" | sudo tee /sys/kernel/debug/bluetooth/6lowpan_control

Additional information can be found
`here <https://docs.zephyrproject.org/1.13.0/samples/bluetooth/ipsp/README.html>`_.
