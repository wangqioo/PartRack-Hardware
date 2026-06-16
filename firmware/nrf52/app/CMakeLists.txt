cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(part_rack_nrf52)

target_include_directories(app PRIVATE
  src
  ../../../protocol
)

target_sources(app PRIVATE
  src/main.c
  src/app_ble.c
  src/binding_table.c
  src/device_health.c
  src/light_control.c
  src/nfc_wake.c
  ../../../protocol/viberack_adv_payload.c
  ../../../protocol/viberack_ble_dispatcher_model.c
  ../../../protocol/viberack_ble_lifecycle.c
  ../../../protocol/viberack_binding_table_model.c
  ../../../protocol/viberack_device_health.c
  ../../../protocol/viberack_light_frame.c
  ../../../protocol/viberack_light_policy.c
  ../../../protocol/viberack_light_state.c
  ../../../protocol/viberack_protocol.c
  ../../../protocol/viberack_read_all_pacer.c
  ../../../protocol/viberack_storage.c
)
