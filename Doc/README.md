# stm32-envsense-car — STM32 IoT Smart Environmental Monitoring Car

A feature-rich embedded project based on STM32F103ZE, integrating environmental sensing, WiFi connectivity, and intelligent vehicle control.

## Features

- **Environmental Monitoring** — DHT11 temperature & humidity sensor, HC-SR04 ultrasonic ranging
- **WiFi Connectivity** — ESP8266 module with MQTT protocol for cloud data reporting
- **Intelligent Control** — Obstacle avoidance, line following, servo spatial scanning
- **User Interface** — OLED display with menu system, snake game, WiFi config interface
- **Debug & Diagnostics** — Real-time debug output, sensor data visualization

## Project Structure

```
STM32-EnvSense/
├── Libraries/                          # CMSIS + StdPeriph Library
│   ├── CMSIS/                          # Cortex-M3 core + STM32F10x device support
│   └── STM32F10x_StdPeriph_Driver/     # Standard peripheral driver (inc/src)
├── Hardware/                           # Hardware abstraction layer
│   ├── Inc/                            # Header files
│   │   ├── motor.h, oled.h, dht11.h, sr04.h
│   │   ├── key.h, led.h, beep.h, delay.h
│   │   ├── avoid.h, follow.h, spatial.h
│   │   ├── servo.h, usart.h, bmp.h
│   └── Src/                            # Source files
│       ├── motor.c, oled.c, dht11.c, sr04.c
│       ├── key.c, led.c, beep.c, delay.c
│       ├── avoid.c, follow.c, spatial.c
│       ├── servo.c, usart.c
├── Application/                        # Application layer
│   ├── Inc/                            # Header files
│   │   ├── menu.h, ui.h, snake.h, debug.h
│   │   ├── wificfg.h, creeper.h, oledfont.h
│   │   ├── ui_res.h, ui_wifi.h
│   └── Src/                            # Source files
│       ├── menu.c, ui.c, snake.c, debug.c
│       └── wificfg.c
├── Communication/                      # Communication module
│   ├── esp8266.c / esp8266.h           # WiFi AT command driver
│   └── esp8266_mqtt.c / .h             # MQTT protocol implementation
├── System/                             # System layer
│   └── sys.h / sys.c                   # System clock & interrupt config
├── User/                               # Entry point
│   ├── main.c                          # Main program entry
│   ├── stm32f10x_it.c / .h             # Interrupt handlers
│   └── stm32f10x_conf.h                # Peripheral config
├── Project/                            # Keil MDK project files
│   ├── STM32-EnvSense.uvprojx           # Project file
│   └── STM32-EnvSense.uvoptx            # Options file
├── Assets/                             # UI resource assets
│   └── wifi/                            # WiFi animation bitmaps
├── Build/                              # Build output
│   ├── Output/                          # Compiled binary
│   └── Listing/                         # Listing files
└── Doc/                                # Documentation
    └── README.md
```

## Hardware Requirements

- MCU: STM32F103ZE (Cortex-M3, High Density)
- Sensors: DHT11, HC-SR04 × 3
- Motor: L298N / L9110S driver
- Display: 0.96" OLED (I2C)
- WiFi: ESP8266-01S
- Servo: SG90

## Development Environment

- IDE: Keil MDK-ARM v5
- Toolchain: ARM Compiler v5/6
- Debug: J-Link / ST-Link

## Build & Flash

1. Open `Project/STM32-EnvSense.uvprojx` with Keil MDK
2. Build (F7)
3. Flash (F8) via ST-Link

## MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `sensor/temp` | Upload | Temperature data |
| `sensor/humi` | Upload | Humidity data |
| `sensor/dist` | Upload | Distance data |
| `car/control` | Download | Remote control commands |
