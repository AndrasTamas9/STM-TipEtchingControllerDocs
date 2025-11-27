
<p align="center">
  <img src="docs/hun-ren-ek-logo.png" alt="HUN-REN EK logo" width="300"/>
</p>

# STM Tip Etching Controller  
**Semi-automated electrochemical etching system for high-quality Pt/Ir STM tips**  
*Developed at the Nanostructures Department,  
HUN-REN Centre for Energy Research (EK),  
Institute of Technical Physics and Materials Science (MFA)*

---

## 📘 Overview

This repository contains the firmware and documentation of the **STM Tip Etching Controller**,  
a custom-designed microcontroller-based system created to automate the electrochemical etching  
of **platinum–iridium STM tips** using a **1 molar CaCl₂ aqueous electrolyte**.

The system provides:

- Highly reproducible tip geometry  
- Automated meniscus detection via real-time RMS current monitoring  
- Multi-stage etching protocol (contact → validation → etching → retract)  
- Dual voltage switching (9 V / 30 V) through relays  
- Precision z-axis motion controlled by a TMC2209 driver  
- On-device parameter editor via LCD keypad  
- Manual jogging mode  
- Full non-blocking firmware architecture  

---

## 🧪 Purpose of the Device

Scanning tunneling microscopy requires extremely sharp, clean, and reproducible metal tips.  
This controller automates the most sensitive steps of tip fabrication, reducing human error  
and guaranteeing consistent quality suitable for atomic-resolution STM experiments.

---

## 📄 Online Documentation

The full API documentation, class descriptions, diagrams, and firmware details are available here:

👉 **https://andrastamas9.github.io/STM-TipEtchingControllerDocs/**

---

## 🛠 Firmware Modules (Summary)

- **CurrentSensor** – RMS/peak current computation and filtering  
- **StepperDriver** – Non-blocking microstepper motion engine  
- **ModeController** – UI state machine for all modes  
- **Modes** – HOME, MOD1, MOD2, JOG, PARAM  
- **ParametersMode** – On-device configuration editor  
- **Lcd1602** – LCD control  
- **KeypadShield** – Analog keypad driver  
- **MovingAverage** – Optimized fixed-point moving average filter  
- **Parameters** – Global parameter set  

---

## 🏛 Intellectual Property & Copyright

**© HUN-REN Centre for Energy Research (EK)**  
**Nanostructures Department, Institute of Technical Physics and Materials Science (MFA)**  
*All rights reserved.*

This hardware and software package—including all design files, firmware,  
and documentation—is the intellectual property of the Nanostructures Department.  
Any publication, redistribution, or commercial use requires  
**explicit written permission** from the department.

---

## 👥 Authors

### **Tamás András**  
MSc Physics student, Babeș–Bolyai University  
Email: **andrastamas44@gmail.com**  
GitHub: **https://github.com/AndrasTamas9**

### **Dr. Gergely Dobrik**  
PhD, Senior Research Fellow  
Nanostructures Department  
HUN-REN Centre for Energy Research (EK)  
Institute of Technical Physics and Materials Science (MFA)  
Email: **dobrik@energia.mta.hu**  
Phone: +36-1-392-2222 ext. 1378  
Fax: +36-1-392-2226

---

## ⭐ Acknowledgements

This project was developed in the  
**Nanostructures Department, HUN-REN Centre for Energy Research (EK),  
Institute of Technical Physics and Materials Science (MFA)**  
with support and supervision from **Dr. Gergely Dobrik**.

---
