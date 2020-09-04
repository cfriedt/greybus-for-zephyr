# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(../../../include)

zephyr_library()

zephyr_library_sources(
	greybus-stubs.c
	greybus-core.c
	greybus-manifest.c

	platform/manifest.c
	platform/manifest-generator.c
	platform/platform.c
	platform/deferred-init.c

	platform/service.c

	platform/bundle.c
	platform/bus.c
	platform/control.c
	platform/interface.c
	platform/string.c

	qsort.c
)

zephyr_library_sources_ifdef(CONFIG_GREYBUS_XPORT_TCPIP    platform/transport-tcpip.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_XPORT_UART     platform/transport-uart.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_CONTROL        control-gpb.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_AUDIO          audio.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_CAMERA         camera.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_GPIO           gpio.c platform/gpio.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_DEBUG          greybus-debug.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_HID            hid.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_I2C            i2c.c platform/i2c.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_LIGHTS         lights.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_LOOPBACK       loopback.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_POWER_SUPPLY   power_supply.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_PWM            pwm-protocol.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_SDIO           sdio.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_SPI            spi.c platform/spi.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_UART           uart.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_USB            usb.c)
zephyr_library_sources_ifdef(CONFIG_GREYBUS_VIBRATOR       vibrator.c)
