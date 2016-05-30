# Openhab-Irrigation
Irrigation controller using Openhab and the ESP8266 module

****************************Hardware****************************

Raspberry Pi running Openhab

NodeMCU ESP8266 development boards (frontyard and backyard zones)

8-channel relay boards

24VAC transformer

5VDC power supply (USB)

****************************System****************************

****************************ESP8266****************************

Controls sprinkler valves via 8 channel relay board

Does not execute sprinkler timing logic, scheduling done in Openhab

Receives open / close commands from Openhab

Provides status updates to Openhab via MQTT

Has "watchdog" timer limiting runtime of any zone, preventing lawn flooding if comms are lost

Only one zone can run at a time

****************************Openhab****************************

Allows for manual on / off via sitemap

Provides (4) programs

Provides weather compensation, via wunderground (or weather binding of your choice)


