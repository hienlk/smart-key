#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define PRESENT_KEY_SIZE 10
#define PRESENT_BLOCK_SIZE 8
#define PRESENT_ROUNDS 31

int key_id_B = 2020703220207033;
RF24 radio(9, 10); // CE, CSN
const int button1 = 2;
const int button2 = 3;  // led đỏ
const int button3 = 4;  // Led vàng
const int button4 = 5;  // Led xanh
const byte addresses[][6] = {"1Node", "2Node"};
uint8_t keyB[PRESENT_KEY_SIZE] = {0};
uint8_t blockB[PRESENT_BLOCK_SIZE] = {0};

bool authenticated = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
int lastButtonState[4] = {HIGH, HIGH, HIGH, HIGH};
int buttonState[4] = {HIGH, HIGH, HIGH, HIGH};

static const uint8_t SBox[16] = {
    0xC, 0x5, 0x6, 0xB, 0x9, 0x0, 0xA, 0xD, 0x3, 0xE, 0xF, 0x8, 0x4, 0x7, 0x1, 0x2
};

static const uint8_t PBox[64] = {
    0, 16, 32, 48, 1, 17, 33, 49, 2, 18, 34, 50, 3, 19, 35, 51,
    4, 20, 36, 52, 5, 21, 37, 53, 6, 22, 38, 54, 7, 23, 39, 55,
    8, 24, 40, 56, 9, 25, 41, 57, 10, 26, 42, 58, 11, 27, 43, 59,
    12, 28, 44, 60, 13, 29, 45, 61, 14, 30, 46, 62, 15, 31, 47, 63
};

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ;
  }
  
  if (!radio.begin()) {
    Serial.println("NRF24L01 initialization failed!");
    while (1);
  }
  radio.openWritingPipe(addresses[1]); // 00002
  radio.openReadingPipe(1, addresses[0]); // 00001
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);  
  radio.stopListening();
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);
  pinMode(button4, INPUT_PULLUP);
  
  Serial.println("Day la thiet bi B");
}

void printHex(uint8_t* data, int length) {
  for (int i = 0; i < length; i++) {
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void processReceivedData(uint8_t* keynhan) {
  Serial.print("Goi tin tu A: ");
  printHex(keynhan, 18);

  for (int i = 0; i < PRESENT_KEY_SIZE; i++) {
    keyB[i] = keynhan[i];
  }

  for (int i = 0; i < PRESENT_BLOCK_SIZE; i++) {
    blockB[i] = keynhan[i + PRESENT_KEY_SIZE];
  }

  Serial.print("keyB: ");
  printHex(keyB, PRESENT_KEY_SIZE);

  Serial.print("blockB: ");
  printHex(blockB, PRESENT_BLOCK_SIZE);
}

void present_encrypt(uint8_t *block, uint8_t *key) {
    uint64_t state = 0;
    uint64_t round_key[PRESENT_ROUNDS + 1];

    for (int i = 0; i < PRESENT_BLOCK_SIZE; ++i) {
        state |= ((uint64_t)block[i]) << (8 * (7 - i));
    }

    for (int i = 0; i <= PRESENT_ROUNDS; ++i) {
        round_key[i] = 0;
        for (int j = 0; j < PRESENT_KEY_SIZE; ++j) {
            round_key[i] |= ((uint64_t)key[j]) << (8 * (9 - j));
        }

        for (int j = 0; j < 8; ++j) {
            key[j] = key[j + 1];
        }
        key[8] = key[9];
        key[9] = key[1] ^ i;
    }

    for (int i = 0; i < PRESENT_ROUNDS; ++i) {
        state ^= round_key[i];

        uint64_t new_state = 0;
        for (int j = 0; j < 16; ++j) {
            new_state |= ((uint64_t)SBox[(state >> (4 * j)) & 0xF]) << (4 * j);
        }
        state = new_state;

        new_state = 0;
        for (int j = 0; j < 64; ++j) {
            new_state |= ((state >> j) & 0x1) << PBox[j];
        }
        state = new_state;
    }

    state ^= round_key[PRESENT_ROUNDS];

    for (int i = 0; i < PRESENT_BLOCK_SIZE; ++i) {
        block[i] = (state >> (8 * (7 - i))) & 0xFF;
    }
}

void check_buttons() {
  for (int i = 0; i < 4; i++) {
    int reading = digitalRead(button1 + i);
    if (reading != lastButtonState[i]) {
      lastDebounceTime = millis();
    }
    
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (reading != buttonState[i]) {
        buttonState[i] = reading;
        if (buttonState[i] == LOW) {
          if (i == 0) {
            button1_press();
          } else if (authenticated) {
            send_led_command("RYG"[i-1]);
          }
        }
      }
    }
    lastButtonState[i] = reading;
  }
}

void button1_press() {
  // Send key_id_B
  radio.stopListening();
  bool tx_ok = radio.write(&key_id_B, sizeof(key_id_B));
  Serial.println("Gui key id ");

  if (!tx_ok) {
    delay(100);
    tx_ok = radio.write(&key_id_B, sizeof(key_id_B));
  }
  
  radio.startListening();
  
  // Wait for response from receiver
  unsigned long startTime = millis();
  bool receivedResponse = false;
  while (millis() - startTime < 5000) {
    if (radio.available()) {
      char response[32];
      radio.read(&response, sizeof(response));
      Serial.print("Trang thai: ");
      Serial.println(response);
      receivedResponse = true;
      break;
    }
  }
  
  if (!receivedResponse) {
    Serial.println("K phan hoi");
    radio.stopListening();
    return;
  }
  
  // Wait to receive data from receiver
  startTime = millis();
  while (millis() - startTime < 5000) {
    if (radio.available()) {
      uint8_t keynhan[18];
      radio.read(&keynhan, sizeof(keynhan));
      processReceivedData(keynhan);
      
      // Encrypt blockB
      present_encrypt(blockB, keyB);
      
      Serial.print("Ma hoa blockB: ");
      printHex(blockB, PRESENT_BLOCK_SIZE);
      
      // Send encrypted blockB
      radio.stopListening();
      tx_ok = radio.write(&blockB, sizeof(blockB));
      Serial.print("Gui blockB: ");
      Serial.println(!tx_ok ? "OK" : "Fail");
      radio.startListening();
      
      // Wait for confirmation from receiver
      startTime = millis();
      while (millis() - startTime < 5000) {
        if (radio.available()) {
          char confirmation[32];
          radio.read(&confirmation, sizeof(confirmation));
          Serial.print("Trang thai: ");
          Serial.println(confirmation);
          if (strcmp(confirmation, "Xac thuc thanh cong") == 0) {
            authenticated = true;
            Serial.println("Xac thuc thanh cong. Dung nut 2 3 4 de mo LED");
          }
          radio.stopListening();
          return;
        }
      }
      Serial.println("K co thong tin tra ve");
      break;
    }
  }
  Serial.println("Timeout");
  radio.stopListening();
}

void send_led_command(char command) {
  radio.stopListening();
  bool tx_ok = radio.write(&command, sizeof(command));
  Serial.print("Mo LED  ");
  Serial.print(command);
  Serial.println(!tx_ok ? ": OK" : ": Failed");
  radio.startListening();
}

void loop() {
  check_buttons();
}