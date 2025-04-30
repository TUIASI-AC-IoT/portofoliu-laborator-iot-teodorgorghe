/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "sl_common.h"
#include "sl_bt_api.h"
#include "app_assert.h"
#include "app.h"
#include "app_log.h"
#include "gatt_db.h"
#include "sl_bt_api.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

// Application Init.
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
}

// Application Process Action.
SL_WEAK void app_process_action(void)
{
  if (app_is_process_required()) {
    /////////////////////////////////////////////////////////////////////////////
    // Put your additional application code here!                              //
    // This is will run each time app_proceed() is called.                     //
    // Do not call blocking functions from here!                               //
    /////////////////////////////////////////////////////////////////////////////
  }
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  uint8_t recv_val;
  size_t recv_len;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);

      uint8_t flags = SL_BT_SM_CONFIGURATION_MITM_REQUIRED | SL_BT_SM_CONFIGURATION_BONDING_REQUIRED;
      sc = sl_bt_sm_configure(flags, sl_bt_sm_io_capability_keyboarddisplay);
      app_assert_status(sc);

      sc = sl_bt_sm_set_passkey(-1);
      app_assert_status(sc);

      sc = sl_bt_sm_set_bondable_mode(1);
      break;
    case sl_bt_evt_connection_opened_id:
      sc = sl_bt_sm_increase_security(evt->data.evt_connection_opened.connection);
      app_assert_status(sc);
      break;
    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////
    case sl_bt_evt_gatt_server_attribute_value_id:
      if (gattdb_LED_IO == evt->data.evt_gatt_server_characteristic_status.characteristic) {
          sl_bt_gatt_server_read_attribute_value(gattdb_LED_IO,
                                                  0,
                                                  sizeof(recv_val),
                                                  &recv_len,
                                                  &recv_val);
          if (recv_val) // Aprinde LED
            GPIO_PinOutSet(gpioPortA, 4);
          else // Stinge LED
            GPIO_PinOutClear(gpioPortA, 4);
          app_log("LED= %d\r\n", recv_val);
      }
      break;
    case sl_bt_evt_sm_passkey_display_id:
      app_log("Passkey %06d\r\n", evt->data.evt_sm_confirm_passkey.passkey);
      break;
    case sl_bt_evt_sm_bonded_id:
      app_log("Connection bounded successfully!\r\n");
      break;
    case sl_bt_evt_sm_bonding_failed_id:
      app_log("Connection failed to bound!\r\n");
      break;
    case sl_bt_evt_connection_parameters_id:
      app_log("Security mode info:\r\n");
      app_log("Interval %d\r\n", evt->data.evt_connection_parameters.interval);
      app_log("Latency %d\r\n", evt->data.evt_connection_parameters.latency);
      app_log("Security Mode %d\r\n", evt->data.evt_connection_parameters.security_mode);
      app_log("Timeout %d\r\n", evt->data.evt_connection_parameters.timeout);
      break;
    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
