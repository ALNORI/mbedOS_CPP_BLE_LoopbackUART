/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"
#include "nRF51822n.h"

#define BLE_UUID_NUS_SERVICE            0x0001                       /**< The UUID of the Nordic UART Service. */
#define BLE_UUID_NUS_TX_CHARACTERISTIC  0x0002                       /**< The UUID of the TX Characteristic. */
#define BLE_UUID_NUS_RX_CHARACTERISTIC  0x0003                       /**< The UUID of the RX Characteristic. */

#ifdef DEBUG
#define LOG(args...)    pc.printf(args)

Serial pc(USBTX, USBRX);
#else
#define LOG(args...)
#endif

nRF51822n   nrf;                /* BLE radio driver */

DigitalOut  led1(p1);
DigitalOut  advertisingStateLed(p30);

// The Nordic UART Service
uint8_t uart_base_uuid[] = {0x6e, 0x40, 0x01, 0x00, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9, 0xe5,0x0e, 0x24, 0xdc, 0xca, 0x9e};
uint8_t txPayload[8] = {0,};
uint8_t rxPayload[8] = {0,};
GattService        uartService (uart_base_uuid);
GattCharacteristic txCharacteristic (BLE_UUID_NUS_TX_CHARACTERISTIC, 1, 8,
                                     GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE);
GattCharacteristic rxCharacteristic (BLE_UUID_NUS_RX_CHARACTERISTIC, 1, 8,
                                     GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY);

/* Advertising data and parameters */
GapAdvertisingData   advData;
GapAdvertisingData   scanResponse;
GapAdvertisingParams advParams ( GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED );

uint16_t             uuid16_list[] = {BLE_UUID_NUS_SERVICE};

/**************************************************************************/
/*!
    @brief  This custom class can be used to override any GapEvents
            that you are interested in handling on an application level.
*/
/**************************************************************************/
class GapEventHandler : public GapEvents
{
    //virtual void onTimeout(void) {}

    virtual void onConnected(void) {
        advertisingStateLed = 0;
        LOG("Connected!\n\r");
    }

    /* When a client device disconnects we need to start advertising again. */
    virtual void onDisconnected(void) {
        nrf.getGap().startAdvertising(advParams);
        advertisingStateLed = 1;
        LOG("Disconnected!\n\r");
        LOG("Restarting the advertising process\n\r");
    }
};

/**************************************************************************/
/*!
    @brief  This custom class can be used to override any GattServerEvents
            that you are interested in handling on an application level.
*/
/**************************************************************************/
class GattServerEventHandler : public GattServerEvents
{
    virtual void onDataSent(uint16_t charHandle) {
        if (charHandle == txCharacteristic.handle) {
            LOG("onDataSend()\n\r");

        }
    }

    virtual void onDataWritten(uint16_t charHandle) {
        if (charHandle == txCharacteristic.handle) {
            LOG("onDataWritten()\n\r");
            nrf.getGattServer().readValue(txCharacteristic.handle, txPayload, 8); // Current APIs don't provide the length of the payload
            LOG("ECHO: %s\n\r", (char *)txPayload);
            nrf.getGattServer().updateValue(rxCharacteristic.handle, txPayload, 8);
        }
    }

    virtual void onUpdatesEnabled(uint16_t charHandle) {
        LOG("onUpdateEnabled\n\r");
    }

    virtual void onUpdatesDisabled(uint16_t charHandle) {
        LOG("onUpdatesDisabled\n\r");
    }

    //virtual void onConfirmationReceived(uint16_t charHandle) {}
};

/**************************************************************************/
/*!
    @brief  Program entry point
*/
/**************************************************************************/
int main(void)
{

    /* Setup an event handler for GAP events i.e. Client/Server connection events. */
    nrf.getGap().setEventHandler(new GapEventHandler());
    nrf.getGattServer().setEventHandler(new GattServerEventHandler());

    /* Initialise the nRF51822 */
    nrf.init();

    /* Make sure we get a clean start */
    nrf.reset();

    /* Add BLE-Only flag and complete service list to the advertising data */
    advData.addFlags(GapAdvertisingData::BREDR_NOT_SUPPORTED);
    advData.addData(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS,
                    (uint8_t*)uuid16_list, sizeof(uuid16_list));

    advData.addData(GapAdvertisingData::SHORTENED_LOCAL_NAME,
                    (uint8_t*)"BLE UART", sizeof("BLE UART") - 1);

    advData.addAppearance(GapAdvertisingData::UNKNOWN);
    nrf.getGap().setAdvertisingData(advData, scanResponse);

    /* UART Service */
    uartService.addCharacteristic(rxCharacteristic);
    uartService.addCharacteristic(txCharacteristic);
    nrf.getGattServer().addService(uartService);

    /* Start advertising (make sure you've added all your data first) */
    nrf.getGap().startAdvertising(advParams);
    
    advertisingStateLed = 1;

    /* Do blinky on LED1 while we're waiting for BLE events */
    for (;;) {
        led1 = !led1;
        wait(1);
    }
}
