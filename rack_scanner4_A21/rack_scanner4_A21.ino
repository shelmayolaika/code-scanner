#include <WiFi.h>
#include <WiFiUdp.h>
//pin komunikasi Serial2 untuk komunikasi dengan modul RFID
#define rx2 16
#define tx2 17

//VARIABEL
const int maxTag = 100;
const int maxRespByte = 30;
const int lenEPC = 12;
const int readTime = 2000;
byte resp[maxTag][maxRespByte];
byte readyToSend[maxTag][lenEPC];
byte header[] = { 0xAA, 0xAA, 0xFF };
unsigned long startTime = 0;
bool isError = false;  //variabel untuk menjalankan task ledBlink
bool isStocked = false;
bool isPicked = false;
int mode = 0;
int tagValid = 0;  //jumlah tag yang valid dan siap dikirim ke raspi
int tag = -1;      //jumlah tag yang terbaca sekaligus menjadi indeks dalam matriks resp
//pin LED
int redLED = 25;
int greenLED = 32;

// NETWORK
const char* ssid = "octagon";
const char* password = "raspberrypi";
IPAddress ip(192, 168, 4, 1);  //IP address raspi
const int raspiPortStocking = 49154;
const int raspiPortSO = 49156;
const int raspiPortPicking = 49158;
const int localPort = 1237;
WiFiUDP udp;

//COMMAND
//command untuk membaca device address
byte cmd1[] = { 0xAA, 0xAA, 0xFF, 0x06, 0x05, 0x01, 0xFF, 0x1B, 0xA2 };
//command untuk setting frequency region 920.125~924.875MHz
byte cmd2[] = { 0xAA, 0xAA, 0xFF, 0x06, 0x30, 0x00, 0x01, 0x08, 0x17 };
//command untuk setting work frequency channel
byte cmd3[] = { 0xAA, 0xAA, 0xFF, 0x06, 0x32, 0x00, 0x00, 0x76, 0x56 };
//command untuk turn Automatic Frequency Hopping Mode on
byte cmd4[] = { 0xAA, 0xAA, 0xFF, 0x06, 0x37, 0x00, 0xFF, 0x83, 0x56 };
//command untuk Set RF Emission Power Capacity 0BB8 = 3000 = 30 dBm
//byte cmd5[] = {0xAA,0xAA,0xFF,0x07,0x3B,0x00,0x0B,0xB8,0xEB,0x5E};
//command untuk Set RF Emission Power Capacity 14 dBm
//byte cmd5[] = { 0xAA, 0xAA, 0xFF, 0x07, 0x3B, 0x00, 0x05, 0x78, 0x11, 0x1D };
//command untuk Set RF Emission Power Capacity 20 dBm
//byte cmd5[] = {0xAA,0xAA,0xFF,0x07,0x3B,0x00,0x07,0xD0,0x43,0x9D};
//command untuk Set RF Emission Power Capacity 30 dBm
byte cmd5[] = {0xAA,0xAA,0xFF,0x07,0x3B,0x00,0x0B,0xB8,0xEB,0x5E};
// command untuk single tag inventory
// byte cmd6[] = {0xAA,0xAA,0xFF,0x05,0xC8,0x00,0x3A,0x5E};
// command untuk set working parameters of the antenna
// jumlah antenna 1, port 1, polling open, power 30 dBm, batas kali bacaan per antenna 10
//byte cmd7[] = {0xAA, 0xAA, 0xFF, 0x0E, 0x3F, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x0B, 0xB8, 0x0A, 0x21, 0xB2};
// jumlah antenna 1, port 1, polling open, power 10 dBm, batas kali bacaan per antenna 10
//byte cmd7[] = {0xAA, 0xAA, 0xFF, 0x0E, 0x3F, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03, 0xE8, 0x0A, 0x86, 0xAC};
// jumlah antenna 1, port 1, polling open, power 14 dBm, batas kali bacaan per antenna 10
//byte cmd7[] = {0xAA, 0xAA, 0xFF, 0x0F, 0x3F, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x05, 0x78, 0x00, 0x0A, 0x09, 0x0C};
// jumlah antenna 1, port 1, polling open, power 20 dBm, batas kali bacaan per antenna 200
//byte cmd7[] = {0xAA, 0xAA, 0xFF, 0x0F, 0x3F, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x07, 0xD0, 0x00, 0xC8, 0x09, 0x57};
// jumlah antenna 1, port 1, polling open, power 30 dBm, batas kali bacaan per antenna 200
byte cmd7[] = { 0xAA, 0xAA, 0xFF, 0x0F, 0x3F, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x0B, 0xB8, 0x00, 0xC8, 0x74, 0xAF };
// command untuk start multiple tags in inventory
// mulai multiple read
byte cmd8[] = { 0xAA, 0xAA, 0xFF, 0x08, 0xC1, 0x00, 0x08, 0x00, 0x00, 0x60, 0x4A };
//menghentikan multiple read
byte cmd9[] = { 0xAA, 0xAA, 0xFF, 0x05, 0xC0, 0x00, 0xB3, 0xF7 };
//reset reader
byte cmd10[] = {0xAA,0xAA,0xFF,0x05,0x0F,0x00,0xB5,0x9D};

void setup() {
  Serial.begin(115200);                         // konfigurasi komunikasi serial untuk ESP32
  Serial2.begin(115200, SERIAL_8N1, rx2, tx2);  // konfigurasi komunikasi serial untuk modul RFID
  //Serial2.write(cmd1, sizeof(cmd1));
  Serial2.write(cmd2, sizeof(cmd2));
  Serial2.write(cmd3, sizeof(cmd3));
  Serial2.write(cmd4, sizeof(cmd4));
  Serial2.write(cmd5, sizeof(cmd5));
  Serial2.write(cmd7, sizeof(cmd7));

  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);

  xTaskCreatePinnedToCore(mainTask, "Task1", 8192, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(ledBlink, "Task2", 2048, NULL, 1, NULL, 0);
  //SETUP WIFI
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  udp.begin(localPort);
}
void mainTask(void* pvParameters) {
  (void)pvParameters;
  while (1) {
    int packetSize = udp.parsePacket();
    if (packetSize) {
      char buffer[packetSize];
      udp.read(buffer, packetSize);
      Serial.println(buffer[0]);
      if (buffer[0] == '1') {
        //stocking
        mode = 1;

        int respByte = 0;
        
        startTime = millis();

        while (millis() - startTime < readTime) {
          Serial2.write(cmd8, sizeof(cmd8));
          while (Serial2.available() > 0) {
            int newTag = 0;
            if (Serial2.find(header, 3)) {
              byte incomingByte = Serial2.read();
              int lenData = int(incomingByte);
              if (lenData == 24) {
                tag++;
                newTag = 1;
                resp[tag][0] = 0xAA;
                resp[tag][1] = 0xAA;
                resp[tag][2] = 0xFF;
                resp[tag][3] = incomingByte;
                respByte = 4;
                for (int i = 0; i < lenData - 1; i++) {
                  resp[tag][respByte] = Serial2.read();
                  respByte++;
                }
                for (int i = 0; i < respByte; i++) {
                  Serial.printf("%02X", resp[tag][i]);
                  Serial.print(" ");
                }
                Serial.println();
              }
            }
            if (tag > 0 && newTag == 1) {
              for (int i = 0; i < tag; i++) {
                if (compareTags(resp[tag], resp[i], 10, 22)) {
                  Serial.println("masuk");
                  tag--;
                  Serial.println(tag);
                  break;
                }
              }
            }
            if (millis() - startTime >= readTime) {
              Serial2.flush();
              break;
            }
          }
          Serial2.flush();
        }
        Serial2.flush();
        Serial2.write(cmd9, sizeof(cmd9));

        //kirim
        for (int i = 0; i <= tag; i++) {
          byte* row = resp[i];
          int lenRespByte = int(resp[i][3]);
          long CRC = resp[i][lenRespByte + 1] << 8 | resp[i][lenRespByte + 2];

          if (Calculate_CRC(row, lenRespByte + 1) == CRC) {
            for (int j = 0; j < lenEPC; j++) {
              readyToSend[tagValid][j] = resp[i][j + 10];
            }
            tagValid++;
          }
        }

        Serial.println("Siap kirim");
        udp.beginPacket(ip, raspiPortStocking);
        // send UDP Packets
        if (tagValid == 0) {
          byte kosong = 0x0;
          udp.write(kosong);
          Serial.println("kosong");
        } else {
          for (int i = 0; i < tagValid; i++) {
            for (int j = 0; j < lenEPC; j++) {
              udp.write(readyToSend[i][j]);
              Serial.printf("%02X", readyToSend[i][j]);
              Serial.print(" ");
            }
            Serial.println();
          }
        }
        udp.endPacket();

        bool isResponded = false;
        while (isResponded == false) {
          int packetRespSize = udp.parsePacket();
          if (packetRespSize) {
            char bufferResp[packetRespSize];
            udp.read(bufferResp, packetRespSize);
            Serial.println(bufferResp);
            if (strcmp(bufferResp, "sukses") == 0) {
              isResponded = true;
              isError = false;
              isStocked = true;
            } else if (strcmp(bufferResp, "notsukses") == 0) {
              isResponded = true;
              isError = true;
              isStocked = false;
            }
            else if (strcmp(bufferResp, "standby") == 0) {
              isResponded = true;
              isStocked = false;
              isError = false;
            }
          }
        }

        Serial.println("data sent");
        //clear all matrix
        memset(resp, 0, sizeof(resp));
        memset(readyToSend, 0, sizeof(readyToSend));
        memset(buffer, 0, sizeof(buffer));
        tagValid = 0;
        tag = -1;
      } else if (buffer[0] == '2') {
        //stock opname
        mode = 2;

        int respByte = 0;
        unsigned long startTime = millis();

        while (millis() - startTime < readTime+8000) {
          Serial2.write(cmd8, sizeof(cmd8));
          while (Serial2.available() > 0) {
            int newTag = 0;
            if (Serial2.find(header, 3)) {
              byte incomingByte = Serial2.read();
              int lenData = int(incomingByte);
              if (lenData == 24) {
                tag++;
                newTag = 1;
                resp[tag][0] = 0xAA;
                resp[tag][1] = 0xAA;
                resp[tag][2] = 0xFF;
                resp[tag][3] = incomingByte;
                respByte = 4;
                for (int i = 0; i < lenData - 1; i++) {
                  resp[tag][respByte] = Serial2.read();
                  respByte++;
                }
                for (int i = 0; i < respByte; i++) {
                  Serial.printf("%02X", resp[tag][i]);
                  Serial.print(" ");
                }
                Serial.println();
              }
            }
            if (tag > 0 && newTag == 1) {
              for (int i = 0; i < tag; i++) {
                if (compareTags(resp[tag], resp[i], 10, 22)) {
                  Serial.println("masuk");
                  tag--;
                  Serial.println(tag);
                  break;
                }
              }
            }
            if (millis() - startTime >= readTime+8000) {
              Serial2.flush();
              break;
            }
          }
          Serial2.flush();
        }
        Serial2.flush();
        //kirim
        resetRFID();

        for (int i = 0; i <= tag; i++) {
          byte* row = resp[i];
          int lenRespByte = int(resp[i][3]);
          long CRC = resp[i][lenRespByte + 1] << 8 | resp[i][lenRespByte + 2];

          if (Calculate_CRC(row, lenRespByte + 1) == CRC) {
            for (int j = 0; j < lenEPC; j++) {
              readyToSend[tagValid][j] = resp[i][j + 10];
            }
            tagValid++;
          }
        }
        Serial.println("Siap kirim");
        udp.beginPacket(ip, raspiPortSO);
        // send UDP Packets
        if (tagValid == 0) {
          byte kosong = 0x0;
          udp.write(kosong);
        } else {
          for (int i = 0; i < tagValid; i++) {
            for (int j = 0; j < lenEPC; j++) {
              udp.write(readyToSend[i][j]);
              Serial.printf("%02X", readyToSend[i][j]);
              Serial.print(" ");
            }
            Serial.println();
          }
        }
        udp.endPacket();

        bool isResponded = false;
        while (isResponded == false) {
          int packetRespSize = udp.parsePacket();
          if (packetRespSize) {
            char bufferResp[packetRespSize];
            udp.read(bufferResp, packetRespSize);
            if (strcmp(bufferResp, "sukses") == 0) {
              isResponded = true;
              isError = false;
            } else if (strcmp(bufferResp, "notsukses") == 0) {
              isResponded = true;
              isError = true;
            }
          }
        }

        Serial.println("data sent");
        //clear all matrix
        memset(resp, 0, sizeof(resp));
        memset(readyToSend, 0, sizeof(readyToSend));
        memset(buffer, 0, sizeof(buffer));
        tagValid = 0;
        tag = -1;
      } else if (buffer[0] == '3') {
        //picking
        mode = 3;
        int respByte = 0;
        unsigned long startTime = millis();

        while (millis() - startTime < readTime) {
          Serial2.write(cmd8, sizeof(cmd8));
          while (Serial2.available() > 0) {
            int newTag = 0;
            if (Serial2.find(header, 3)) {
              byte incomingByte = Serial2.read();
              int lenData = int(incomingByte);
              if (lenData == 24) {
                tag++;
                newTag = 1;
                resp[tag][0] = 0xAA;
                resp[tag][1] = 0xAA;
                resp[tag][2] = 0xFF;
                resp[tag][3] = incomingByte;
                respByte = 4;
                for (int i = 0; i < lenData - 1; i++) {
                  resp[tag][respByte] = Serial2.read();
                  respByte++;
                }
                for (int i = 0; i < respByte; i++) {
                  Serial.printf("%02X", resp[tag][i]);
                  Serial.print(" ");
                }
                Serial.println();
              }
            }
            if (tag > 0 && newTag == 1) {
              for (int i = 0; i < tag; i++) {
                if (compareTags(resp[tag], resp[i], 10, 22)) {
                  Serial.println("masuk");
                  tag--;
                  Serial.println(tag);
                  break;
                }
              }
            }
            if (millis() - startTime >= readTime) {
              Serial2.flush();
              break;
            }
          }
          Serial2.flush();
        }
        Serial2.flush();

        //kirim
        Serial2.write(cmd9, sizeof(cmd9));

        for (int i = 0; i <= tag; i++) {
          byte* row = resp[i];
          int lenRespByte = int(resp[i][3]);
          long CRC = resp[i][lenRespByte + 1] << 8 | resp[i][lenRespByte + 2];

          if (Calculate_CRC(row, lenRespByte + 1) == CRC) {
            for (int j = 0; j < lenEPC; j++) {
              readyToSend[tagValid][j] = resp[i][j + 10];
            }
            tagValid++;
          }
        }
        Serial.println("Siap kirim");
        udp.beginPacket(ip, raspiPortPicking);
        // send UDP Packets
        if (tagValid == 0) {
          byte kosong = 0x0;
          udp.write(kosong);
          Serial.println("kosong");
        } else {
          for (int i = 0; i < tagValid; i++) {
            for (int j = 0; j < lenEPC; j++) {
              udp.write(readyToSend[i][j]);
              Serial.printf("%02X", readyToSend[i][j]);
              Serial.print(" ");
            }
            Serial.println();
          }
        }
        udp.endPacket();

        bool isResponded = false;
        while (isResponded == false) {
          int packetRespSize = udp.parsePacket();
          if (packetRespSize) {
            char bufferResp[packetRespSize];
            udp.read(bufferResp, packetRespSize);
            Serial.println(bufferResp);
            if (strcmp(bufferResp, "sukses") == 0) {
              isResponded = true;
              isError = false;
              isPicked = true;
            } else if (strcmp(bufferResp, "notsukses") == 0) {
              isResponded = true;
              isError = true;
              isPicked = false;
            }
            else if (strcmp(bufferResp, "standby") == 0) {
              isResponded = true;
              isPicked = false;
              isError = false;
            }
            else if (strcmp(bufferResp, "glitch") == 0) {
              isResponded = true;
              isPicked = false;
            }
          }
        }

        Serial.println("data sent");
        //clear all matrix
        memset(resp, 0, sizeof(resp));
        memset(readyToSend, 0, sizeof(readyToSend));
        memset(buffer, 0, sizeof(buffer));
        tagValid = 0;
        tag = -1;
      } 
    }
    else{
      if(mode ==1 && isStocked==false){
        mode = 1;
      }
      else if(mode == 3 && isPicked == false){
        mode = 3;
      }
      else{
      mode = 0;
      }
    }
    vTaskDelay(1);
  }
}

void ledBlink(void* pvParameters) {
  (void)pvParameters;
  while (1) {
    if (mode == 1) {  //mode stocking
      if (isError == false) {
        digitalWrite(redLED, LOW);
        digitalWrite(greenLED, HIGH);
      } else {
        digitalWrite(redLED, LOW);
        digitalWrite(greenLED, LOW);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        digitalWrite(redLED, LOW);
        digitalWrite(greenLED, HIGH);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
    } else if (mode == 2) {  //mode stock opname
      if (isError == false) {
        digitalWrite(redLED, HIGH);
        digitalWrite(greenLED, HIGH);
      } else {
        digitalWrite(redLED, LOW);
        digitalWrite(greenLED, LOW);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        digitalWrite(redLED, HIGH);
        digitalWrite(greenLED, HIGH);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
    } else if (mode == 3) {  //mode picking
      if (isError == false) {
        digitalWrite(redLED, HIGH);
        digitalWrite(greenLED, LOW);
      } else {
        digitalWrite(redLED, LOW);
        digitalWrite(greenLED, LOW);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        digitalWrite(redLED, HIGH);
        digitalWrite(greenLED, LOW);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
    } else {
      digitalWrite(redLED, LOW);
      digitalWrite(greenLED, LOW);
    }
    vTaskDelay(1);
  }
}

unsigned int Calculate_CRC(unsigned char* ptr, unsigned char len) {
  unsigned int xorval;
  unsigned char i, j;
  unsigned int CRCacc = 0xffff;
  for (j = 0; j < len; j++) {
    for (i = 0; i < 8; i++) {
      xorval = ((CRCacc >> 8) ^ (ptr[j] << i)) & 0x0080;
      CRCacc = (CRCacc << 1) & 0xfffe;
      if (xorval)
        CRCacc ^= 0x1021;
    }
  }
  return CRCacc;
}

bool compareTags(byte* tag1, byte* tag2, int start, int end) {
  for (int i = start; i < end; i++) {
    if (tag1[i] != tag2[i]) {
      return false;
    }
  }
  return true;
}

void resetRFID(){
  Serial2.write(cmd10, sizeof(cmd10));
  vTaskDelay(250 / portTICK_PERIOD_MS);
  Serial2.write(cmd2, sizeof(cmd2));
  Serial2.write(cmd3, sizeof(cmd3));
  Serial2.write(cmd4, sizeof(cmd4));
  Serial2.write(cmd5, sizeof(cmd5));
  Serial2.write(cmd7, sizeof(cmd7));
  Serial2.write(cmd9, sizeof(cmd9));
  vTaskDelay(250 / portTICK_PERIOD_MS);
}
void loop() {
}