#include <SPI.h>
#include <SD.h>
#include <RH_RF95.h>

// Feather M0: (don't ask me why we wired it this way)
#define RFM95_CS   5
#define RFM95_RST  6
#define RFM95_INT  9

// defines chip select pin on the feather
#define PIN_SPI_CS 4

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

File file;
char readfile;

void setup() {

  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(115200); // usb bus
  Serial1.begin(115200); // uart from sensor feather

  while (!Serial) delay(1);

  while (!Serial);

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

  Serial.println("Feather LoRa TX Test!");

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
  Serial.println("LoRa radio init OK!");

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
}

int16_t packetnum = 0;  // packet counter, we increment per xmission

void loop() {
  //delay(1000);
    //char radiopacket 
  char radiopacket[20] = "Data packet #      "; // default message
  itoa(packetnum++, radiopacket+13, 10);
  //Serial.print("Sending "); Serial.println(radiopacket);
  radiopacket[19] = 0;
  
  String dataString = "";

  if (Serial1.available()) {
    char inByte = Serial1.read();
    // Serial.write(inByte); // debug
    dataString += String(inByte);     
  }
  
  file = SD.open("stest4.txt", FILE_WRITE);
  // read from sensor thing, send to computer
  Serial.print(dataString);

  if (file) {
    //Serial.write(dataString);  
    file.print(dataString);
    file.close();
  }
     else { // error oopsies
    Serial.print(F("SD Card: error on opening file for writing"));
  }

  // Serial.print("SD Card ing, attempt "); Serial.println(packetnum);
  // do the file things (reading)
  file = SD.open("stest4.txt", FILE_READ);
  // Serial.println(file); // debug
  if (file) {

    while (file.available()) {
      readfile = file.read();
      
    }
    //Serial.println(readfile);
    file.close();
  }
   else { // error oopsies
    Serial.print(F("SD Card: error on opening file for reading"));
  }
  
  /*
  // delay(1000); // Wait 1 second between transmits, could also 'sleep' here!
  Serial.println("Transmitting..."); // Send a message to rf95_server
  delay(10);
  rf95.send((uint8_t *)inByte, 150);

  Serial.println("Waiting for packet to complete...");
  delay(10);
  rf95.waitPacketSent();
  // Now wait for a reply
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  Serial.println("Waiting for reply...");
  if (rf95.waitAvailableTimeout(2000)) {
    // Should be a reply message for us now
    if (rf95.recv(buf, &len)) {
      Serial.print("Got reply: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);
    } else {
      Serial.println("Receive failed");
    }
  } else {
    Serial.println("No reply, is there a listener around?");
  }
 */
}
