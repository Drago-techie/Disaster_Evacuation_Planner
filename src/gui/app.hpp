#ifndef APP_HPP
#define APP_HPP

#include "../core/graph.hpp"
#include "../core/solvers.hpp"
#include "raylib.h"
#include <vector>
#include <string>

namespace Evac {

enum class ToolMode {
    Select,
    AddNode,
    AddEdge,
    ToggleBlock,
    SetHazard,
    SetSpawn,
    SetSafeZone,
    DeleteNode,
    DeleteEdge
};

struct EvacueeAgent {
    int id;
    int algorithmType; // 0 = BFS (Cyan), 1 = Greedy (Purple), 2 = Dijkstra (Lime)
    std::vector<int> pathNodes;
    size_t currentSegmentIndex = 0;
    float progress = 0.0f; // 0.0 to 1.0 along current edge segment
    float baseSpeed = 130.0f; // Base pixels per second
    Color color;
    float lineOffset = 0.0f;
};

class EvacPlannerApp {
private:
    Graph graph_;
    ToolMode currentTool_ = ToolMode::Select;

    int selectedNodeId_ = -1;
    int hoveredNodeId_ = -1;
    int draggingNodeId_ = -1;
    int edgeStartNodeId_ = -1;

    int selectedStartSpawnId_ = 1;

    PathResult bfsResult_;
    PathResult greedyResult_;
    PathResult dijkstraResult_;

    bool showBFS_ = true;
    bool showGreedy_ = true;
    bool showDijkstra_ = true;

    // Multi-Agent Evacuation Flow Simulator State
    bool showLiveAgentStream_ = true;
    std::vector<EvacueeAgent> activeAgents_;
    float spawnTimer_ = 0.0f;
    float spawnIntervalSec_ = 0.30f; // Spawn a new crowd wave every 300ms
    int totalAgentsEvacuated_ = 0;
    int nextAgentId_ = 1;

    // Dynamic Flash Flood Propagation Engine (Exclusively for City Flash Flood Scenario)
    bool isFlashFloodScenario_ = false;
    bool isFloodSpreadingActive_ = false;
    float floodSpreadTimer_ = 0.0f;
    float floodSpreadIntervalSec_ = 3.5f; // Water spreads to neighboring corridors every 3.5s

    // Step-by-Step Animation State Machine
    bool isAnimating_ = false;
    int currentAnimStep_ = 0;
    float animTimer_ = 0.0f;
    float animSpeedMs_ = 350.0f; // Base milliseconds per step
    float animSpeedMultiplier_ = 1.0f; // 0.5x, 0.75x, 1.0x, 2.0x

    std::string statusMessage_ = "Ready. Select an evacuee spawn point and run algorithms.";

    Font uiFont_;
    bool isFontLoaded_ = false;

public:
    EvacPlannerApp();
    void run();

private:
    void initGraph();
    void update();
    void draw();

    void solveAll();
    int getNodeAtPosition(Point2D pos, float radius = 25.0f);
    void drawHeader();
    void drawSidebar();
    void drawCanvas();
    void drawMetricsOverlay();
    void drawPathLine(const std::vector<int>& pathNodes, float offset, unsigned char r, unsigned char g, unsigned char b);

    void drawText(const char* text, float posX, float posY, float fontSize, Color color);
    float measureTextWidth(const char* text, float fontSize);
};

} // namespace Evac

#endif // APP_HPP
