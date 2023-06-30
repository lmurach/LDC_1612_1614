# Inductance to Digital Converter Library for the LDC1312, LDC1314, LDC1612, and LDC1614

<hr>

## Introduction

This library serves as an alternative to the library created by SEEED that incorporates the ability to use the 4 channel chip and instructions on how to set up a working version without using the Grove demo board. This sheet will explain everything you need to maximize accuracy for your own LDC, how to calculate every parameter required for calculations, a sample PCB design, and how to set up an Arduino program.

## Usage

(will fill in with installation instructions when the library is able to be installed through Arduino)

In order to use the installed library, put the following code up top. Change the LDC1612 value to the name of your LDC chip (LDC1312, LDC1314, LDC1612, and LDC1614 are the 4 accepted types):
```c
#include "LDC_1612_1614.h"
LDC_1612_1614 ldc(LDC1612);
```
Then write ldc. before each function. Function names and purposes can be found at the [bottom of this page](https://github.com/Quillington/LDC_1612_1614/blob/master/README.md#code-functions). See examples for more clarity.

## Setup

### Hardware

#### Grove Board
Choose which channel you want to use (0 or 1 for LDC1612, 0, 1, 2, or 3 for LDC1614). Connect each end to the Ch0 pins or Ch1 pins. Remember which channel you use, its important for the code setup. Plug the Grove connector into the board. Match up the ground, Vcc, SDA, and SCL pins on the Arduino.

#### Only the Chip
Connect a coil and a small capacitor (such as 100 pf) in parallel. Choose which channel you want to use (0 or 1 for LDC1612, 0, 1, 2, or 3 for LDC1614). Connect one end to the INA pin for your channel and the other end to the INB pin. Remember which channel you use, its important for the code setup. A 40MHz crystal oscillator is optimal for the CLKIN, otherwise the internal clock can be used. The library has a function to swap to the internal clock. Connect up the pins to match the pins on the Arduino. The ADDR pin should high. If it is low, then you will need to use the function to change the I2C address. These pins are the same on the LDC13 series.
<p align="center"><img src="https://github.com/Quillington/LDC_1612_1614/assets/66843400/8a9e87b6-197d-4a14-96d9-3ceb7f481c9c" alt="pin diagram"></p>

### Finding Your Coil's Parameters

In order to have accurate calculations, there are 3 required parameters in the configure_channel() function. These parameters are 
1. **Inductance** - This is the amount of inductance in micro Henries (µH) of the coil. Do NOT count parasitic resistance, only the coil's inductance. There are many online calculators that can be used to get an estimate such as [this one](http://www.circuits.dk/calculator_flat_spiral_coil_inductor.htm) for flat coils or [this one](https://www.allaboutcircuits.com/tools/coil-inductance-calculator/) for cylindrical air coils. The default value for the 20 turn Groce inductive coil is 18.147. To get a better estimate, see the section below.
2. **Capacitance** - This is the amount of capacitance put in parallel with the coil inductor in pico Ferads (pF). The default value for the Grove inductive sensor is 100.
3. **Rp** - This is a representation of parasitic resistance and inductance. The details on the specifics can be found [here](https://www.ti.com/lit/an/snaa221b/snaa221b.pdf?ts=1687778558050&ref_url=https%253A%252F%252Fwww.ti.com%252Fproduct%252Fde-de%252FLDC1612). The default value for the grove 20 turn coil is 15.727. Measuring this value for homemade coils can be achieved in several different methods outlined by TI in [this document](https://www.ti.com/lit/an/snoa936/snoa936.pdf?ts=1687797220953&ref_url=https%253A%252F%252Fwww.google.com%252F). The method that only involves an oscilloscope and function generator is outlined below with pictures.

### Measuring Rp and Inductance

Make sure that you have a function generator, variable resistor/potentiometer (one that goes up to 100k is ideal), and 2-channel oscilloscope before you begin. The faster the ocilloscope and function generator, the better. The goal is to find the resonant frequency which is about 3.75MHz for the sample Grove coil. Both the function generator and the ocilloscope needs to be able to handle that frequency, ideally higher. If you do not have a good setup, a higher capacitor instead of the typical 100pF.
1. Setup this circuit
<p align="center">
  <img src="https://github.com/Quillington/LDC_1612_1614/assets/66843400/5b863a93-ff89-4c5c-96d8-009d9eebbcca" alt="diagram to set up for Rp"/>
</p>
<ol start=2>
  <li>Hold metal near your coil at the maximum distance you desire to measure metal.</li>
  <li>Change the frequency of the scope until the input and output channel match phase. The output's amplitude should peak at this point. This is the resonant frequency. Pictured below: Not in resonance (left) and in resonance (right).</li>
<p align="center"><img src="https://github.com/Quillington/LDC_1612_1614/assets/66843400/8a34925a-9ecc-40f7-8a53-7a0f95d5843e" alt="not in resonance" width="400"/><img src="https://github.com/Quillington/LDC_1612_1614/assets/66843400/67159a9f-991d-4831-9e32-ea89a89eae91" alt="in resonance" width="400"/></p>

<ol start=4 type="1">
  <li>At this point the inductance can be calculated with resonant frequency. Use the equation L = (C/(2πf)^2) or <a href="https://www.omnicalculator.com/physics/resonant-frequency-lc">this calculator</a>. With the values of C=100pf and f= 3.68MHz, the calculated value for L is 18.7, which is very close to the expected L of 18.1.</li>
  <li>Now, adjust the resistor so that the output is 4x smaller than the input. 2x can also be used for less current consumption, but 4x is the recommeneded amount.</li>
</ol>

### Making sure the Parameters are in the Measurable Range

The chip has limitations on what it can measure. The frequency and Rp must be within a certain range.
<ol type="1">
  <li> **Frequency must be above 1kHz** - If it is not, then then the capacitance should be decreased.</li>
  <li> **Frequency must be under 10MHz** - If it is not, then the capacitance should be increased.</li>
  <li> **Rp must be under 90k** - If it is not, then a small (100 ohm or so) resistor must be added in parallel.</li>
</ol>

Note that changing either of these two values requires redoing both of the measurements.

## Code Functions
```
configure_channel(uint8_t channel, float inductance, float capacitance, float Rp);
```
This function must be used in the initial setup before any information can be pulled out of the chip. Use the values found earlier. It returns void. (No value)
```
get_channel_data(uint8_t channel);
```
This function returns a 32 bit unsigned value that changes based on inductance. The change in this value represents an increase or decrease in inductance. See the demo Arduino files for possible applications.
```
change_I2C_address();
```
The I2C address is 0x2B by default. If the LDC ADDR pin is held low, it becomes 0x2A. Do NOT use this function if using a Grove board, as the ADDR pin is always held high.
```change_I2C_address(0)```
changes the address to 0x2A
```change_I2C_address(1)```
changes the address to 0x2B (default)
```
change_clock_mode(bool mode);
```
This function allows the use of the internal clock. Note: this should only be used if an external clock is not present or permitted in the design. Use the following values:
```change_clock_mode(INTERNAL)``` 
changes to the internal clock.
```change_clock_mode(EXTERNAL)``` 
changes to the external clock. (default)
