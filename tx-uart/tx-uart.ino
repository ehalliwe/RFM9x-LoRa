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
#include <SD.h>
#include <RH_RF95.h>

// Feather M0: (don't ask me why we wired it this way)
#define RFM95_CS 5   // SPI chip select
#define RFM95_RST 6  // SPI reset
#define RFM95_INT 9  // SPI interrupt

// defines chip select pin on the feather
#define PIN_SPI_CS 4  // for SD card

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0  // NA ISM band

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

File file;
File root;
char readfile;
int16_t packetnum = 0;  // packet counter, we increment per xmission
int verbose = 1;
int verbose_readfile = 0;
String filename = "sat-tx.txt";
int slowdown = 200;
int dataStream = 0;  // defaults to basic packet, change to 1 if sensors are connected
char time[10] = "";
int benchMode = 0; // 1 if on bench, 0 if flight

void setup() {

  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(115200);   // usb bus to computer
  Serial1.begin(115200);  // uart from sensor feather

  if (benchMode){
  while (!Serial) delay(1);  // waits for serial

  while (!Serial1) delay(1);  // waits for serial

  while (!Serial);  // waits if not worky
  }

  Serial.print("--------------");
  Serial.print("Tx Startup Sequence");
  Serial.println("--------------");

  Serial.print("Initializing SD card...");

  if (!SD.begin(PIN_SPI_CS)) {

    Serial.println("initialization failed. Things to check:");

    Serial.println("1. is a card inserted?");

    Serial.println("2. is your wiring correct?");

    Serial.println("3. did you change the chipSelect pin to match your shield or module?");

    Serial.println("Note: press reset or reopen this serial monitor after fixing your issue!");

    while (true)
      ;
  }
  Serial.println("initialization done.");
  delay(100);

  Serial.println("Contents of SD Card:");
  root = SD.open("/");
  printDirectory(root, 0);

  Serial.println("Feather LoRa TX Module Starting!");

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
    while (1)
      ;
  }
  Serial.println("LoRa Tx radio init OK!");

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1)
      ;
  }
  Serial.print("Set Freq to: ");
  Serial.println(RF95_FREQ);

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);

  Serial.println("Tx Power set to 23 dBm");

  Serial.print("Verbosity level: ");
  Serial.println(verbose);

  Serial.print("File in use: ");
  Serial.println(filename);

  Serial.print("Data stream of choice (0 for filler, 1 for sensors): ");
  Serial.println(dataStream);

  time_t t = now();
  Serial.print("The current time is: ");
  Serial.print(hour(t));Serial.print(minute(t));Serial.println(second(t));

  Serial.println("Here goes nothing...");
}

void loop() {

  delay(slowdown);  // send data only every 1 sec
  //char radiopacket
  
  // for whatever reason, i can't get this bit to work. please revisit it tomorrow.
  //char timepacket[20] = "Time packet =      "; // time packet
  //itoa(second(t), timepacket + 17, 10);
  //itoa(minute(t), timepacket + 15, 10);       // iteration counter
  //itoa(second(t), timepacket + 17, 10);       // iteration counter
  //Serial.print("--------------");
  //Serial.print(timepacket);
  //Serial.println("--------------");

  char radiopacket[20] = "Data packet #      ";  // default message
  itoa(packetnum++, radiopacket + 13, 10);       // iteration counter
  Serial.print("--------------");
  Serial.print(radiopacket);
  Serial.println("--------------");
  radiopacket[19] = 0;  // not sure why this is here

  time_t t = now();
  Serial.print("The pkt timestamp will be: ");
  unsigned long h = hour(t);
  unsigned long m = minute(t);
  unsigned long s = second(t); 
 
  String preamble = "";  // preallocate preamble
  preamble += ",";
  preamble += String(h);
  preamble += String(m);
  preamble += String(s);
  Serial.println(preamble);
  preamble += ",";

  preamble += String(radiopacket);
  Serial.println(preamble);
  preamble += ",";

  String sensorData = ""; // preallocate sensor data

  if (dataStream) {
    while (Serial1.available()) {    // reads data from sensor UART if available
      char inByte = Serial1.read();  // reads to char
                                     // if (verbose) {
      Serial.write(inByte);          // debug
                                     // }
      sensorData += String(inByte);  // appends char to string
    }
  }
  preamble += String(sensorData.length()); // appends sensor data length
  Serial.println(preamble);
  preamble += ",";

  String middleMan = ""; // preallocate temp variable to get length
  middleMan += preamble; // add preamble 
  Serial.println(middleMan);
  middleMan += sensorData; // add sensor data
  Serial.println(middleMan);

  String dataString = "";
  dataString += String(middleMan.length());
  dataString += middleMan; // this is the packet to be sent


  file = SD.open(filename, FILE_WRITE);  // opens Tx SD card for data writing. also print to computer terminal
  if (verbose) {
    Serial.print("Write File Status: ");
    Serial.println(file);  // debug
  }

  if (verbose) {
    Serial.println("Sensor output: ");
    Serial.println(dataString);  // prints to computer
  }

  if (file) {
    // if (verbose) {
    // Serial.write(dataString); // this now throws an error. NO IDEA WHY.
    // }
    if (dataStream) {
      file.print(dataString);  // prints data into file
    } else {
      file.print(radiopacket);  // test write of radiopacket data. turn off
    }
    file.close();  // don't need that anymore
  } else {         // error oopsies
    Serial.println(F("SD Card: error on opening Tx file for writing"));
  }

  // Serial.print("SD Card ing, attempt "); Serial.println(packetnum);
  // reading from file to check that the data was written in correctly. don't use this in the flight code

  if (verbose_readfile) {
    file = SD.open(filename, FILE_READ);
    Serial.print("Read File Status: ");
    Serial.println(file);  // debug

    if (file) {
      while (file.available()) {  // reads file from start to finish.. not sure how to do just the latest line.
        readfile = file.read();
        // Serial.write(readfile); // too much barf
      }
    }
    Serial.print("Readfile output: ");
    Serial.println(readfile);

    if (!file) {  // error oopsies
      Serial.println(F("SD Card: error on opening file for reading"));
    }
    file.close();
  }

  // now actually sending the data to the Rx feather + LoRa
  // if(dataStream) {
  //   int packet_len = dataString.length()+1 ;
  //   char packet_array[packet_len];
  //   dataString.toCharArray(packet_array, packet_len);
  // }

  Serial.println("Transmitting...");  // Send a message to rf95_server
  digitalWrite(LED_BUILTIN, HIGH);
  delay(10);
  if (dataStream) {
    int packet_len = dataString.length() + 1;
    char packet_array[packet_len];
    dataString.toCharArray(packet_array, packet_len);

    rf95.send((uint8_t *)packet_array, packet_len);
    if (verbose) {
      Serial.print("Tx pkt size: "); Serial.println(sizeof(packet_array));
      Serial.println("Tx pkt contents: "); Serial.println(packet_array);
    }
  } else {
    rf95.send((uint8_t *)radiopacket, 20);  // for testing if not connected
    if (verbose) {
      Serial.print("Tx Packet: ");
      Serial.println(radiopacket);
    }
  }
  Serial.println("Sent. Waiting for acknowledgement...");
  delay(10);
  rf95.waitPacketSent();
  // Now wait for a reply
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  // Serial.println("Waiting for reply...");
  if (rf95.waitAvailableTimeout(3000)) {
    // Should be a reply message for us now
    if (rf95.recv(buf, &len)) {
      Serial.print("Got reply: ");
      Serial.println((char *)buf);
      Serial.print("RSSI: ");  // received signal strength index. signal is stronger when value is closer to 0.
      Serial.println(rf95.lastRssi(), DEC);
    } else {
      Serial.println("Receive failed");
    }
  } else {
    Serial.println("Receive timeout.");
  }
  digitalWrite(LED_BUILTIN, LOW);

}

void printDirectory(File dir, int numTabs) {

  while (true) {

    File entry = dir.openNextFile();

    if (!entry) {

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
