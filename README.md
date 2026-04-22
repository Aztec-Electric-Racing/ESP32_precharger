# Precharger-Board
### Setup
- Install PlatformIO on VS Code
- Clone repository
- Open folder through PlatformIO
- BUILD

or 

- Install PlatformIO through terminal
- Clone repository
```bash
pio run
```
### Configuration
```ini
platform = espressif32
board = esp32-s2-saola-1
framework = espidf
monitor_speed = 115200
```

### Pinouts (as shown in image below)
```c++
#define SDC 17                      //Shutdown circuit end: 1 = open, 0 = closed (Input), SHORTED pin 17 & 18 on board
#define OPTO_ISOLATOR_OUT 4         //Reads signal from HV comparator (Input)
#define HV_UNSWITCHED_RELAY 5       //Relay for turning on ground reference for high voltage (Output)
#define CAPACITOR_CHARGE_RELAY 7    //Relay to start charging capacitor (Output)
#define PRECHARGE_RELAY 6           //Bridges resistor across positive AIR (Output)
#define HV_CONTACTOR_RELAY 8        //Turns on positive AIR (Output)
```
<img width="617" alt="image" src="https://github.com/user-attachments/assets/34f5a331-8648-448b-8cbd-8d2e4d6aa96a" />

### Overall Dataflow Diagram 
<img width="976" alt="image" src="https://github.com/user-attachments/assets/c2400043-45c1-44cf-bb26-94dcd0c134dd" />


---
### Helpful Links
- [ESP32-S2Saola Data Sheet](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s2/hw-reference/esp32s2/user-guide-saola-1-v1.2.html)
- [Official ESP-IDF GPIO](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html)
