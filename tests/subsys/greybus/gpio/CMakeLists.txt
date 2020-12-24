# SPDX-License-Identifier: Apache-2.0

#zephyr_include_directories(../../../../include)

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(greybus)

if(CONFIG_GREYBUS_ENABLE_TLS)

  set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

  if(CONFIG_GREYBUS_TLS_CLIENT_VERIFY_OPTIONAL OR CONFIG_GREYBUS_TLS_CLIENT_VERIFY_REQUIRED)

    generate_inc_file_for_target(
      app
      ${ZEPHYR_BASE}/samples/net/sockets/echo_server/src/server.der
      ${gen_dir}/greybus_client.inc
    )

    generate_inc_file_for_target(
      app
      ${ZEPHYR_BASE}/samples/net/sockets/echo_server/src/server_privkey.der
      ${gen_dir}/greybus_client_privkey.inc
    )

  else()

    # if client verification is not needed, then the client needs to
    # load these credentials
    generate_inc_file_for_target(
      app
      ${ZEPHYR_BASE}/samples/net/sockets/echo_server/src/ca.der
      ${gen_dir}/greybus_ca.inc
    )

  endif()
endif()

FILE(GLOB_RECURSE app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
