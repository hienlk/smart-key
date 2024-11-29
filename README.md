# Smart key using symmetric encryption algorithm and hashing algorithm 



This project utilizes the PRESENT encryption algorithm to create a secure encryption block and incorporates the SHA3-256 hashing algorithm to divide input data into fixed-size blocks. The devices exchange data packets multiple times during the authentication process, ensuring secure and reliable communication between them.

The encryption keys and devices communicate with each other via radio waves, offering flexibility and efficiency in data transmission. Additionally, the system uses LEDs to represent different functions, which can be replaced with specific functionalities as required by the application.

The project also supports the scalability of encryption keys. By simply adding a key ID to the receiver, users can integrate additional encryption keys, enhancing the system's adaptability and expanding its use cases.



## Hardware Components Used:
<p align="center">
<img src="https://github.com/hienlk/smart-key/blob/main/res/key.png" height="500" width="650">
<br>
Key Fob
<br>
<img src="https://github.com/hienlk/smart-key/blob/main/res/rec.png" height="500" width="650">
<br>
Receive Device
<br>
</p>
<br>

- **Arduino Uno R3 or Arduino mini** 

- **NRFL01+** 

- **Button** 

- **LED** 

| NRFL01+     | Arduino |   
| ---      | ---       |
| GND |GND |
| VIN     | 3.3V       |
| CE |D9 |
| CSN     | D10     |
| SCK |D13 |
| MOSI     | D11       |
| MISO |D12 |
| IRQ     |        |
<br>

| Button     | Arduino |   
| ---      | ---       |
| GND |GND |
| B1     | D2    |
| B2 |D3 |
| B3     | D4     |
| B4 |D5 |

<br>

| LED     | Arduino |   
| ---      | ---       |
| GND |GND |
| RED     | D2    |
| YELLOW |D3 |
| GREEN     | D4     |

<br>

## Workflow
<p align="center">
<img src="https://github.com/hienlk/smart-key/blob/main/res/work_flow.png" height="700" width="400">
</p>
<br>

## Result
<img src="https://github.com/hienlk/smart-key/blob/main/res/result.gif" width="700">
<br><br>

## Getting Started


1. Assemble the hardware components according to the schematic diagram provided.

2. Upload the provided firmware to the Arduino using Arduino IDE.

3. Change LEDs to any function which you want or add more key fobs.


