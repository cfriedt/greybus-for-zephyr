.. _linux_software_requirements:

***************************
Linux Software Requirements
***************************

This document describes the common software requirements that a Linux machine
requires in order to connect to Greybus devices.

Recommended Linux Distribution
##############################

The recommended Linux distribution for evaluating Greybus is
`Ubuntu Focal Fossa <https://releases.ubuntu.com/20.04/>`_
(as of December, 2020) primarily because it already includes most of the
required Linux kernel modules and configuration.

If running Ubuntu natively is not an option, an alternative solution is to
run it inside of a virtual machine such as `Qemu <https://www.qemu.org/>`_
or `VirtualBox <https://www.virtualbox.org/>`_.

Greybus Linux Kernel Modules
############################

First, the main Greybus kernel modules will need to be added to the running
Linux kernel. 

.. code-block:: bash

    MODULES=$(ls /lib/modules/$(uname -r)/kernel/drivers/staging/greybus)
    for m in ${MODULES//.ko/}; do
        sudo modprobe $m
    done 

The gb-netlink Kernel Module
############################

Next, clone and build `Greybus <https://github.com/cfriedt/greybus>`_, and
insert the ``gb-netlink`` kernel module into the running Linux kernel.

.. code-block:: bash

    git clone -b gb_netlink https://github.com/cfriedt/gb-netlink.git
    cd gb-netlink
    make -j$(nproc --all)
    sudo insmod gb-netlink.ko 

Gbridge
#######

The final component required is `Gbridge <https://github.com/cfriedt/gbridge>`_
so clone and build that.

This step assumes that the ``gb-netlink`` directory is in the present working directory.

.. code-block:: bash

    git clone https://github.com/cfriedt/gbridge.git
    cd gbridge
    autoreconf -vfi
    GBDIR="$PWD/../gb-netlink" ./configure --disable-bluetooth --enable-tcpip --enable-uart --enable-netlink --disable-gbsim
    make -j$(nproc --all)

Finally, start ``gbridge`` to begin using Greybus!

.. code-block:: bash

    ./gbridge
