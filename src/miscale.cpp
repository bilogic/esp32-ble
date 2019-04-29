#include "ssid.h"
#include "miscale.h"

void miscale::process(uint8_t *pData, size_t length)
{
    float weight;
    int unit;

    weight = ((float)(pData[2] << 8) + pData[1]) / 100;
    unit = 0;
    if ((pData[0] >> 0) & 1)
        unit = 1;
    if ((pData[0] >> 4) & 1)
        unit = 2;
    // Serial.print("unit: ");
    // Serial.println(unit);
    switch (unit)
    {
    case 0:
        weight = weight / 2;
        break;
    }

    Serial.print(weight);

    if ((pData[0] >> 5) & 1)
    {
        Serial.println(" stablized!");
    }

    if (((pData[0] >> 5) & 1) & ((pData[0] >> 7) & 1))
    {
        Serial.println("And ready!");

        // if (0)
        {
            if ((WiFi.status() == WL_CONNECTED))
            { //Check the current connection status

                HTTPClient http;

                String stringVal = "http://admin.techbox.com.dev.e115.com:8080/module/health/weight/record?value=";
                stringVal.concat(weight);

                http.begin(stringVal);     //Specify the URL
                int httpCode = http.GET(); //Make the request

                if (httpCode > 0)
                { //Check for the returning code
                    String payload = http.getString();
                    Serial.println(stringVal);
                    Serial.println(httpCode);
                    // Serial.println(payload);
                }

                else
                {
                    Serial.println("Error on HTTP request");
                }
                http.end(); //Free the resources
            }
            else
            {
                Serial.println("wifi not connected");
            }
        }
    }
    else
        Serial.println("");
}