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
int16_t recvFail = 0; // counts number of failed packets
int16_t recvTimeout = 0; // counts number of failed packets
int16_t packetnum = 0;  // packet counter, we increment per xmission
int16_t dirnum = 0;
int sdbarf = 0;
String condition = "";
String folder = "";
int verbose = 1; // barfs a lot 
int verbose_readfile = 0; // barfs even more
String filename = "barf-tx.txt"; // file to be written
String sensorfile = "";
int slowdown = 200; // delay between loop iterations
int dataStream = 1;  // defaults to basic packet, change to 1 if sensors are connected
int benchMode = 0; // 1 if on bench, 0 if flight

void setup() {
  // digitalWrite(PIN_LED3,HIGH);
  pinMode(RFM95_RST, OUTPUT); // config lora reset pin
  digitalWrite(RFM95_RST, HIGH); // make spi reset on lora

  Serial.begin(115200);   // usb bus to computer

  Serial1.begin(115200);  // uart from sensor feather

  if (benchMode){ // waits if the system is on the bench
    while (!Serial) delay(1);  // waits for serial to computer

    while (!Serial1) delay(1);  // waits for serial to sensor feather

    while (!Serial);  // waits
  }

  delay(500);
  Serial.print("--------------");
  Serial.print("Tx Startup Sequence");
  Serial.println("--------------");

  delay(500);
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
  delay(500);

  dirnum = 0;
  condition = "/tlog";
  condition += String(dirnum);

  while (SD.exists(condition)){
    dirnum++;
    condition = "/tlog";
    condition += String(dirnum);
    if (verbose) {
      Serial.println(condition);
    }
  }
  SD.mkdir(condition);

  // if (SD.exists("/tx-log-0")){
  //   dirnum++;
  //   folder = "/tx-log-";
  //   folder += String(dirnum);
  //   SD.mkdir(folder);
  // }
  // if (!SD.exists("/tx-log-0")){
  //   SD.mkdir("/tx-log-0");
  // }

  if (sdbarf) {
    Serial.println("Printing contents of SD Card:");
    root = SD.open("/");
    printDirectory(root, 0); // calls function at the bottom
  }

  delay(5000);
  Serial.println("Feather LoRa TX Module Starting!");

  // boop the LoRa spi
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  // consider changing the way this works
  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
    while (1)
      ;
  }
  Serial.println("LoRa Tx radio init OK!");

  // consider changing the way this works
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1)
      ;
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);
  Serial.println("Tx Power set to 23 dBm");
  delay(500);
  Serial.println("Debug info starting...");
  Serial.print("On the bench? ");
  Serial.println(benchMode);

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

  delay(slowdown); 
  
  char radiopacket[20] = "Data packet #      ";  // default message
  itoa(packetnum++, radiopacket + 13, 10);       // iteration counter
  if (verbose) {
    Serial.print("--------------");
    Serial.print(radiopacket);
    Serial.println("--------------");
  }
  radiopacket[19] = 0;  // not sure why this is here

  // get timestamp for packet
  time_t t = now();
  if (benchMode) {
    Serial.print("The pkt timestamp will be: ");
  }
  unsigned long h = hour(t);
  unsigned long m = minute(t);
  unsigned long s = second(t); 
 
  // create the preamble for the packet
  String preamble = "";  // preallocate preamble
  preamble += ",";
  preamble += String(h);
  preamble += String(m);
  preamble += String(s);
  if (benchMode) {
    Serial.println(preamble);
  }
  preamble += ",";
  preamble += String(packetnum);
  if (benchMode) {
    Serial.println(preamble);
  }
  preamble += ",";
  preamble += String(recvFail);
  preamble += ",";
  preamble += String(recvTimeout);
  preamble += ",";

  String sensorData = ""; // preallocate sensor data
  if (dataStream) { // skips if we know not connected to sensor feather
    while (Serial1.available()) {    // reads data from sensor UART if available
      char inByte = Serial1.read();  // reads to char
                                     // if (verbose) {
      Serial.write(inByte);          // debug
                                     // }
      sensorData += String(inByte);  // appends char to string
    }
  }
  preamble += String(sensorData.length()); // appends sensor data length
  if (benchMode) {
    Serial.println(preamble);
  }
  preamble += ",";

  String middleMan = ""; // preallocate temp variable to get length
  middleMan += preamble; // add preamble 
  if (benchMode) {
    Serial.println(middleMan);
  }
  middleMan += sensorData; // add sensor data
  if (benchMode) {
    Serial.println(middleMan);
  }

  String dataString = "";
  dataString += String(middleMan.length());
  dataString += middleMan; // this is the packet to be sent

  file = SD.open(filename, FILE_WRITE); // opens Tx SD card for data writing. 
                                        // also print to computer terminal
  
  if (verbose) {
    Serial.print("Write File Status: ");
    Serial.println(file);  // debug
  }

  if (verbose) {
    Serial.println("Sensor output: ");
    Serial.println(dataString);  // prints to computer
  }
  delay(10);
  if (file) {
    file.print(dataString);  // prints data into file
    file.close();  // don't need that anymore
  } else {         // error oopsies
    Serial.println(F("SD Card: error on opening Tx file for writing"));
  }

  if (verbose_readfile) { // this barfs SO much, you've been warned
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

  sensorfile = "";
  sensorfile += "/tlog";
  sensorfile += String(dirnum);
  sensorfile += "/";
  sensorfile += String(packetnum);
  sensorfile += "-t";
  sensorfile += ".txt";
  if (verbose) {
    Serial.println(sensorfile);
  }
  file = SD.open(sensorfile, FILE_WRITE);
  delay(10);
  if (file) {
    file.print(dataString);
    delay(10);
    file.close();
    delay(10);
  }

  if (verbose) {
    Serial.println("Transmitting...");  // Send a message to rf95_server
  }

  digitalWrite(LED_BUILTIN, HIGH);
  delay(10);
  int packet_len = dataString.length() + 1;
  char packet_array[packet_len];
  dataString.toCharArray(packet_array, packet_len);
  rf95.send((uint8_t *)packet_array, packet_len);
  if (verbose) {
    Serial.print("Tx pkt size: "); Serial.println(sizeof(packet_array));
    Serial.println("Tx pkt contents: "); Serial.println(packet_array);
  }
  if (verbose) {
    Serial.println("Sent. Waiting for acknowledgement...");
  }
  delay(10);
  rf95.waitPacketSent();
  // Now wait for a reply
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  // Serial.println("Waiting for reply...");
  if (rf95.waitAvailableTimeout(1000)) {
    // Should be a reply message for us now
    if (rf95.recv(buf, &len)) {
      if (verbose){
        Serial.print("Got reply: ");
        Serial.println((char *)buf);
      }
      if (verbose){
        Serial.print("RSSI: ");  // received signal strength index. signal is stronger when value is closer to 0.
        Serial.println(rf95.lastRssi(), DEC);
      }
      
    } else {
      if (verbose) {
        Serial.println("Receive failed");
      }
      recvFail++;
    }
  } else {
    if (verbose){
      Serial.println("Receive timeout.");
    }
    recvTimeout++;
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
