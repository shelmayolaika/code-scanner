#include <WiFi.h>
#include <WiFiUdp.h>

//pin komunikasi Serial2 untuk komunikasi dengan modul RFID
#define rx2 16
#define tx2 17

//VARIABEL
const int maxTag = 100;
const int maxRespByte = 30;
const int lenEPC = 12;
byte resp[maxTag][maxRespByte];
byte readyToSend[maxTag][lenEPC];
byte header[] = { 0xAA, 0xAA, 0xFF };
int tagValid = 0;
//pin tombol dan LED
int button = 13;
int redLED = 25;
int greenLED = 32;
//mode == 0 berarti mode baca dan
//mode==1 berarti kirim data ke Raspberry Pi
int mode = 0;
bool isError = false;
bool isSent = false;
int tag = -1;

// NETWORK
const char* ssid = "octagon";
const char* password = "raspberrypi";
IPAddress ip(192, 168, 4, 1);  //IP address raspi
const int raspiPort = 49152;
const int localPort = 1238;
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


//button interrupt untuk ganti mode
void IRAM_ATTR isr() {
  //debouncing
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();

  if (interrupt_time - last_interrupt_time > 300) {
    if (mode == 0) {
      mode = 1;
    } else if (mode == 2 && isError == true) {
      mode = 0;
      isError = false;
    } else{
      mode = 0;
      isError = false;
    }
  }
  last_interrupt_time = interrupt_time;
}

void setup() {
  // konfigurasi komunikasi serial dari ESP32 ke Serial Monitor
  Serial.begin(115200);
  // konfigurasi komunikasi serial antara ESP32 dengan modul RFID
  Serial2.begin(115200, SERIAL_8N1, rx2, tx2);
  Serial2.write(cmd1, sizeof(cmd1));
  Serial2.write(cmd2, sizeof(cmd2));
  Serial2.write(cmd3, sizeof(cmd3));
  Serial2.write(cmd4, sizeof(cmd4));
  Serial2.write(cmd5, sizeof(cmd5));
  Serial2.write(cmd7, sizeof(cmd7));

  //setup interupsi tombol untuk ganti mode
  pinMode(button, INPUT_PULLUP);
  attachInterrupt(button, isr, FALLING);

  //setup pin LED sebagai output
  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);

  xTaskCreatePinnedToCore(mainTask, "Task1", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(ledBlink, "Task2", 2048, NULL, 1, NULL, 0);

  //setup WiFi
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
    if (mode == 0) {  //mode baca


      //baca multiple tag secara terus menerus
      int respByte = 0;
      Serial2.write(cmd8, sizeof(cmd8));
      // if (Serial2.available() <= 0) {
      //   loop();
      //}
      // else {
        while (Serial2.available() > 0 && mode == 0) {
          int newTag = 0;
          //mencari respons modul RFID yang diawali dengan 0xAA, 0xAA, 0xFF
          if (Serial2.find(header, 3)) {
            byte incomingByte = Serial2.read();
            int lenData = int(incomingByte);
            //tag yang berhasil terbaca dengan sempurna memiliki panjang data 24 byte
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
                //Serial.println("masuk");
                tag--;
                //Serial.println(tag);
                break;
              }
            }
          }
        }
      //}
    }


    else if (mode == 1) {  //mode kirim
      if (isSent == false) {
        //kirim perintah untuk stop baca tag
        Serial2.write(cmd9, sizeof(cmd9));

        //cek CRC
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

        // kirim paket UDP
        udp.beginPacket(ip, raspiPort);
        for (int i = 0; i < tagValid; i++) {
          for (int j = 0; j < lenEPC; j++) {
            udp.write(readyToSend[i][j]);
            Serial.printf("%02X", readyToSend[i][j]);
            Serial.print(" ");
          }
          Serial.println();
        }
        udp.endPacket();
        Serial.println("data sent");
        isSent = true;
      } else if (isSent = true) {
        int packetSize = udp.parsePacket();
        if (packetSize) {
          char buffer[packetSize];
          udp.read(buffer, packetSize);
          if (strcmp(buffer, "sukses") == 0) {
            mode = 0;
            isSent = false;
          } else {
            isError = true;
            mode = 2;
            isSent = false;
          }
          //clear semua matrix
          memset(resp, 0, sizeof(resp));
          memset(readyToSend, 0, sizeof(readyToSend));
          tagValid = 0;
          tag = -1;
        }
      }
    }
    vTaskDelay(1);
  }
}

void ledBlink(void* pvParameters) {
  (void)pvParameters;
  while (1) {
    if (isError == true && mode ==2) {
      digitalWrite(redLED, LOW);
      digitalWrite(greenLED, LOW);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      digitalWrite(redLED, HIGH);
      digitalWrite(greenLED, LOW);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    else if(isError == false && mode == 0){
      //menyalakan LED merah sebagai tanda mode baca
      digitalWrite(redLED, LOW);
      digitalWrite(greenLED, HIGH);
    }
    else if(isError == false && mode == 1){
        //menyalakan LED hijau sebagai tanda siap kirim hasil scan dan boleh tekan tombol kirim
        digitalWrite(redLED, HIGH);
        digitalWrite(greenLED, LOW);
    }
    vTaskDelay(1);
  }
}


//fungsi menghitung CRC
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

//fungsi untuk mengecek duplikasi tag
bool compareTags(byte* tag1, byte* tag2, int start, int end) {
  for (int i = start; i < end; i++) {
    if (tag1[i] != tag2[i]) {
      return false;
    }
  }
  return true;
}

void loop() {
}
