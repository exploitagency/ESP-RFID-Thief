#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>
// Library Documentation available at https://github.com/esp8266/Arduino/
// Install Library via Board Manager URL for Arduino IDE http://arduino.esp8266.com/stable/package_esp8266com_index.json
// Tastic RFID Thief Originally by Bishop Fox https://www.bishopfox.com/resources/tools/rfid-hacking/
// Remix of Code and Port to ESP8266 by Corey Harding from LegacySecurityGroup - https://www.legacysecuritygroup.com/ http://www.exploit.agency/


// Begin WiFi Configuration

const int accesspointmode = 1; // set to 0 to connect to an existing network or leave it set to 1 to use the esp8266 as an access point

// SSID and PASSWORD of network go below
const char ssid[] = "RFID";
const char password[] = "";
// channel and hidden are for when using the esp8266 as an access point
const int channel = 6;
const int hidden = 0; // set int hidden to 0 to broadcast SSID of access point or leave as 1 to hide SSID

// Configure the Network
IPAddress local_IP(192,168,1,1); //IP of the esp8266 server
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

// Port for web server
ESP8266WebServer server(80);

// End of WiFi Configuration block


// Begin RFID Thief Config


#define MAX_BITS 100                 // max number of bits 
#define WEIGAND_WAIT_TIME  3000      // time to wait for another weigand pulse.  


unsigned char databits[MAX_BITS];    // stores all of the data bits
volatile unsigned int bitCount = 0;
unsigned char flagDone;              // goes low when data is currently being captured
unsigned int weigand_counter;        // countdown until we assume there are no more bits

volatile unsigned long facilityCode=0;        // decoded facility code
volatile unsigned long cardCode=0;            // decoded card code

// Breaking up card value into 2 chunks to create 10 char HEX value
volatile unsigned long bitHolder1 = 0;
volatile unsigned long bitHolder2 = 0;
volatile unsigned long cardChunk1 = 0;
volatile unsigned long cardChunk2 = 0;


///////////////////////////////////////////////////////
// Process interrupts

// interrupt that happens when INTO goes low (0 bit)
void ISR_INT0()
{
  //Serial.print("0");
  bitCount++;
  flagDone = 0;
  
  if(bitCount < 23) {
      bitHolder1 = bitHolder1 << 1;
  }
  else {
      bitHolder2 = bitHolder2 << 1;
  }
    
  weigand_counter = WEIGAND_WAIT_TIME;  
  
}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1()
{
  //Serial.print("1");
  bitCount++;
  flagDone = 0;
  
   if(bitCount < 23) {
      bitHolder1 = bitHolder1 << 1;
      bitHolder1 |= 1;
   }
   else {
     bitHolder2 = bitHolder2 << 1;
     bitHolder2 |= 1;
   }
  
  weigand_counter = WEIGAND_WAIT_TIME;  
}
// End RFID Thief Config

// Dish out Root Web Page
void handle_root() {
  server.send(200, "text/html", "<html><body>ESP-RFID-Thief<br>A Tastic RFID Thief Port/Remix by:<br>Corey Harding from www.LegacySecurityGroup.com<br>-----<br><a href=\"/log\">View /log.txt</a></html><br>-<br><a href=\"/wipe\">Wipe /log.txt</a><br>-<br><a href=\"/format\">Format File System</a></html>");
}
String webString="";
// End of Dish


// Start Networking
void setup()
{
  Serial.begin(9600);
  Serial.println();

// Determine if set to Access point mode
  if (accesspointmode == 1) {
    Serial.print("Setting up Network Configuration ... ");
    Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Success" : "Failed!");

    Serial.print("Starting Access Point ... ");
    Serial.println(WiFi.softAP(ssid, password, channel, hidden) ? "Success" : "Failed!");

    Serial.print("IP address = ");
    Serial.println(WiFi.softAPIP());
  }
// or Join existing network
  else if (accesspointmode != 1) {
    Serial.print("Setting up Network Configuration ... ");
    Serial.println(WiFi.config(local_IP, gateway, subnet) ? "Success" : "Failed!");

    Serial.print("Connecting to network ... ");
    Serial.println(WiFi.begin(ssid, password) ? "Success" : "Failed!");

    Serial.print("IP address = ");
    Serial.println(WiFi.localIP());
  }


// Initialize file system and log file
  SPIFFS.begin();
  // this opens the file "log.txt" in read-mode
  File f = SPIFFS.open("/log.txt", "r");
  
  if (!f) {
    Serial.println("File doesn't exist yet. Creating it");
    // open the file in write mode
    File f = SPIFFS.open("/log.txt", "w");
    if (!f) {
      Serial.println("File creation failed!");
    }
    f.println("File: /log.txt");
    f.println("Captured Cards:");
  }
  f.close();
// End file system block
    
// Begin Web Pages
  server.on("/", handle_root);
  
  server.on("/log", [](){
    webString="";
    File f = SPIFFS.open("/log.txt", "r");
    String webString = f.readString();
    f.close();
    server.send(200, "text/plain", webString);
    Serial.println(webString);
    webString="";
  });

  server.on("/wipe", [](){
    server.send(200, "text/html", "<html><body>This will wipe all your captures from /log.txt file.<br><br>Are you sure?<br><br><a href=\"/wipe/yes\">YES</a> - <a href=\"/\">NO</a></html>");
  });

  server.on("/wipe/yes", [](){
    server.send(200, "text/html", "Logs have been wiped.<br><br><a href=\"/\"><- BACK TO INDEX</a>");
    File f = SPIFFS.open("/log.txt", "w");
    f.println("File: /log.txt");
    f.println("Captured Cards:");
    f.close();
    Serial.println("Logs wiped");
  });

  server.on("/format", [](){
    server.send(200, "text/html", "<html><body>This will reformat the SPIFFS File System.<br><br>Are you sure?<br><br><a href=\"/format/yes\">YES</a> - <a href=\"/\">NO</a></html></html>");
  });

  server.on("/format/yes", [](){
    server.send(200, "text/html", "Formatting file system<br>This may take up to 90 seconds<br><br><a href=\"/\"><- BACK TO INDEX</a>");
    Serial.print("Formatting file system...");
    SPIFFS.format();
    Serial.println(" Success");
  });

  server.begin();
  Serial.println("HTTP Server Started");
// End of Web Pages

//Start RFID Reader

  pinMode(2, OUTPUT);  // LED
  pinMode(14, INPUT);     // DATA0 (INT0)
  pinMode(12, INPUT);     // DATA1 (INT1)
  
  Serial.println("RFID Reader Started");
  
  // binds the ISR functions to the falling edge of INTO and INT1
  attachInterrupt(14, ISR_INT0, FALLING);  
  attachInterrupt(12, ISR_INT1, FALLING);

  weigand_counter = WEIGAND_WAIT_TIME;
}
//

//Do It!

///////////////////////////////////////////////////////
// LOOP function
void loop()
{

  // This waits to make sure that there have been no more data pulses before processing data
  if (!flagDone) {    

    if (--weigand_counter == 0)
      flagDone = 1;  

  }
  
  // if we have bits and we the weigand counter went out
  if (bitCount > 0 && flagDone) {
    unsigned char i;
    
    getCardValues();
    getCardNumAndSiteCode();
       
    printBits();

     // cleanup and get ready for the next card
     bitCount = 0; facilityCode = 0; cardCode = 0;
     bitHolder1 = 0; bitHolder2 = 0;
     cardChunk1 = 0; cardChunk2 = 0;
     
     for (i=0; i<MAX_BITS; i++) 
     {
       databits[i] = 0;
     }
  }
  
  // Check if someone is requesting a Web Page
  server.handleClient();

}

///////////////////////////////////////////////////////
// PRINTBITS function
void printBits()
{
      
      // open the file in append mode
      File f = SPIFFS.open("/log.txt", "a");
      f.print(bitCount);
      f.print(" bit card : ");
      //f.print(facilityCode);
      //f.print(" : ");
      //f.print(cardCode);
      //f.print(" : ");
      f.print("HEX = ");
      f.print(cardChunk1, HEX);
      f.println(cardChunk2, HEX);
      f.close();

      Serial.print(bitCount);
      Serial.print(" bit card : ");
      //Serial.print("FC = ");
      //Serial.print(facilityCode, HEX);
      //Serial.print(", CC = ");
      //Serial.print(cardCode, HEX);
      Serial.print(", HEX = ");
      Serial.print(cardChunk1, HEX);
      Serial.println(cardChunk2, HEX);

}


///////////////////////////////////////////////////////
// SETUP function
void getCardNumAndSiteCode()
{
     unsigned char i;
  
    // we will decode the bits differently depending on how many bits we have
    // see www.pagemac.com/azure/data_formats.php for more info
    // also specifically: www.brivo.com/app/static_data/js/calculate.js
    switch (bitCount) {

      
    ///////////////////////////////////////
    // standard 26 bit format
    // facility code = bits 2 to 9  
    case 26:
      for (i=1; i<9; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      
      // card code = bits 10 to 23
      for (i=9; i<25; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }
      break;

    ///////////////////////////////////////
    // 33 bit HID Generic    
    case 33:  
      for (i=1; i<8; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      
      // card code
      for (i=8; i<32; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }    
      break;

    ///////////////////////////////////////
    // 34 bit HID Generic 
    case 34:  
      for (i=1; i<17; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      
      // card code
      for (i=17; i<33; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }    
      break;
 
    ///////////////////////////////////////
    // 35 bit HID Corporate 1000 format
    // facility code = bits 2 to 14     
    case 35:  
      for (i=2; i<14; i++)
      {
         facilityCode <<=1;
         facilityCode |= databits[i];
      }
      
      // card code = bits 15 to 34
      for (i=14; i<34; i++)
      {
         cardCode <<=1;
         cardCode |= databits[i];
      }    
      break;

    }
    return;
  
}


//////////////////////////////////////
// Function to append the card value (bitHolder1 and bitHolder2) to the necessary array then tranlate that to
// the two chunks for the card value that will be output
void getCardValues() {
  switch (bitCount) {
    case 26:
        // Example of full card value
        // |>   preamble   <| |>   Actual card value   <|
        // 000000100000000001 11 111000100000100100111000
        // |> write to chunk1 <| |>  write to chunk2   <|
        
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 2){
            bitWrite(cardChunk1, i, 1); // Write preamble 1's to the 13th and 2nd bits
          }
          else if(i > 2) {
            bitWrite(cardChunk1, i, 0); // Write preamble 0's to all other bits above 1
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 20)); // Write remaining bits to cardChunk1 from bitHolder1
          }
          if(i < 20) {
            bitWrite(cardChunk2, i + 4, bitRead(bitHolder1, i)); // Write the remaining bits of bitHolder1 to cardChunk2
          }
          if(i < 4) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i)); // Write the remaining bit of cardChunk2 with bitHolder2 bits
          }
        }
        break;

    case 27:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 3){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 3) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 19));
          }
          if(i < 19) {
            bitWrite(cardChunk2, i + 5, bitRead(bitHolder1, i));
          }
          if(i < 5) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 28:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 4){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 4) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 18));
          }
          if(i < 18) {
            bitWrite(cardChunk2, i + 6, bitRead(bitHolder1, i));
          }
          if(i < 6) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 29:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 5){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 5) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 17));
          }
          if(i < 17) {
            bitWrite(cardChunk2, i + 7, bitRead(bitHolder1, i));
          }
          if(i < 7) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 30:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 6){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 6) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 16));
          }
          if(i < 16) {
            bitWrite(cardChunk2, i + 8, bitRead(bitHolder1, i));
          }
          if(i < 8) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 31:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 7){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 7) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 15));
          }
          if(i < 15) {
            bitWrite(cardChunk2, i + 9, bitRead(bitHolder1, i));
          }
          if(i < 9) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 32:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 8){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 8) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 14));
          }
          if(i < 14) {
            bitWrite(cardChunk2, i + 10, bitRead(bitHolder1, i));
          }
          if(i < 10) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 33:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 9){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 9) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 13));
          }
          if(i < 13) {
            bitWrite(cardChunk2, i + 11, bitRead(bitHolder1, i));
          }
          if(i < 11) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 34:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 10){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 10) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 12));
          }
          if(i < 12) {
            bitWrite(cardChunk2, i + 12, bitRead(bitHolder1, i));
          }
          if(i < 12) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 35:        
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 11){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 11) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 11));
          }
          if(i < 11) {
            bitWrite(cardChunk2, i + 13, bitRead(bitHolder1, i));
          }
          if(i < 13) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 36:
       for(int i = 19; i >= 0; i--) {
          if(i == 13 || i == 12){
            bitWrite(cardChunk1, i, 1);
          }
          else if(i > 12) {
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 10));
          }
          if(i < 10) {
            bitWrite(cardChunk2, i + 14, bitRead(bitHolder1, i));
          }
          if(i < 14) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;

    case 37:
       for(int i = 19; i >= 0; i--) {
          if(i == 13){
            bitWrite(cardChunk1, i, 0);
          }
          else {
            bitWrite(cardChunk1, i, bitRead(bitHolder1, i + 9));
          }
          if(i < 9) {
            bitWrite(cardChunk2, i + 15, bitRead(bitHolder1, i));
          }
          if(i < 15) {
            bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
          }
        }
        break;
  }
  return;
}
