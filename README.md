# 🤖 RoboEyes: Smooth OLED Expressions for Arduino & ESP32

![Project Status](https://img.shields.io/badge/Status-Active-green) ![Platform](https://img.shields.io/badge/Platform-Arduino%20|%20ESP32-blue) ![License](https://img.shields.io/badge/License-MIT-yellow)

**Bring your robot to life with realistic, non-blocking eye animations!**

This project provides a highly optimized, easy-to-use system for drawing animated eyes on an SSD1306 OLED display. Unlike traditional code that pauses your robot to blink, **RoboEyes is non-blocking**. This means your robot can sense the world, move motors, and process data *while* looking around!

## ✨ Features

* **🚫 Non-Blocking:** No `delay()` used! Your main loop stays fast for sensors.
* **🎭 15 Expressions:** Happy, Sad, Angry, Confused, Sleepy, Scared, and more.
* **🌊 Smooth Transitions:** Uses math (Linear Interpolation) for fluid, natural movement.
* **👶 Beginner Friendly:** Change expressions with one line of code.
* **⚡ Optimized:** Fits easily on Arduino Uno, Nano, and ESP32.
* **💤 Auto-Blink:** Eyes blink naturally on their own if no command is sent.

---

## 🛠️ Hardware Required

1.  **Microcontroller:** Arduino Uno, Nano, Mega, or ESP32.
2.  **Display:** 0.96" or 1.3" OLED Display (I2C Interface).
3.  **Jumper Wires:** 4 wires (VCC, GND, SDA, SCL).

### 🔌 Wiring Guide (I2C)

| Pin on OLED | Arduino Uno/Nano | ESP32 |
| :--- | :--- | :--- |
| **VCC** | 5V | 3.3V (or 5V) |
| **GND** | GND | GND |
| **SCL** | A5 | D22 (GPIO 22) |
| **SDA** | A4 | D21 (GPIO 21) |

---

## 🚀 How to Install

1.  **Download the Code:** Copy the `.ino` file from this repository.
2.  **Open Arduino IDE:** Paste the code into a new sketch.
3.  **Install Libraries:**
    * Go to `Sketch` -> `Include Library` -> `Manage Libraries...`
    * Search for **Adafruit SSD1306** and install it.
    * Search for **Adafruit GFX** and install it.
4.  **Upload:** Select your board and port, then click Upload.

---

## 🎮 Try it Online (Wokwi Simulation)

Don't have hardware yet? Test the code in your browser!

1.  Go to [Wokwi.com](https://wokwi.com/projects/new/arduino-uno).
2.  Click the purple **"+"** button and add an **SSD1306** display.
3.  Connect **VCC** to 5V, **GND** to GND, **SDA** to A4, **SCL** to A5.
4.  Paste the `RoboEyes` code into the code editor.
5.  Click the **Green Play Button** to see it in action!

---

## 👨‍💻 How to Use (Code Examples)

### 1. The "Magic" Command
To change the robot's face, use `setExpression` inside your code:

```cpp
// Make the robot happy
setExpression(HAPPY);

// Make the robot angry
setExpression(ANGRY);

// Go back to normal
setExpression(IDLE);
