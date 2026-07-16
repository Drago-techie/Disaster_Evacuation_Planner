# Disaster Evacuation Route Planner

A modern, high-performance C++ GUI application for emergency evacuation pathfinding, multi-agent crowd flow simulation, dynamic flood propagation, and real-time algorithm performance benchmarking. 

Built for the **SIH Disaster Management** theme using **C++17**, **Raylib 5.0**, and **CMake**.

![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![Build](https://img.shields.io/badge/Build-CMake%20%7C%20MinGW-green.svg)
![WebAssembly](https://img.shields.io/badge/WebAssembly-Wasm-purple.svg)
![GUI](https://img.shields.io/badge/GUI-Raylib%205.0-orange.svg)

---

## Live Interactive Web Demo

- **WebAssembly (Wasm) Browser Preview**: [https://drago-techie.github.io/Disaster_Evacuation_Planner/](https://drago-techie.github.io/Disaster_Evacuation_Planner/)

---

## Key Features

1. **Modular Graph Architecture (Adjacency List)**:
   - Dynamic Nodes representing *Evacuee Spawns*, *Safe Zone Exits*, *Hazard/Smoke Zones*, and *Blocked Impassable Routes*.
   - Dynamic Corridor Edges combining physical distance ($m$), hazard heat, capacity limits, and dynamic congestion bottlenecks.

2. **Real-Time Algorithm Search Benchmarking**:
   - **BFS (Breadth-First Search)**: Finds hop-minimal evacuation routes around structural blockages.
   - **Greedy Best-First Search**: Heuristic search prioritising distance to exits and danger avoidance.
   - **Dijkstra Search**: Baseline optimal weighted safety route solver.
   - **Performance HUD**: Real-time microsecond ($\mu s$) latency, visited nodes count, total distance, survival rate %, and clearance time estimation.

3. **Multi-Agent Crowd Particle Flow**:
   - Spawns live glowing particle streams moving along parallel subway-style tracks toward safe exit gates.
   - Speed dynamically slows down on congested/hazardous corridors.

4. **Dynamic Flash Flood Propagation (City Flash Flood Scenario)**:
   - Cap total flood duration at 3 minutes with slow, one-by-one sector inundation.
   - Real-time path recalculation as rising water engulfs sectors.

5. **Interactive Canvas Editor & Tools**:
   - Add/drag nodes, link corridors, toggle hazard radii, and delete nodes/edges via UI buttons or `Delete`/`Backspace` keyboard shortcuts.

---

## Build & Installation

### Requirements
- **C++17 Compiler** (GCC / MinGW-w64 or MSVC)
- **CMake 3.16+**
- **Git**

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/Drago-techie/Disaster_Evacuation_Planner.git
cd Disaster_Evacuation_Planner

# Configure CMake build directory
cmake -B build -G "MinGW Makefiles"

# Compile application executable
cmake --build build

# Launch GUI Application
./build/DisasterEvacPlanner.exe
```

---

## Repository Structure

```text
Disaster_Evacuation_Planner/
├── .github/
│   └── workflows/
│       └── deploy-pages.yml # Wasm GitHub Pages Deployment Workflow
├── assets/
│   └── fonts/
│       └── font.ttf         # Bundled TTF Vector Typography Asset
├── src/
│   ├── core/
│   │   ├── graph.hpp        # Adjacency List Graph Engine & Hazard Radius calculations
│   │   ├── solvers.hpp      # BFS, Greedy, and Dijkstra solvers with SearchStep recording
│   │   ├── solvers.cpp      # Pathfinding implementations & Evacuee Throughput Analytics
│   │   └── presets.hpp      # Pre-built disaster scenario maps
│   ├── gui/
│   │   ├── app.hpp          # Application state machine & rendering declarations
│   │   └── app.cpp          # Canvas renderer, GUI control widgets & animation engine
│   └── main.cpp             # Entry point
├── CMakeLists.txt           # CMake build configuration with auto-fetching Raylib
├── .gitignore
└── README.md
```
