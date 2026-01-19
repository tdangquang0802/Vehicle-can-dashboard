# Vehicle CAN Dashboard

## Overview
This project is a personal automotive embedded project that simulates a
Vehicle Health & Diagnostics Dashboard using CAN bus data.

The purpose of this project is to practice CAN communication,
Qt/QML-based HMI development, and system architecture design
commonly used in automotive embedded software.

## Features
- Receive and parse CAN frames (simulated or real CAN)
- Display vehicle data such as RPM, speed, coolant temperature
- Qt Quick based dashboard UI
- Keyboard-based CAN signal simulation for development

## Tech Stack
- C++
- Qt 6 (QtQuick, QtSerialBus)
- QML
- Linux / SocketCAN
- Git & GitHub

## Project Structure
- src/backend - CAN receiver and parser
- src/ui - QML user interface
- docs - System documentation
- tools - Simulation and test tools

## Current Status
- CAN simulation: In progress
- UI dashboard: In progress

## Roadmap
- Add DTC (Diagnostic Trouble Codes)
- Support real CAN hardware
- Improve UI animations
