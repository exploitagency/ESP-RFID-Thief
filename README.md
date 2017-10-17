# ESP-RFID-Thief
The ESP-RFID-Thief is a port of the Tastic RFID Thief(Originally created by Fran Brown from Bishop Fox) to the ESP12S chip. One of the benefits the ESP-RFID-Thief has over the original Tastic RFID Thief is the addition of WiFi and a web interface to review captured credentials. The on board flash also eliminates the need for an SD card. The device can be combined with a RFID reader that outputs Weigand data along with a battery pack to create a standalone RFID reader that saves all scanned cards to a log file accessible through the web interface.  The HID MaxiProx 5375 running on 12V(8xAA Batteries) can capture cards from up to two feet away when combined with this device. This device can also be planted inside existing RFID reader installations to capture card data.  
# Hardware License
Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)  
Hardware by Corey Harding  
# Software License
MIT License  
Based off the work of Fran Brown from Bishop Fox  
Ported to the ESP12S with a web interface by Corey Harding  
# Instructions
(INCOMPLETE)  
Gather parts  
Assemble board  
Program board using FTDI  
Install into RFID reader  
Configure settings  
  
Current software is taken from my original https://github.com/exploitagency/github-ESP_RFID_Thief project and will be brought up to speed using the same web interface you are used to in ESPloitV2 and ESPortalV2 shortly. Please be patient and wait for the updated version. This repo is mainly a placeholder for now. There may be minor hardware modifications but for now the hardware appears to be functional in my prototype units and I will mainly be focusing on software development and documentation for now.  
