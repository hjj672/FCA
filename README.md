# FCA — Smart Fungi Chamber

**FCA (Fungal Cultivation Automation)** is an experimental platform for monitoring and automating mushroom cultivation environments.

The project combines **fungal biology, environmental sensing, embedded systems, automation, and data logging**, with future work extending toward computer vision, machine learning, and fungal bioelectrical signal measurement.

> **Goal:** Build an affordable, modular system for observing, measuring, and controlling fungal cultivation environments.

---

## 📌 Project Status

**Current stage:** Experimental / Active Development

FCA is currently focused on **environmental monitoring, automated cultivation control, and data logging using an ESP32-based system**.

Computer vision, AI/ML, fungal electrical-signal acquisition, and the Forest API are **future research directions** and are not yet part of the completed system.

---

# ✅ What Exists Now

## 🔧 Current Hardware

The current FCA system is based around an **ESP32** microcontroller.

Current components include:

* ESP32 DevKit
* SHT31 temperature/humidity sensors
* Relay modules
* Ventilation fans
* Humidifier
* LED lighting
* OLED display
* Local storage
* Power distribution and control circuitry

### Current sensor configuration

Two SHT31 sensors are used to measure environmental conditions at different positions in the chamber.

```text
SHT31 #1
SDA → GPIO 21
SCL → GPIO 22

SHT31 #2
SDA → GPIO 25
SCL → GPIO 26
```

The hardware configuration may change as the project develops.

---

# 🖥️ Current Firmware

The ESP32 firmware currently handles:

* Temperature measurement
* Relative-humidity measurement
* Sensor data acquisition
* Relay control
* Ventilation control
* Humidification control
* Lighting control
* OLED status display
* Local data logging
* Threshold-based environmental control

The project has also experimented with **SPIFFS-based CSV logging**, allowing measurements to be stored locally without requiring an internet connection.

---

# 📊 Current Data Logging

Environmental measurements can be recorded locally as CSV data.

Example:

```text
timestamp,temperature,humidity
2026-09-27T12:00,24.1,88.2
2026-09-27T12:05,24.0,89.1
2026-09-27T12:10,24.2,90.0
```

The system has been configured for approximately **5-minute measurement intervals**.

The purpose of the logging system is to build a historical record of the cultivation environment for later analysis.

---

# 🎛️ Current Environmental Control

The current control system uses **threshold-based logic**.

For example:

```text
IF humidity < lower_threshold
    → activate humidifier

IF humidity > upper_threshold
    → deactivate humidifier
```

A range can be used instead of a single target value to reduce unnecessary actuator switching.

```text
Humidity

Lower limit ─────────────── Upper limit
     85%                         93%

        ← acceptable range →
```

The exact thresholds depend on the cultivation experiment and are not considered universal biological requirements.

---

# 🍄 Current Biological Scope

The project is intended for mushroom cultivation experiments.

Initial species of interest include:

* **Pleurotus ostreatus** — Oyster mushroom
* **Hericium erinaceus** — Lion's Mane
* **Lentinula edodes** — Shiitake

Species-specific cultivation parameters are treated as experimental variables rather than fixed FCA system specifications.

---

# 🧪 Current Experimental Approach

The project follows an iterative engineering and experimental process:

```text
Observe
   ↓
Measure
   ↓
Modify
   ↓
Run experiment
   ↓
Collect data
   ↓
Analyze
   ↓
Repeat
```

The current focus is establishing a reliable environmental-control and data-acquisition platform before adding more complex biological measurements.

---

# 🚧 Future Development

The following sections describe **planned or experimental future work**. They should not be interpreted as currently implemented FCA capabilities.

---

## 👁️ Future — Computer Vision

A future version of FCA may include a camera system for periodically recording fungal growth.

Potential applications:

* Fruiting-body detection
* Primordia detection
* Mushroom-size measurement
* Growth-rate estimation
* Surface-colonization analysis
* Morphological analysis
* Contamination detection
* Harvest-stage estimation

Possible pipeline:

```text
Camera
   ↓
Image acquisition
   ↓
Segmentation
   ↓
Fungal structure detection
   ↓
Morphological measurements
   ↓
Growth analysis
```

This system has not yet been implemented as part of the current FCA platform.

---

# ⚡ Future — Fungal Electrical Signals

A longer-term experimental direction is investigating electrical signals associated with fungal mycelium.

A possible future acquisition system could contain:

* Electrodes
* AD620 instrumentation amplifier
* ADS1115 ADC
* ESP32
* Signal filtering
* Local time-series logging

Conceptually:

```text
Fungal mycelium
       │
       ▼
   Electrodes
       │
       ▼
     AD620
       │
       ▼
    ADS1115
       │
       ▼
      ESP32
       │
       ▼
    Time-series
       │
       ▼
   Data analysis
```

This is a **future experimental research direction**.

The project does not currently claim that such signals can be used to decode fungal communication, cognition, or biological intent.

The initial research question would simply be whether reproducible electrical signals can be measured under controlled experimental conditions.

---

# 🧠 Future — AI / Machine Learning

Once sufficient environmental and biological data have been collected, FCA may incorporate machine-learning methods.

Potential applications include:

* Growth prediction
* Fruiting-stage estimation
* Anomaly detection
* Environmental-response analysis
* Contamination detection
* Harvest-time estimation
* Analysis of relationships between environmental conditions and fungal morphology

A possible future architecture:

```text
Environmental data
        +
Camera data
        +
Cultivation history
        +
Growth measurements
        │
        ▼
   Machine Learning
        │
        ▼
 Biological / Growth Models
        │
        ▼
 Experimental Decisions
```

No production machine-learning model is currently part of FCA.

---

# 🌲 Future Concept — Forest API

The **Forest API** is a long-term conceptual direction for FCA.

The idea is to create a standardized interface through which biological systems could expose measurable observations to software systems.

Conceptually:

```text
                ENVIRONMENT
                     │
       ┌─────────────┼─────────────┐
       ▼             ▼             ▼
   Temperature     Humidity       Light
       │             │             │
       └─────────────┼─────────────┘
                     ▼
                  FUNGUS
                     │
          ┌──────────┴──────────┐
          ▼                     ▼
       Growth              Electrical
       patterns              signals
          │                     │
          └──────────┬──────────┘
                     ▼
                    FCA
                     │
                     ▼
                 DATA / API
```

The Forest API is **only a concept at the current stage**.

Potential future work includes:

* Biological data schemas
* Standardized sensor measurements
* Biological event definitions
* Time-series interfaces
* Integration with computer vision
* Integration with fungal electrophysiology
* Communication between multiple FCA systems

---

# 🗺️ Roadmap

## Phase 1 — Environmental Monitoring

* [x] ESP32 environmental monitoring
* [x] Temperature measurement
* [x] Relative humidity measurement
* [x] OLED status display
* [x] Relay control
* [x] Local CSV logging
* [x] Basic automated environmental control

## Phase 2 — Cultivation Automation

* [ ] Improve ventilation control
* [ ] Improve humidification control
* [ ] Improve electrical safety
* [ ] Add additional environmental sensors
* [ ] Improve data reliability
* [ ] Standardize cultivation experiments

## Phase 3 — Computer Vision

* [ ] Install dedicated camera
* [ ] Automatic image acquisition
* [ ] Build fungal-growth dataset
* [ ] Image segmentation
* [ ] Growth-rate estimation
* [ ] Morphological analysis
* [ ] Contamination detection

## Phase 4 — Biological Signal Acquisition

* [ ] Design electrode system
* [ ] Build analog front-end
* [ ] Test AD620 amplifier
* [ ] Test ADS1115 acquisition
* [ ] Characterize electrical noise
* [ ] Develop signal-processing pipeline
* [ ] Perform controlled experiments

## Phase 5 — AI-Assisted Analysis

* [ ] Combine environmental and visual data
* [ ] Build experimental dataset
* [ ] Growth analysis
* [ ] Anomaly detection
* [ ] Environmental-response modeling
* [ ] Experimental optimization

## Phase 6 — Forest API

* [ ] Define biological data model
* [ ] Standardize measurements
* [ ] Define biological events
* [ ] Develop API
* [ ] Integrate biological signal data
* [ ] Connect multiple FCA systems

---

# 📁 Repository Structure

```text
FCA/
│
├── firmware/
│   ├── esp32/
│   ├── sensors/
│   ├── actuators/
│   └── logging/
│
├── hardware/
│   ├── schematics/
│   ├── pcb/
│   ├── wiring/
│   └── bom/
│
├── software/
│   ├── data_analysis/
│   ├── computer_vision/
│   └── machine_learning/
│
├── experiments/
│   ├── cultivation/
│   ├── environmental/
│   └── electrical_signals/
│
├── data/
│
├── documentation/
│   ├── architecture/
│   ├── experiments/
│   └── protocols/
│
├── images/
│
└── README.md
```

---

# 🤝 Contributing

Contributions are welcome in areas related to:

* ESP32 firmware
* Sensor integration
* Electronics
* PCB design
* Environmental-control algorithms
* Data analysis
* Computer vision
* Machine learning
* Mycology
* Experimental design
* Fungal electrophysiology
* Documentation

Experimental contributions should include sufficient information to reproduce the experiment whenever possible.

---

# ⚠️ Experimental Disclaimer

FCA is an experimental engineering and biological research project.

The system involves electricity, water, humidity, and automated equipment. Appropriate electrical, biological, and laboratory safety practices should be followed.

Measurements produced by FCA should be treated as experimental data. Biological conclusions require appropriate controls, replication, and analysis.

---

# 📜 License

**License:** TBD

---

# 👤 Project

**FCA — Fungal Cultivation Automation**

An independent experimental project at the intersection of:

**Mycology × Electronics × Automation × Data Science × AI**

> *Measure the organism. Understand the environment. Automate the system. Learn from the fungus.*

