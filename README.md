# Disaster Evacuation Route Planner 🚨🗺️

A modern, high-performance C++ GUI application for emergency evacuation pathfinding and real-time algorithm performance benchmarking. 

Built for the **SIH Disaster Management** theme using **C++17**, **Raylib**, and **CMake**.

![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![Build](https://img.shields.io/badge/Build-CMake%20%7C%20MinGW-green.svg)
![GUI](https://img.shields.io/badge/GUI-Raylib%205.0-orange.svg)

---

## 🌟 Key Features

1. **Modular Graph Architecture (Adjacency List)**:
   - Dynamic Nodes representing *Evacuee Spawns*, *Safe Zone Exits*, *Hazard/Smoke Zones*, and *Blocked Impassable Routes*.
   - Dynamic Corridor Edges combining physical distance ($m$), hazard heat, capacity limits, and dynamic congestion bottlenecks.

2. **Real-Time Algorithm Search Benchmarking**:
   - **BFS (Breadth-First Search)**: Finds hop-minimal evacuation routes around structural blockages.
   - **Greedy Best-First Search**: Heuristic search prioritising distance to exits and danger avoidance.
   - **Dijkstra Search**: Baseline optimal weighted safety route solver.
   - **Performance HUD**: Real-time microsecond ($\mu s$) latency, visited nodes count, total distance, survival rate %, and clearance time estimation.

3. **Interactive Graphical Simulation Engine**:
   - **Visual Canvas Editor**: Click & drag nodes, add/connect edges, right-click hazard cycling.
   - **Dynamic Hazard Heat Radius**: Spawning a hazard node renders an expanding red radial danger zone that inflates neighboring corridor risk.
   - **Bottleneck Visualizer**: Corridor thickness ($2.5\text{px} \to 8.0\text{px}$) scales with live congestion; heavy bottlenecks pulsate in dark crimson red.
   - **Step-by-Step Search Playback**: Play/Pause, Step Forward, and speed controls ($0.5x, 0.75x, 1x, 2x$) to watch search wavefronts expand in real time.

4. **Preset Disaster Scenarios**:
   - 🏢 **Building Floor Plan**: Multi-floor office layout with stairwells and fire exit gates.
   - 🌊 **City Flash Flood**: Urban sector with river overflow surge, flooded subway entrances, and hilltop relief shelters.
   - 🏟️ **Stadium Arena Plan**: Arena field evacuation through concourses, ramps, and perimeter safe zones.

---

## 🛠️ Build & Installation

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

## 📁 Repository Structure

```text
Disaster_Evacuation_Planner/
├── assets/
│   └── fonts/
│       └── font.ttf       # Bundled TTF Vector Typography Asset
├── src/
│   ├── core/
│   │   ├── graph.hpp      # Adjacency List Graph Engine & Hazard Radius calculations
│   │   ├── solvers.hpp    # BFS, Greedy, and Dijkstra solvers with SearchStep recording
│   │   ├── solvers.cpp    # Pathfinding implementations & Evacuee Throughput Analytics
│   │   └── presets.hpp    # Pre-built disaster scenario maps
│   ├── gui/
│   │   ├── app.hpp        # Application state machine & rendering declarations
│   │   └── app.cpp        # Canvas renderer, GUI control widgets & animation engine
│   └── main.cpp           # Entry point
├── CMakeLists.txt         # CMake build configuration with auto-fetching Raylib
├── .gitignore
└── README.md
```
