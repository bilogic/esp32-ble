/*
 *
 * This sketch emulates a Heart rate watch and is able to receive BLE signals of a Polar H7 Heart Rate Sensor.
 * It shows the received values in Serial and is also able to switch notificaton on the sensor on and off (using BLE2902)

   Copyright <2017> <Andreas Spiess>

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.

   Based on Neil Kolban's example file: https://github.com/nkolban/ESP32_BLE_Arduino
 */
#include <Arduino.h>
#include "BLEDevice.h"
//#include "BLEScan.h"

#include "ssid.h"
#include "miscale.h"
#include "bc85.h"

// { // miscale
// static BLEUUID serviceUUID(BLEUUID((uint16_t)0x181D));
// static BLEUUID charUUID(BLEUUID((uint16_t)0x2A9D));
// }

// { // BC85
static BLEUUID serviceUUID(BLEUUID((uint16_t)0x1810));
static BLEUUID charUUID(BLEUUID((uint16_t)0x2A35));
// }

static BLEAddress *pServerAddress;
static BLEClient *bclient = NULL;
static BLERemoteCharacteristic *pRemoteCharacteristic;

int state = 0;
static miscale *ms = new miscale();
static bc85 *bp = new bc85();

/**
   Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    /**
        Called for each advertising BLE server.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        Serial.print("BLE Advertised Device found: ");
        Serial.println(advertisedDevice.toString().c_str());

        // We have found a device, let us now see if it contains the service we are looking for.
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID))
        {
            advertisedDevice.getScan()->stop();
            pServerAddress = new BLEAddress(advertisedDevice.getAddress());

            Serial.println("Found our device!");
            state = 1;
        }
    }
};

class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *bclient)
    {
        Serial.println("onConnect");
    }

    void onDisconnect(BLEClient *bclient)
    {
        Serial.println("onDisconnect");
    }

    void onOpen(BLEClient *bclient)
    {
        Serial.println("onOpen");
        state = 2;
    }

    void onClose(BLEClient *bclient)
    {
        Serial.println("onClose");
        state = 1;
    }
};

static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    if (!isNotify)
        Serial.print("Indication callback for characteristic:");
    else
        Serial.print("Notification callback for characteristic:");

    for (int i = 0; i < length; i++)
    {
        Serial.print(pData[i]);
        Serial.print(" ");
    }
    Serial.println();

    // ms->process(pData, length);
    bp->process(pData, length);
}

void ble_scan()
{
    pServerAddress = nullptr;
    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->start(10, true);
}

void blec_register()
{
    bclient = BLEDevice::createClient();
    bclient->setClientCallbacks(new MyClientCallback());
    bclient->app_register();

    Serial.println("Registered a new client");
}

bool blec_open(BLEAddress pAddress)
{
    // Connect to the remote BLE Server.
    if (bclient->open(pAddress))
        return true;

    return false;
}

bool blec_enable_callback(BLEUUID sUUID, BLEUUID cUUID, notify_callback ncb)
{
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *pRemoteService = bclient->getService(sUUID);
    if (pRemoteService == nullptr)
    {
        Serial.print("Failed to find our service UUID: ");
        Serial.println(sUUID.toString().c_str());
        return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(cUUID);
    if (pRemoteCharacteristic == nullptr)
    {
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(cUUID.toString().c_str());
        return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    std::string value = pRemoteCharacteristic->readValue();
    Serial.print("The characteristic value was: ");
    Serial.println(value.c_str());

    Serial.println("Registering for indication/notification callback");
    pRemoteCharacteristic->registerForNotify(ncb, false);

    return true;
}

void setup()
{
    Serial.begin(1152000);
    Serial.println("Starting Arduino BLE Client application...");

    wifi_connect("ssid", "password");

    BLEDevice::init("");
    blec_register();
}

void loop()
{
    if (bclient != NULL)
    {
        switch (state)
        {
        case 0:
            ble_scan();
            break;

        case 1: // disconnected
            if (!bclient->isConnected())
            {
                if (pServerAddress == nullptr)
                {
                    state = 0;
                    Serial.println("No matching server, go back to scanning");
                }
                else
                {
                    Serial.print("Opening a virtual connection to ");
                    Serial.println(pServerAddress->toString().c_str());
                    if (blec_open(*pServerAddress))
                    {
                        Serial.println("Opened virtual connection to BLE Server.");
                    }
                    else
                    {
                        state = 0;
                        Serial.println("Connected, but unable to open virtual connection, go back to scanning");
                    }
                }
            }
            else
            {
                Serial.println("onDisconnect received, but isConnected() == true");
            }
            break;
        case 2: //connected, register notifications
            if (blec_enable_callback(serviceUUID, charUUID, notifyCallback))
            {
                Serial.println("Success! Waiting for callbacks");
                state = 3;
            }
            else
                state = 1;
            break;
        }
    }
}
