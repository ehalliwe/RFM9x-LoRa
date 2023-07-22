/////////////////////////////////////////////////////////////////////////////////////////////
// 
// E F Halliwell 
// Summer 2023
// RMC 549 Space Mission Design
// Sensors -> MISC. -> Sci. Feather -> UART -> LoRa Feather -> SPI -> LoRa Module 
//
/////////////////////////////////////////////////////////////////////////////////////////////

#include <TimeLib.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <SD.h>

// Feather M0: (don't ask me why we wired it this way)
#define RFM95_CS   5 
#define RFM95_RST  6
#define RFM95_INT  9

// defines chip select pin on the feather
#define PIN_SPI_CS 4 // for SD card

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

File file;
File root;
char readfile;
int16_t packetnum = 0;  // packet counter, we increment per xmission
int verbose = 0;
String filename = "sat-rx.txt";

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(115200); // usb bus to computer
  while (!Serial) delay(1);
  delay(100); // master Rx delay timer

  while (!Serial); // waits for serial
	
  Serial.print ("--------------"); Serial.print("Rx Startup Sequence"); Serial.println("--------------");
  Serial.print("Initializing SD card...");

  if (!SD.begin(PIN_SPI_CS)) {

    Serial.println("initialization failed. Things to check:");

    Serial.println("1. is a card inserted?");

    Serial.println("2. is your wiring correct?");

    Serial.println("3. did you change the chipSelect pin to match your shield or module?");

    Serial.println("Note: press reset or reopen this serial monitor after fixing your issue!");

    while (true);

  }
  Serial.println("initialization done.");
  delay(100);

  Serial.println("Contents of SD Card:");
  root = SD.open("/");
  printDirectory(root, 0);

  Serial.println("Feather LoRa RX Module Starting!");

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
    while (1);
  }
  Serial.println("LoRa Rx radio init OK!");

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);

	Serial.println("Tx Power set to 23 dBm");
	
	Serial.print("Verbosity level: "); Serial.println(verbose);
	
	Serial.print("File in use: "); Serial.println(filename);

  time_t t = now();
  Serial.print("The current time is: ");
  Serial.print(hour(t));Serial.print(minute(t));Serial.println(second(t));

	Serial.println("Here goes nothing...");
}

void loop() {
  // Serial.println("Waiting for signal...");
  // Serial.println(rf95.available());
  if (rf95.available()) {
    // Should be a message for us now
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len)) {
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("---------------------------------------------");
      time_t t = now();
      Serial.print("Packet recieved at internal time: "); 
      Serial.print(hour(t));Serial.print(minute(t));Serial.println(second(t));
  
      RH_RF95::printBuffer("Received: ", buf, len);
      Serial.print("Got: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);
      // Serial.print("Rx pkt size: "); Serial.println(sizeof(buf)); // always 251. duh
      
      file = SD.open(filename, FILE_WRITE); // create file to write data to
      if (file) {
        file.print((char*)buf);
        file.close();
      }
      else {
        Serial.print(F("SD Card: error on opening Rx file for writing"));
      }

      // read and print for debug; don't need this in the final... maybe
      // start of debug block
      if (verbose) {
      	file = SD.open(filename, FILE_READ);
      	// Serial.println(file); // debug
      		if (file) {
        	while (file.available()) { // reads file from start to finish..
               	                           // not sure how to do just the latest line. 
          	readfile = file.read();
          
          	Serial.print(readfile);
        	}
        file.close();
      	}
      	else { // error oopsies
        	Serial.print(F("SD Card: error on opening file for reading"));
      	}
      } 

      // Send a reply. Don't actually need this... maybe. 
      // please decide to comment out or not!
      uint8_t data[] = "Packet received.";
      rf95.send(data, sizeof(data));
      rf95.waitPacketSent();
      Serial.println("Sent a reply");
      digitalWrite(LED_BUILTIN, LOW);
    } 
    else {
      Serial.println("Receive failed");
    }
  }
}

void printDirectory(File dir, int numTabs) {

  while (true) {

    File entry =  dir.openNextFile();

    if (! entry) {

      // no more files

      break;

    }

    for (uint8_t i = 0; i < numTabs; i++) {

      Serial.print('\t');

    }

    Serial.print(entry.name());

    if (entry.isDirectory()) {

      Serial.println("/");

      printDirectory(entry, numTabs + 1);

    } else {

      // files have sizes, directories do not

      Serial.print("\t\t");

      Serial.println(entry.size(), DEC);

    }

    entry.close();

  }
}
