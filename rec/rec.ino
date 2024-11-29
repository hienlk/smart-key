#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define PRESENT_KEY_SIZE 10
#define PRESENT_BLOCK_SIZE 8
#define PRESENT_ROUNDS 31
#define KECCAKF_ROUNDS 24
#define SHA3_256_RATE 136

RF24 radio(9, 10); // CE, CSN
const byte addresses[][6] = {"1Node", "2Node"};
const int greenLedPin = 4;
const int redLedPin = 5;    
const int yellowLedPin = 6; 
uint8_t keyA[PRESENT_KEY_SIZE] = {0};
uint8_t blockA[PRESENT_BLOCK_SIZE] = {0};
uint8_t blockB[PRESENT_BLOCK_SIZE] = {0};
char index[256];
uint8_t selected_values[18];
const int key_id = 2020703220207033;

bool authenticated = false;
unsigned long lastAuthTime = 0;
const unsigned long authInterval = 30000; 

static const uint8_t SBox[16] = {
    0xC, 0x5, 0x6, 0xB, 0x9, 0x0, 0xA, 0xD, 0x3, 0xE, 0xF, 0x8, 0x4, 0x7, 0x1, 0x2
};

static const uint8_t PBox[64] = {
    0, 16, 32, 48, 1, 17, 33, 49, 2, 18, 34, 50, 3, 19, 35, 51,
    4, 20, 36, 52, 5, 21, 37, 53, 6, 22, 38, 54, 7, 23, 39, 55,
    8, 24, 40, 56, 9, 25, 41, 57, 10, 26, 42, 58, 11, 27, 43, 59,
    12, 28, 44, 60, 13, 29, 45, 61, 14, 30, 46, 62, 15, 31, 47, 63
};

static const uint64_t keccakf_rndc[24] = {
    0x0000000000000001, 0x0000000000008082, 0x800000000000808A,
    0x8000000080008000, 0x000000000000808B, 0x0000000080000001,
    0x8000000080008081, 0x8000000000008009, 0x000000000000008A,
    0x0000000000000088, 0x0000000080008009, 0x000000008000000A,
    0x000000008000808B, 0x800000000000008B, 0x8000000000008089,
    0x8000000000008003, 0x8000000000008002, 0x8000000000000080,
    0x000000000000800A, 0x800000008000000A, 0x8000000080008081,
    0x8000000000008080, 0x0000000080000001, 0x8000000080008008
};

static const int keccakf_rotc[24] = {
    1,  3,  6, 10, 15, 21, 28, 36, 45, 55, 2, 14,
   27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44
};

static const int keccakf_piln[24] = {
    10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1
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
  radio.openWritingPipe(addresses[0]); 
  radio.openReadingPipe(1, addresses[1]); 
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);  
  radio.startListening();
  pinMode(greenLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(yellowLedPin, OUTPUT);
  digitalWrite(greenLedPin, LOW);
  digitalWrite(redLedPin, LOW);
  digitalWrite(yellowLedPin, LOW);
  
  Serial.println("A");
}

void printHex(uint8_t* data, int length) {
  for (int i = 0; i < length; i++) {
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void keccakf(uint64_t st[25]) {
    int round, i, j, k;
    uint64_t t, bc[5];

    for (round = 0; round < KECCAKF_ROUNDS; round++) {
        // Theta
        for (i = 0; i < 5; i++) {
            bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
        }
        for (i = 0; i < 5; i++) {
            t = bc[(i + 4) % 5] ^ ((bc[(i + 1) % 5] << 1) | (bc[(i + 1) % 5] >> (64 - 1)));
            for (j = 0; j < 25; j += 5) {
                st[j + i] ^= t;
            }
        }
        // Rho Pi
        t = st[1];
        for (i = 0; i < 24; i++) {
            j = keccakf_piln[i];
            bc[0] = st[j];
            st[j] = (t << keccakf_rotc[i]) | (t >> (64 - keccakf_rotc[i]));
            t = bc[0];
        }
        // Chi
        for (j = 0; j < 25; j += 5) {
            for (i = 0; i < 5; i++) {
                bc[i] = st[j + i];
            }
            for (i = 0; i < 5; i++) {
                st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
            }
        }
        // Iota
        st[0] ^= keccakf_rndc[round];
    }
}

void sha3_256(uint8_t *output, const uint8_t *input, size_t input_len) {
    uint64_t st[25];
    uint8_t temp[SHA3_256_RATE];
    size_t i, rsiz, rsizw;

    memset(st, 0, sizeof(st));
    rsiz = SHA3_256_RATE;
    rsizw = rsiz / 8;

    for ( ; input_len >= rsiz; input_len -= rsiz, input += rsiz) {
        for (i = 0; i < rsizw; i++) {
            st[i] ^= ((uint64_t *) input)[i];
        }
        keccakf(st);
    }

    memcpy(temp, input, input_len);
    temp[input_len++] = 0x06;
    memset(temp + input_len, 0, rsiz - input_len);
    temp[rsiz - 1] |= 0x80;

    for (i = 0; i < rsizw; i++) {
        st[i] ^= ((uint64_t *) temp)[i];
    }

    keccakf(st);

    memcpy(output, st, 32);
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

void select_random_values() {
  uint8_t hash[32];
  sha3_256(hash, (uint8_t*)index, 256);
  
  for (int i = 0; i < 18; i++) {
    selected_values[i] = hash[i % 32];
  }
  
  memcpy(keyA, selected_values, 10);
  
  memcpy(blockA, selected_values + 10, 8);
}

void generate_random_index() {
  uint8_t initial_seed[32];
  uint8_t hash[32];
  
  for (int i = 0; i < 32; i++) {
    initial_seed[i] = random(256);
  }
  
  for (int i = 0; i < 8; i++) {
    sha3_256(hash, initial_seed, 32);
    memcpy(index + (i * 32), hash, 32);
    
    memcpy(initial_seed, hash, 32);
  }
}

void send_selected_values() {
  radio.write(selected_values, sizeof(selected_values));
}

bool compare_blocks() {
  return memcmp(blockA, blockB, PRESENT_BLOCK_SIZE) == 0;
}

void handle_led_command() {
  if (radio.available()) {
    char command;
    radio.read(&command, sizeof(command));
    Serial.print("LED: ");
    Serial.println(command);
    
    switch(command) {
      case 'R':
        digitalWrite(redLedPin, HIGH);
        digitalWrite(yellowLedPin, LOW);
        digitalWrite(greenLedPin, LOW);
        delay(1000);
        digitalWrite(redLedPin, LOW);
        break;
      case 'Y':
        digitalWrite(redLedPin, LOW);
        digitalWrite(yellowLedPin, HIGH);
        digitalWrite(greenLedPin, LOW);
        delay(1000);
        digitalWrite(yellowLedPin, LOW);
        break;
      case 'G':
        digitalWrite(redLedPin, LOW);
        digitalWrite(yellowLedPin, LOW);
        digitalWrite(greenLedPin, HIGH);
        delay(1000);
        digitalWrite(greenLedPin, LOW);
        break;
      default:
        Serial.println("*");
    }
  }
}

void loop() {
  unsigned long currentTime = millis();
  
  if (!authenticated || (currentTime - lastAuthTime >= authInterval)) {
    authenticated = false; 
    
    if (radio.available()) {
      int receivedKeyId;
      radio.read(&receivedKeyId, sizeof(receivedKeyId));

      if (receivedKeyId == key_id) {
        radio.stopListening();
        const char* response = "Da nhan key id";
        bool tx_ok = radio.write(response, strlen(response) + 1);
        
        if (!tx_ok) {
          delay(100);
          tx_ok = radio.write(response, strlen(response) + 1);
        }
        
        delay(100);
        radio.startListening();
        
        generate_random_index();
        select_random_values();
        
        Serial.print("So da chon: ");
        printHex(selected_values, 18);
        
        radio.stopListening();
        tx_ok = radio.write(&selected_values, sizeof(selected_values));
        Serial.print("Trang thai gui: ");
        Serial.println(tx_ok ? "OK" : "Failed");
        
        delay(100);
        radio.startListening();
        
        present_encrypt(blockA, keyA);
        
        Serial.print("Ma hoa blockA: ");
        printHex(blockA, PRESENT_BLOCK_SIZE);
        
        unsigned long startTime = millis();
        while (!radio.available()) {
          if (millis() - startTime > 5000) {
            Serial.println("Timeout");
            return;
          }
        }
        
        if (radio.available()) {
          radio.read(&blockB, sizeof(blockB));
          Serial.print("Nhan dc blockB da ma hoa: ");
          printHex(blockB, PRESENT_BLOCK_SIZE);
          
          if (compare_blocks()) {
            Serial.println("2 block giong nhau");
            radio.stopListening();
            const char* confirmation = "Xac thuc thanh cong";

tx_ok = radio.write(confirmation, strlen(confirmation) + 1);
            Serial.print("Trang thai: ");
            Serial.println(tx_ok ? "OK" : "Failed");
            digitalWrite(redLedPin, HIGH);
            digitalWrite(yellowLedPin, HIGH);
            digitalWrite(greenLedPin, HIGH);
            delay(500);
            digitalWrite(redLedPin, LOW);
            digitalWrite(yellowLedPin, LOW);
            digitalWrite(greenLedPin, LOW);
            delay(100);
            radio.startListening();
            
            authenticated = true;
            lastAuthTime = currentTime; 
            Serial.println("Doi lenh mo LED");
          } else {
            Serial.println("Xac thuc that bai");
          }
        }
      } else {
        Serial.println("Sai keyid");
      }
    }
  } else {
    handle_led_command();
  }
  
  if (authenticated && (currentTime - lastAuthTime >= authInterval)) {
    authenticated = false;
  }
}