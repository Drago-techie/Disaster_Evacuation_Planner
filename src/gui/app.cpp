#include "app.hpp"
#include "../core/presets.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace Evac {

EvacPlannerApp::EvacPlannerApp() {
    initGraph();
}

void EvacPlannerApp::initGraph() {
    PresetScenarios::loadHighRiseFloorPlan(graph_);
    isFlashFloodScenario_ = false;
    isFloodSpreadingActive_ = false;
    floodTotalElapsedSec_ = 0.0f;
    floodSpreadTimer_ = 0.0f;
    auto spawns = graph_.getSpawnIds();
    if (!spawns.empty()) {
        selectedStartSpawnId_ = spawns[0];
    } else {
        selectedStartSpawnId_ = 1;
    }
    solveAll();
}

void EvacPlannerApp::solveAll() {
    bfsResult_ = EvacSolver::solveBFS(graph_, selectedStartSpawnId_);
    greedyResult_ = EvacSolver::solveGreedy(graph_, selectedStartSpawnId_);
    dijkstraResult_ = EvacSolver::solveDijkstra(graph_, selectedStartSpawnId_);

    currentAnimStep_ = 0;
    animTimer_ = 0.0f;
    activeAgents_.clear();
    spawnTimer_ = 0.0f;

    if (bfsResult_.found && greedyResult_.found) {
        statusMessage_ = "Computed evacuation routes for Evacuee Node " + std::to_string(selectedStartSpawnId_) + ". Simulation engine ready.";
    } else if (!bfsResult_.found && !greedyResult_.found) {
        std::vector<int> allSafe = graph_.getSafeZoneIds();
        std::vector<int> validSafe = graph_.getValidSafeZoneIds();
        if (!allSafe.empty() && validSafe.empty()) {
            statusMessage_ = "CRITICAL ALERT: All designated safe exit gates are engulfed in hazard spheres! Evacuees must shelter in place.";
        } else {
            statusMessage_ = "WARNING: No evacuation path available from Evacuee Node " + std::to_string(selectedStartSpawnId_) + "! All exits blocked.";
        }
    } else {
        statusMessage_ = "Partial path coverage detected under simulated hazard conditions.";
    }
}

int EvacPlannerApp::getNodeAtPosition(Point2D pos, float radius) {
    for (const auto& pair : graph_.getNodes()) {
        const Node& node = pair.second;
        if (pos.distanceTo(node.pos) <= radius) {
            return node.id;
        }
    }
    return -1;
}

void EvacPlannerApp::drawText(const char* text, float posX, float posY, float fontSize, Color color) {
    Vector2 pos{std::floor(posX), std::floor(posY)};
    if (isFontLoaded_) {
        DrawTextEx(uiFont_, text, pos, fontSize, 0.0f, color);
    } else {
        DrawText(text, (int)pos.x, (int)pos.y, (int)fontSize, color);
    }
}

float EvacPlannerApp::measureTextWidth(const char* text, float fontSize) {
    if (isFontLoaded_) {
        return MeasureTextEx(uiFont_, text, fontSize, 0.0f).x;
    } else {
        return (float)MeasureText(text, (int)fontSize);
    }
}

void EvacPlannerApp::run() {
    const int screenWidth = 1380;
    const int screenHeight = 840;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(screenWidth, screenHeight, "Disaster Evacuation Route Planner - BFS vs Greedy Benchmark");
    SetTargetFPS(60);

    std::string fontPath = "";
    if (FileExists("assets/fonts/font.ttf")) {
        fontPath = "assets/fonts/font.ttf";
    } else if (FileExists("../assets/fonts/font.ttf")) {
        fontPath = "../assets/fonts/font.ttf";
    } else if (FileExists("C:\\Windows\\Fonts\\segoeuib.ttf")) {
        fontPath = "C:\\Windows\\Fonts\\segoeuib.ttf";
    } else if (FileExists("C:\\Windows\\Fonts\\segoeui.ttf")) {
        fontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
    }

    if (!fontPath.empty()) {
        uiFont_ = LoadFontEx(fontPath.c_str(), 36, 0, 0);
        if (uiFont_.texture.id != 0) {
            SetTextureFilter(uiFont_.texture, TEXTURE_FILTER_POINT);
            isFontLoaded_ = true;
        }
    }

    while (!WindowShouldClose()) {
        update();
        draw();
    }

    if (isFontLoaded_) {
        UnloadFont(uiFont_);
    }

    CloseWindow();
}

void EvacPlannerApp::update() {
    float dt = GetFrameTime();
    Vector2 mousePos = GetMousePosition();
    Point2D mPos{mousePos.x, mousePos.y};

    // Step-by-Step Playback animation timer logic
    if (isAnimating_) {
        animTimer_ += dt * 1000.0f * animSpeedMultiplier_;
        if (animTimer_ >= animSpeedMs_) {
            animTimer_ = 0.0f;
            currentAnimStep_++;

            size_t maxSteps = std::max({bfsResult_.steps.size(), greedyResult_.steps.size(), dijkstraResult_.steps.size()});
            if (currentAnimStep_ >= (int)maxSteps) {
                isAnimating_ = false;
                statusMessage_ = "Step-by-step playback simulation complete.";
            }
        }
    }

    // Dynamic Slow One-by-One Flash Flood Propagation Logic (Max 3-minute backend timer limit)
    if (isFlashFloodScenario_ && isFloodSpreadingActive_) {
        if (floodTotalElapsedSec_ < maxFloodDurationSec_) {
            floodTotalElapsedSec_ += dt;
            floodSpreadTimer_ += dt;

            if (floodSpreadTimer_ >= floodSpreadIntervalSec_) {
                floodSpreadTimer_ = 0.0f;

                // Find candidate normal nodes adjacent to active flood zones
                std::vector<int> candidates;
                for (const auto& pair : graph_.getNodes()) {
                    if (pair.second.type == NodeType::HazardZone) {
                        for (const auto& edge : graph_.getEdgesFrom(pair.first)) {
                            int nId = edge.toNode;
                            const Node* n = graph_.getNode(nId);
                            if (n && n->type == NodeType::Normal) {
                                if (std::find(candidates.begin(), candidates.end(), nId) == candidates.end()) {
                                    candidates.push_back(nId);
                                }
                            }
                        }
                    }
                }

                if (!candidates.empty()) {
                    // Pick ONE single node slowly to inundate
                    int nextFloodedNodeId = candidates[0];
                    Node* n = graph_.getNode(nextFloodedNodeId);
                    if (n) {
                        n->type = NodeType::HazardZone;
                        solveAll();

                        int remainingSec = (int)(maxFloodDurationSec_ - floodTotalElapsedSec_);
                        int remainingMins = remainingSec / 60;
                        int remSecPart = remainingSec % 60;

                        std::stringstream ss;
                        ss << "FLASH FLOOD SPREADING (Slow): Inundated sector '" << n->name 
                           << "'. Max 3-min flood timer remaining: " << remainingMins << "m " << remSecPart << "s.";
                        statusMessage_ = ss.str();
                    }
                } else {
                    statusMessage_ = "FLASH FLOOD PROPAGATION: Maximum inundation area reached.";
                }
            }
        } else {
            isFloodSpreadingActive_ = false;
            statusMessage_ = "FLASH FLOOD SIMULATION: 3-minute max propagation duration reached. Flood expansion halted.";
        }
    }

    // Multi-Agent Evacuation Flow Physics Loop
    if (showLiveAgentStream_) {
        spawnTimer_ += dt;
        if (spawnTimer_ >= spawnIntervalSec_) {
            spawnTimer_ = 0.0f;

            auto spawnAgent = [&](const PathResult& res, int algoType, Color col, float lineOffset) {
                if (res.found && res.pathNodes.size() >= 2) {
                    EvacueeAgent agent;
                    agent.id = nextAgentId_++;
                    agent.algorithmType = algoType;
                    agent.pathNodes = res.pathNodes;
                    agent.currentSegmentIndex = 0;
                    agent.progress = 0.0f;
                    agent.baseSpeed = 135.0f;
                    agent.color = col;
                    agent.lineOffset = lineOffset;
                    activeAgents_.push_back(agent);
                }
            };

            if (showBFS_) spawnAgent(bfsResult_, 0, Color{0, 220, 255, 255}, -7.0f);
            if (showGreedy_) spawnAgent(greedyResult_, 1, Color{220, 90, 255, 255}, 0.0f);
            if (showDijkstra_) spawnAgent(dijkstraResult_, 2, Color{90, 240, 100, 255}, 7.0f);
        }

        // Update particle movements along edge corridors
        for (auto it = activeAgents_.begin(); it != activeAgents_.end(); ) {
            EvacueeAgent& agent = *it;
            if (agent.currentSegmentIndex >= agent.pathNodes.size() - 1) {
                totalAgentsEvacuated_++;
                it = activeAgents_.erase(it);
                continue;
            }

            int uId = agent.pathNodes[agent.currentSegmentIndex];
            int vId = agent.pathNodes[agent.currentSegmentIndex + 1];

            const Node* u = graph_.getNode(uId);
            const Node* v = graph_.getNode(vId);

            if (!u || !v) {
                it = activeAgents_.erase(it);
                continue;
            }

            float dist = u->pos.distanceTo(v->pos);
            if (dist <= 0.0f) dist = 1.0f;

            float radiusHeat = graph_.getEdgeHazardRadiusInfluence(uId, vId);
            float totalRisk = std::min(1.0f, radiusHeat * 1.5f);

            // Speed slows down on congested/hazardous corridors
            float moveSpeed = agent.baseSpeed / (1.0f + 1.8f * totalRisk);

            agent.progress += (moveSpeed * dt) / dist;

            if (agent.progress >= 1.0f) {
                agent.progress = 0.0f;
                agent.currentSegmentIndex++;
                if (agent.currentSegmentIndex >= agent.pathNodes.size() - 1) {
                    totalAgentsEvacuated_++;
                    it = activeAgents_.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    hoveredNodeId_ = getNodeAtPosition(mPos);

    // Keyboard shortcut: Press DEL or BACKSPACE to instantly delete hovered node
    if ((IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) && hoveredNodeId_ != -1) {
        int targetDel = hoveredNodeId_;
        graph_.removeNode(targetDel);
        if (selectedStartSpawnId_ == targetDel) {
            auto spawns = graph_.getSpawnIds();
            selectedStartSpawnId_ = spawns.empty() ? 1 : spawns[0];
        }
        solveAll();
        statusMessage_ = "Deleted Node " + std::to_string(targetDel) + " via keyboard shortcut.";
    }

    bool mouseOnCanvas = (mousePos.x > 260.0f && mousePos.x < 1370.0f && mousePos.y > 72.0f && mousePos.y < 570.0f);

    if (mouseOnCanvas) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (currentTool_ == ToolMode::DeleteNode) {
                if (hoveredNodeId_ != -1) {
                    int delId = hoveredNodeId_;
                    graph_.removeNode(delId);
                    if (selectedStartSpawnId_ == delId) {
                        auto spawns = graph_.getSpawnIds();
                        selectedStartSpawnId_ = spawns.empty() ? 1 : spawns[0];
                    }
                    solveAll();
                    statusMessage_ = "Deleted Node " + std::to_string(delId) + " successfully.";
                }
            } else if (currentTool_ == ToolMode::DeleteEdge) {
                if (hoveredNodeId_ != -1) {
                    if (edgeStartNodeId_ == -1) {
                        edgeStartNodeId_ = hoveredNodeId_;
                        statusMessage_ = "Delete Edge: Select second node to un-link corridor edge.";
                    } else if (edgeStartNodeId_ != hoveredNodeId_) {
                        graph_.removeEdge(edgeStartNodeId_, hoveredNodeId_);
                        edgeStartNodeId_ = -1;
                        solveAll();
                        statusMessage_ = "Corridor edge un-linked successfully.";
                    }
                }
            } else if (currentTool_ == ToolMode::ToggleBlock) {
                // Add Blockage Tool: Blocks corridor edge between two nodes
                if (hoveredNodeId_ != -1) {
                    if (edgeStartNodeId_ == -1) {
                        edgeStartNodeId_ = hoveredNodeId_;
                        statusMessage_ = "Add Blockage: Select second node to toggle corridor edge blockage.";
                    } else if (edgeStartNodeId_ != hoveredNodeId_) {
                        bool currentlyBlocked = graph_.isEdgeBlocked(edgeStartNodeId_, hoveredNodeId_);
                        graph_.setEdgeBlocked(edgeStartNodeId_, hoveredNodeId_, !currentlyBlocked);
                        int uId = edgeStartNodeId_;
                        int vId = hoveredNodeId_;
                        edgeStartNodeId_ = -1;
                        solveAll();
                        statusMessage_ = currentlyBlocked ? 
                            ("Unblocked corridor edge between Node " + std::to_string(uId) + " and Node " + std::to_string(vId) + ".") :
                            ("Blocked corridor edge between Node " + std::to_string(uId) + " and Node " + std::to_string(vId) + ".");
                    }
                }
            } else if (hoveredNodeId_ != -1) {
                if (currentTool_ == ToolMode::Select) {
                    draggingNodeId_ = hoveredNodeId_;
                    selectedNodeId_ = hoveredNodeId_;
                } else if (currentTool_ == ToolMode::SetSpawn) {
                    selectedStartSpawnId_ = hoveredNodeId_;
                    solveAll();
                } else if (currentTool_ == ToolMode::SetSafeZone) {
                    Node* n = graph_.getNode(hoveredNodeId_);
                    if (n) {
                        n->type = (n->type == NodeType::SafeZone) ? NodeType::Normal : NodeType::SafeZone;
                        solveAll();
                    }
                } else if (currentTool_ == ToolMode::AddEdge) {
                    if (edgeStartNodeId_ == -1) {
                        edgeStartNodeId_ = hoveredNodeId_;
                        statusMessage_ = "Edge tool: Select destination node to link.";
                    } else if (edgeStartNodeId_ != hoveredNodeId_) {
                        graph_.addEdge(edgeStartNodeId_, hoveredNodeId_);
                        edgeStartNodeId_ = -1;
                        solveAll();
                        statusMessage_ = "Edge created successfully.";
                    }
                }
            } else if (currentTool_ == ToolMode::AddNode) {
                std::string nodeName = "Zone " + std::to_string(graph_.getNodes().size() + 1);
                graph_.addNode(nodeName, mPos, NodeType::Normal);
                solveAll();
                statusMessage_ = "Added new node at canvas cursor.";
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && draggingNodeId_ != -1) {
            Node* n = graph_.getNode(draggingNodeId_);
            if (n) {
                n->pos = mPos;
                for (auto& edgePair : const_cast<std::unordered_map<int, std::vector<Edge>>&>(graph_.getAdjacencyList())) {
                    for (auto& edge : edgePair.second) {
                        const Node* u = graph_.getNode(edge.fromNode);
                        const Node* v = graph_.getNode(edge.toNode);
                        if (u && v) {
                            edge.distance = u->pos.distanceTo(v->pos);
                        }
                    }
                }
                solveAll();
            }
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            draggingNodeId_ = -1;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && hoveredNodeId_ != -1) {
            Node* n = graph_.getNode(hoveredNodeId_);
            if (n) {
                if (n->type == NodeType::Blocked) n->type = NodeType::Normal;
                else if (n->type == NodeType::HazardZone) n->type = NodeType::Blocked;
                else n->type = NodeType::HazardZone;
                solveAll();
            }
        }
    }
}

void EvacPlannerApp::drawHeader() {
    DrawRectangle(0, 0, 1380, 72, Color{22, 27, 36, 255});
    drawText("DISASTER EVACUATION PLANNER", 16, 12, 20, Color{250, 252, 255, 255});
    drawText("C++ Engine | Benchmark Module", 16, 40, 13, Color{180, 202, 230, 255});

    struct ButtonDef { const char* label; ToolMode mode; Color highlightColor; };
    ButtonDef buttons[] = {
        {"Select / Drag", ToolMode::Select, Color{40, 130, 240, 255}},
        {"Set Start", ToolMode::SetSpawn, Color{40, 130, 240, 255}},
        {"Toggle Exit", ToolMode::SetSafeZone, Color{40, 130, 240, 255}},
        {"Add Blockage", ToolMode::ToggleBlock, Color{220, 100, 30, 255}},
        {"Add Edge", ToolMode::AddEdge, Color{40, 130, 240, 255}},
        {"Add Node", ToolMode::AddNode, Color{40, 130, 240, 255}},
        {"Del Node", ToolMode::DeleteNode, Color{210, 50, 50, 255}},
        {"Del Edge", ToolMode::DeleteEdge, Color{210, 50, 50, 255}}
    };

    int startX = 390;
    for (int i = 0; i < 8; i++) {
        Rectangle btnRect = {(float)(startX + i * 120), 12.0f, 114.0f, 48.0f};
        bool active = (currentTool_ == buttons[i].mode);
        Color bg = active ? buttons[i].highlightColor : Color{34, 44, 60, 255};
        DrawRectangleRounded(btnRect, 0.25f, 4, bg);
        DrawRectangleRoundedLines(btnRect, 0.25f, 4, 1.5f, active ? Color{255, 255, 255, 255} : Color{70, 85, 110, 255});
        
        float txtW = measureTextWidth(buttons[i].label, 14);
        drawText(buttons[i].label, btnRect.x + (btnRect.width - txtW)/2.0f, btnRect.y + 16.0f, 14, WHITE);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), btnRect)) {
            currentTool_ = buttons[i].mode;
            edgeStartNodeId_ = -1;
            statusMessage_ = "Switched tool mode to " + std::string(buttons[i].label);
        }
    }
}

void EvacPlannerApp::drawSidebar() {
    DrawRectangle(0, 72, 260, 768, Color{26, 32, 44, 255});
    DrawLine(260, 72, 260, 840, Color{45, 56, 75, 255});

    int y = 78;
    drawText("SIMULATION PLAYBACK", 18, y, 15, Color{100, 200, 255, 255});
    y += 20;

    // Play/Pause, Step Forward & Reset Controls
    Rectangle btnPlay = {16.0f, (float)y, 110.0f, 34.0f};
    Rectangle btnStep = {134.0f, (float)y, 110.0f, 34.0f};

    DrawRectangleRounded(btnPlay, 0.2f, 4, isAnimating_ ? Color{210, 140, 30, 255} : Color{40, 160, 90, 255});
    drawText(isAnimating_ ? "Pause" : "Play Search", isAnimating_ ? 42 : 26, y + 8, 14, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), btnPlay)) {
        isAnimating_ = !isAnimating_;
        size_t maxSteps = std::max({bfsResult_.steps.size(), greedyResult_.steps.size(), dijkstraResult_.steps.size()});
        if (isAnimating_ && currentAnimStep_ >= (int)maxSteps) {
            currentAnimStep_ = 0;
        }
        statusMessage_ = isAnimating_ ? "Playback playing step-by-step..." : "Playback paused.";
    }

    DrawRectangleRounded(btnStep, 0.2f, 4, Color{45, 60, 82, 255});
    DrawRectangleRoundedLines(btnStep, 0.2f, 4, 1.0f, Color{80, 100, 130, 255});
    drawText("Step Forward", 144, y + 8, 14, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), btnStep)) {
        isAnimating_ = false;
        size_t maxSteps = std::max({bfsResult_.steps.size(), greedyResult_.steps.size(), dijkstraResult_.steps.size()});
        if (currentAnimStep_ >= (int)maxSteps) {
            currentAnimStep_ = 0;
        } else {
            currentAnimStep_++;
        }
        statusMessage_ = "Stepped to Step " + std::to_string(currentAnimStep_) + " / " + std::to_string(maxSteps);
    }

    y += 38;
    // Speed Multiplier Toggle (0.5x -> 0.75x -> 1x -> 2x) & Reset Step Buttons
    Rectangle speedRect = {16.0f, (float)y, 110.0f, 28.0f};
    Rectangle resetRect = {134.0f, (float)y, 110.0f, 28.0f};

    DrawRectangleRounded(speedRect, 0.2f, 4, Color{32, 40, 54, 255});
    std::stringstream speedSs;
    speedSs << "Speed: " << std::fixed << std::setprecision((animSpeedMultiplier_ == 1.0f || animSpeedMultiplier_ == 2.0f) ? 1 : 2) << animSpeedMultiplier_ << "x";
    std::string speedStr = speedSs.str();
    drawText(speedStr.c_str(), 24, y + 5, 13, Color{190, 215, 240, 255});
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), speedRect)) {
        if (animSpeedMultiplier_ == 0.5f) animSpeedMultiplier_ = 0.75f;
        else if (animSpeedMultiplier_ == 0.75f) animSpeedMultiplier_ = 1.0f;
        else if (animSpeedMultiplier_ == 1.0f) animSpeedMultiplier_ = 2.0f;
        else animSpeedMultiplier_ = 0.5f;
        statusMessage_ = "Playback speed set to " + speedStr;
    }

    DrawRectangleRounded(resetRect, 0.2f, 4, Color{32, 40, 54, 255});
    drawText("Reset Step", 152, y + 5, 13, Color{255, 180, 100, 255});
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), resetRect)) {
        isAnimating_ = false;
        currentAnimStep_ = 0;
        statusMessage_ = "Reset search playback to initial state.";
    }

    y += 34;
    // Multi-Agent Flow Particle Stream Toggle
    Rectangle agentStreamRect = {16.0f, (float)y, 228.0f, 28.0f};
    DrawRectangleRounded(agentStreamRect, 0.2f, 4, showLiveAgentStream_ ? Color{30, 95, 150, 255} : Color{32, 40, 54, 255});
    DrawRectangleRoundedLines(agentStreamRect, 0.2f, 4, 1.2f, showLiveAgentStream_ ? Color{100, 200, 255, 255} : Color{70, 85, 110, 255});
    std::string agentBtnText = showLiveAgentStream_ ? "Live Crowds: ON" : "Live Crowds: OFF";
    drawText(agentBtnText.c_str(), 32, y + 5, 13, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), agentStreamRect)) {
        showLiveAgentStream_ = !showLiveAgentStream_;
        if (!showLiveAgentStream_) activeAgents_.clear();
    }

    y += 34;
    DrawLine(18, y, 242, y, Color{45, 56, 75, 255});
    y += 6;

    drawText("PRESET SCENARIOS", 18, y, 15, Color{215, 230, 252, 255});
    y += 20;

    Rectangle preset1 = {16.0f, (float)y, 228.0f, 30.0f};
    Rectangle preset2 = {16.0f, (float)y + 34.0f, 228.0f, 30.0f};
    Rectangle preset3 = {16.0f, (float)y + 68.0f, 228.0f, 30.0f};

    DrawRectangleRounded(preset1, 0.2f, 4, Color{36, 48, 66, 255});
    DrawRectangleRoundedLines(preset1, 0.2f, 4, 1.0f, Color{70, 88, 112, 255});
    drawText("Building Floor Plan", 30, y + 6, 14, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), preset1)) {
        PresetScenarios::loadHighRiseFloorPlan(graph_);
        isFlashFloodScenario_ = false;
        isFloodSpreadingActive_ = false;
        floodTotalElapsedSec_ = 0.0f;
        floodSpreadTimer_ = 0.0f;
        auto spawns = graph_.getSpawnIds();
        selectedStartSpawnId_ = spawns.empty() ? 1 : spawns[0];
        solveAll();
    }

    DrawRectangleRounded(preset2, 0.2f, 4, isFlashFloodScenario_ ? Color{40, 95, 160, 255} : Color{36, 48, 66, 255});
    DrawRectangleRoundedLines(preset2, 0.2f, 4, 1.0f, isFlashFloodScenario_ ? Color{100, 200, 255, 255} : Color{70, 88, 112, 255});
    drawText("City Flash Flood 🌊", 30, y + 40, 14, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), preset2)) {
        PresetScenarios::loadCityFlashFlood(graph_);
        isFlashFloodScenario_ = true;
        isFloodSpreadingActive_ = true; // Auto-activate dynamic slow flood propagation for Flash Flood
        floodTotalElapsedSec_ = 0.0f;
        floodSpreadTimer_ = 0.0f;
        auto spawns = graph_.getSpawnIds();
        selectedStartSpawnId_ = spawns.empty() ? 1 : spawns[0];
        solveAll();
    }

    DrawRectangleRounded(preset3, 0.2f, 4, Color{36, 48, 66, 255});
    DrawRectangleRoundedLines(preset3, 0.2f, 4, 1.0f, Color{70, 88, 112, 255});
    drawText("Stadium Arena Plan", 30, y + 74, 14, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), preset3)) {
        PresetScenarios::loadStadiumEvacuation(graph_);
        isFlashFloodScenario_ = false;
        isFloodSpreadingActive_ = false;
        floodTotalElapsedSec_ = 0.0f;
        floodSpreadTimer_ = 0.0f;
        auto spawns = graph_.getSpawnIds();
        selectedStartSpawnId_ = spawns.empty() ? 1 : spawns[0];
        solveAll();
    }

    y += 104;
    DrawLine(18, y, 242, y, Color{45, 56, 75, 255});
    y += 6;

    // Slow Progressive Flash Flood Controls (Rendered exclusively for Flash Flood scenario)
    if (isFlashFloodScenario_) {
        drawText("FLOOD PROPAGATION (3 MIN MAX)", 18, y, 14, Color{100, 200, 255, 255});
        y += 18;

        Rectangle floodToggleRect = {16.0f, (float)y, 228.0f, 28.0f};
        DrawRectangleRounded(floodToggleRect, 0.2f, 4, isFloodSpreadingActive_ ? Color{200, 75, 30, 255} : Color{32, 40, 54, 255});
        DrawRectangleRoundedLines(floodToggleRect, 0.2f, 4, 1.2f, isFloodSpreadingActive_ ? Color{255, 160, 100, 255} : Color{70, 85, 110, 255});
        
        int remSec = std::max(0, (int)(maxFloodDurationSec_ - floodTotalElapsedSec_));
        int remM = remSec / 60;
        int remS = remSec % 60;

        std::stringstream floodTxtSs;
        floodTxtSs << (isFloodSpreadingActive_ ? "Slow Flood: ON (" : "Slow Flood: OFF (") << remM << "m " << remS << "s)";
        std::string floodTxt = floodTxtSs.str();

        drawText(floodTxt.c_str(), 24, y + 5, 13, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), floodToggleRect)) {
            isFloodSpreadingActive_ = !isFloodSpreadingActive_;
            statusMessage_ = isFloodSpreadingActive_ ? "Slow 1-by-1 Flash Flood spreading active (3-min max cap)." : "Flash Flood spreading paused.";
        }

        y += 32;
        DrawLine(18, y, 242, y, Color{45, 56, 75, 255});
        y += 6;
    }

    drawText("HAZARD CONTROLS", 18, y, 15, Color{215, 230, 252, 255});
    y += 18;

    // Add Blockage Tool Sidebar Shortcut Button (Available for ALL scenarios)
    Rectangle btnBlockage = {16.0f, (float)y, 228.0f, 28.0f};
    bool isBlockToolActive = (currentTool_ == ToolMode::ToggleBlock);
    DrawRectangleRounded(btnBlockage, 0.2f, 4, isBlockToolActive ? Color{220, 100, 30, 255} : Color{45, 56, 75, 255});
    DrawRectangleRoundedLines(btnBlockage, 0.2f, 4, 1.2f, isBlockToolActive ? Color{255, 180, 100, 255} : Color{80, 100, 130, 255});
    drawText("Add Blockage Tool", 38, y + 5, 13, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), btnBlockage)) {
        currentTool_ = ToolMode::ToggleBlock;
        edgeStartNodeId_ = -1;
        statusMessage_ = "Switched to Add Blockage Tool. Click two nodes to toggle corridor edge blockage.";
    }

    y += 32;
    Rectangle btnClear = {16.0f, (float)y, 228.0f, 28.0f};
    DrawRectangleRounded(btnClear, 0.2f, 4, Color{185, 45, 45, 255});
    drawText("Clear All Blockages", 34, y + 5, 13, WHITE);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), btnClear)) {
        for (auto& pair : const_cast<std::unordered_map<int, std::vector<Edge>>&>(graph_.getAdjacencyList())) {
            for (auto& edge : pair.second) {
                edge.isBlocked = false;
                edge.hazardLevel = 0.0f;
                edge.congestion = 0.0f;
            }
        }
        for (const auto& pair : graph_.getNodes()) {
            Node* n = const_cast<Node*>(graph_.getNode(pair.first));
            if (n && n->type == NodeType::Blocked) n->type = NodeType::Normal;
        }
        floodTotalElapsedSec_ = 0.0f;
        floodSpreadTimer_ = 0.0f;
        solveAll();
    }

    y += 34;
    DrawLine(18, y, 242, y, Color{45, 56, 75, 255});
    y += 6;

    drawText("ALGORITHM OVERLAYS", 18, y, 15, Color{215, 230, 252, 255});
    y += 18;

    auto drawToggle = [&](const char* label, bool& val, Color c) {
        Rectangle r = {18.0f, (float)y, 16.0f, 16.0f};
        DrawRectangleRec(r, val ? c : Color{40, 48, 60, 255});
        DrawRectangleLinesEx(r, 1.5f, WHITE);
        drawText(label, 44, y + 1, 13, WHITE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), Rectangle{18.0f, (float)y, 220.0f, 18.0f})) {
            val = !val;
        }
        y += 20;
    };

    drawToggle("BFS Shortest Hop (Cyan)", showBFS_, Color{0, 210, 255, 255});
    drawToggle("Greedy Best-First (Purple)", showGreedy_, Color{210, 85, 255, 255});
    drawToggle("Dijkstra Optimal (Lime)", showDijkstra_, Color{80, 235, 90, 255});

    y += 4;
    DrawLine(18, y, 242, y, Color{45, 56, 75, 255});
    y += 6;

    drawText("GRAPH LEGEND", 18, y, 15, Color{215, 230, 252, 255});
    y += 18;
    DrawCircle(26, y + 5, 6, Color{240, 140, 30, 255}); drawText("Start Evacuees", 42, y, 13, Color{225, 235, 250, 255}); y += 16;
    DrawCircle(26, y + 5, 6, Color{40, 200, 100, 255}); drawText("Safe Zone Exit", 42, y, 13, Color{225, 235, 250, 255}); y += 16;
    DrawCircle(26, y + 5, 6, Color{220, 60, 60, 255}); drawText("Hazard / Smoke", 42, y, 13, Color{225, 235, 250, 255}); y += 16;
    DrawCircle(26, y + 5, 6, Color{70, 75, 85, 255}); drawText("Blocked Route", 42, y, 13, Color{225, 235, 250, 255}); y += 16;

    // Simulation Telemetry box dynamically anchored below graph legend with zero overlap
    float telemetryBoxY = std::max(680.0f, (float)y + 8.0f);
    DrawRectangleRounded(Rectangle{12.0f, telemetryBoxY, 236.0f, 92.0f}, 0.15f, 4, Color{18, 23, 32, 255});
    drawText("SIMULATION TELEMETRY:", 20, telemetryBoxY + 6.0f, 13, Color{255, 205, 110, 255});
    drawText(("- Evacuated: " + std::to_string(totalAgentsEvacuated_) + " agents").c_str(), 20, telemetryBoxY + 24.0f, 13, Color{60, 230, 140, 255});
    drawText(("- Active Streams: " + std::to_string(activeAgents_.size()) + " agents").c_str(), 20, telemetryBoxY + 42.0f, 13, Color{200, 212, 230, 255});
    drawText(("- Selected Start: Node " + std::to_string(selectedStartSpawnId_)).c_str(), 20, telemetryBoxY + 60.0f, 13, Color{100, 220, 255, 255});
}

// Side-by-side Subway Transit Line renderer for distinct parallel multi-algorithm overlay lines
void EvacPlannerApp::drawPathLine(const std::vector<int>& pathNodes, float offset, unsigned char r, unsigned char g, unsigned char b) {
    if (pathNodes.size() < 2) return;

    for (size_t i = 0; i < pathNodes.size() - 1; i++) {
        const Node* u = graph_.getNode(pathNodes[i]);
        const Node* v = graph_.getNode(pathNodes[i+1]);
        if (u && v) {
            float dx = v->pos.x - u->pos.x;
            float dy = v->pos.y - u->pos.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len == 0.0f) continue;

            float nx = -dy / len * offset;
            float ny = dx / len * offset;

            Vector2 p1{u->pos.x + nx, u->pos.y + ny};
            Vector2 p2{v->pos.x + nx, v->pos.y + ny};

            DrawLineEx(p1, p2, 4.5f, Color{r, g, b, 235});
        }
    }
}

void EvacPlannerApp::drawCanvas() {
    float timeVal = (float)GetTime();

    // Canvas background grid pattern
    for (int gx = 280; gx < 1370; gx += 40) {
        DrawLine(gx, 72, gx, 570, Color{32, 40, 54, 130});
    }
    for (int gy = 72; gy < 570; gy += 40) {
        DrawLine(280, gy, 1370, gy, Color{32, 40, 54, 130});
    }

    // Highlight selected starting node for Add Edge / Add Blockage / Delete Edge tools
    if (edgeStartNodeId_ != -1) {
        const Node* sNode = graph_.getNode(edgeStartNodeId_);
        if (sNode) {
            float pulse = 5.0f * std::sin(timeVal * 6.0f);
            DrawCircleLines((int)sNode->pos.x, (int)sNode->pos.y, 32.0f + pulse, Color{255, 180, 50, 255});
        }
    }

    // 0. Render Interactive Hazard Radius Spheres (Glowing Fire/Smoke danger zones)
    for (const auto& pair : graph_.getNodes()) {
        const Node& n = pair.second;
        if (n.type == NodeType::HazardZone) {
            Vector2 pos{n.pos.x, n.pos.y};
            float pulse = 8.0f * std::sin(timeVal * 4.0f);
            float r = n.hazardRadius + pulse;

            DrawCircleV(pos, r, Color{220, 45, 45, 38});
            DrawCircleLines((int)pos.x, (int)pos.y, r, Color{240, 65, 65, 110});
        }
    }

    // 1. Draw Base Edges with Live Congestion Thickness & Pulsating Heat Colors
    for (const auto& pair : graph_.getAdjacencyList()) {
        int uId = pair.first;
        const Node* u = graph_.getNode(uId);
        if (!u) continue;

        for (const auto& edge : pair.second) {
            const Node* v = graph_.getNode(edge.toNode);
            if (!v || uId > edge.toNode) continue;

            Vector2 p1{u->pos.x, u->pos.y};
            Vector2 p2{v->pos.x, v->pos.y};

            if (edge.isBlocked) {
                DrawLineEx(p1, p2, 4.5f, Color{90, 35, 35, 255});
            } else {
                float radiusHeat = graph_.getEdgeHazardRadiusInfluence(uId, edge.toNode);
                float totalRisk = std::min(1.0f, edge.hazardLevel + radiusHeat + edge.congestion);

                float lineThickness = 2.5f + totalRisk * 5.5f;

                Color edgeColor = Color{80, 102, 128, 255};
                if (totalRisk > 0.6f) {
                    float pulse = 0.5f + 0.5f * std::sin(timeVal * 6.0f);
                    unsigned char redVal = (unsigned char)(200 + pulse * 55);
                    edgeColor = Color{redVal, 40, 40, 255};
                } else if (totalRisk > 0.25f) {
                    edgeColor = Color{230, 125, 40, 255};
                } else if (totalRisk > 0.0f) {
                    edgeColor = Color{200, 175, 55, 255};
                }

                DrawLineEx(p1, p2, lineThickness, edgeColor);
            }
        }
    }

    // 2. Draw Algorithm Paths (Distinct Subway Map Side-by-Side Lines: Dijkstra +7px, Greedy 0px, BFS -7px)
    bool isStepPlaybackMode = isAnimating_ || (currentAnimStep_ > 0);

    if (isStepPlaybackMode) {
        auto drawStepPath = [&](const PathResult& res, Color mainColor, float offset) {
            if (currentAnimStep_ < (int)res.steps.size()) {
                const auto& step = res.steps[currentAnimStep_];

                // Visited nodes glow faintly
                for (int vId : step.visitedNodes) {
                    const Node* vNode = graph_.getNode(vId);
                    if (vNode) {
                        DrawCircleV(Vector2{vNode->pos.x, vNode->pos.y}, 26.0f, Color{mainColor.r, mainColor.g, mainColor.b, 70});
                    }
                }

                // Frontier active nodes flash
                for (int fId : step.frontierNodes) {
                    const Node* fNode = graph_.getNode(fId);
                    if (fNode) {
                        DrawCircleLines((int)fNode->pos.x, (int)fNode->pos.y, 28.0f, mainColor);
                    }
                }

                // Draw step partial path
                drawPathLine(step.currentPath, offset, mainColor.r, mainColor.g, mainColor.b);
            } else if (res.found) {
                drawPathLine(res.pathNodes, offset, mainColor.r, mainColor.g, mainColor.b);
            }
        };

        if (showDijkstra_) drawStepPath(dijkstraResult_, Color{80, 235, 90, 255}, 7.0f);
        if (showGreedy_) drawStepPath(greedyResult_, Color{210, 85, 255, 255}, 0.0f);
        if (showBFS_) drawStepPath(bfsResult_, Color{0, 210, 255, 255}, -7.0f);
    } else {
        // Static full completed subway side-by-side path overlays
        if (showDijkstra_ && dijkstraResult_.found) drawPathLine(dijkstraResult_.pathNodes, 7.0f, 80, 235, 90);
        if (showGreedy_ && greedyResult_.found) drawPathLine(greedyResult_.pathNodes, 0.0f, 210, 85, 255);
        if (showBFS_ && bfsResult_.found) drawPathLine(bfsResult_.pathNodes, -7.0f, 0, 210, 255);
    }

    // 2.5 Render Multi-Agent Evacuee Crowd Particles Moving along corridors
    if (showLiveAgentStream_) {
        for (const auto& agent : activeAgents_) {
            if (agent.currentSegmentIndex < agent.pathNodes.size() - 1) {
                int uId = agent.pathNodes[agent.currentSegmentIndex];
                int vId = agent.pathNodes[agent.currentSegmentIndex + 1];

                const Node* u = graph_.getNode(uId);
                const Node* v = graph_.getNode(vId);

                if (u && v) {
                    float dx = v->pos.x - u->pos.x;
                    float dy = v->pos.y - u->pos.y;
                    float len = std::sqrt(dx * dx + dy * dy);

                    float nx = 0.0f;
                    float ny = 0.0f;
                    if (len > 0.0f) {
                        nx = -dy / len * agent.lineOffset;
                        ny = dx / len * agent.lineOffset;
                    }

                    Vector2 basePos{
                        u->pos.x + agent.progress * (v->pos.x - u->pos.x) + nx,
                        u->pos.y + agent.progress * (v->pos.y - u->pos.y) + ny
                    };

                    // Draw glowing multi-agent crowd particle
                    DrawCircleV(basePos, 6.5f, agent.color);
                    DrawCircleLines((int)basePos.x, (int)basePos.y, 6.5f, Color{255, 255, 255, 220});
                }
            }
        }
    }

    // 3. Draw Nodes (Circles)
    for (const auto& pair : graph_.getNodes()) {
        const Node& n = pair.second;
        Vector2 pos{n.pos.x, n.pos.y};

        Color fillColor = Color{45, 125, 225, 255};
        if (n.id == selectedStartSpawnId_) fillColor = Color{240, 140, 30, 255};
        else if (n.type == NodeType::SafeZone) {
            bool isCompromised = graph_.isNodeCompromisedByHazard(n.id);
            fillColor = isCompromised ? Color{190, 45, 45, 255} : Color{40, 200, 100, 255};
        }
        else if (n.type == NodeType::HazardZone) fillColor = Color{220, 60, 60, 255};
        else if (n.type == NodeType::Blocked) fillColor = Color{65, 70, 80, 255};

        if (n.id == hoveredNodeId_) {
            DrawCircleV(pos, 27.0f, Color{255, 255, 255, 80});
        }

        DrawCircleV(pos, 22.0f, fillColor);
        DrawCircleLines((int)pos.x, (int)pos.y, 22.0f, Color{255, 255, 255, 220});
    }

    // 4. Draw Distance vs Dynamic Risk Value Pills ON TOP with POINT texture filter and integer grid alignment
    for (const auto& pair : graph_.getAdjacencyList()) {
        int uId = pair.first;
        const Node* u = graph_.getNode(uId);
        if (!u) continue;

        for (const auto& edge : pair.second) {
            const Node* v = graph_.getNode(edge.toNode);
            if (!v || uId > edge.toNode) continue;

            Vector2 p1{u->pos.x, u->pos.y};
            Vector2 p2{v->pos.x, v->pos.y};

            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len == 0.0f) continue;

            float nx = -dy / len * 18.0f;
            float ny = dx / len * 18.0f;

            Vector2 mid{(p1.x + p2.x)/2.0f + nx, (p1.y + p2.y)/2.0f + ny};

            if (edge.isBlocked) {
                float txtW = measureTextWidth("BLOCKED", 15);
                Rectangle tagRect{std::floor(mid.x - txtW/2.0f - 10.0f), std::floor(mid.y - 14.0f), txtW + 20.0f, 28.0f};
                DrawRectangleRounded(tagRect, 0.35f, 4, Color{38, 12, 12, 255});
                DrawRectangleRoundedLines(tagRect, 0.35f, 4, 1.2f, Color{240, 70, 70, 255});
                drawText("BLOCKED", mid.x - txtW/2.0f, mid.y - 8.0f, 15, Color{250, 120, 120, 255});
            } else {
                float radiusHeat = graph_.getEdgeHazardRadiusInfluence(uId, edge.toNode);
                float totalRisk = std::min(1.0f, edge.hazardLevel + radiusHeat + edge.congestion);

                std::stringstream ss;
                ss << std::fixed << std::setprecision(0) << edge.distance << "m";
                if (totalRisk > 0.2f) {
                    ss << " (+Danger)";
                } else if (totalRisk > 0.05f) {
                    ss << " (+Risk)";
                }
                std::string dStr = ss.str();

                float txtW = measureTextWidth(dStr.c_str(), 15);
                Rectangle pillRect{std::floor(mid.x - txtW/2.0f - 10.0f), std::floor(mid.y - 14.0f), txtW + 20.0f, 28.0f};

                Color pillBg = Color{15, 20, 30, 255};
                Color pillBorder = Color{70, 95, 125, 255};
                Color pillText = Color{210, 230, 255, 255};

                if (totalRisk > 0.4f) {
                    pillBg = Color{48, 14, 14, 255};
                    pillBorder = Color{240, 75, 75, 255};
                    pillText = Color{255, 130, 130, 255};
                } else if (totalRisk > 0.05f) {
                    pillBg = Color{45, 30, 12, 255};
                    pillBorder = Color{235, 145, 45, 255};
                    pillText = Color{255, 200, 110, 255};
                }

                DrawRectangleRounded(pillRect, 0.35f, 4, pillBg);
                DrawRectangleRoundedLines(pillRect, 0.35f, 4, 1.2f, pillBorder);
                drawText(dStr.c_str(), mid.x - txtW/2.0f, mid.y - 8.0f, 15, pillText);
            }
        }
    }

    // 5. Draw Node Badges ON TOP with integer grid alignment
    for (const auto& pair : graph_.getNodes()) {
        const Node& n = pair.second;
        Vector2 pos{n.pos.x, n.pos.y};

        std::string badgeText = n.name;
        bool isCompromised = (n.type == NodeType::SafeZone && graph_.isNodeCompromisedByHazard(n.id));
        if (isCompromised) {
            badgeText += " (HAZARD OVERRUN)";
        }

        float nameW = measureTextWidth(badgeText.c_str(), 18);
        Rectangle nameBadge{std::floor(pos.x - nameW/2.0f - 12.0f), std::floor(pos.y + 27.0f), nameW + 24.0f, 32.0f};

        Color badgeBg = isCompromised ? Color{48, 15, 15, 255} : Color{14, 18, 26, 255};
        Color badgeBorder = isCompromised ? Color{240, 75, 75, 255} : Color{60, 85, 115, 255};
        Color textColor = isCompromised ? Color{255, 130, 130, 255} : Color{252, 254, 255, 255};

        DrawRectangleRounded(nameBadge, 0.35f, 4, badgeBg);
        DrawRectangleRoundedLines(nameBadge, 0.35f, 4, 1.2f, badgeBorder);
        drawText(badgeText.c_str(), pos.x - nameW/2.0f, pos.y + 30.0f, 18, textColor);
    }
}

void EvacPlannerApp::drawMetricsOverlay() {
    int overlayX = 275;
    int overlayY = 575;
    int overlayW = 1085;
    int overlayH = 178;

    DrawRectangleRounded(Rectangle{(float)overlayX, (float)overlayY, (float)overlayW, (float)overlayH}, 0.08f, 4, Color{20, 26, 36, 248});
    DrawRectangleRoundedLines(Rectangle{(float)overlayX, (float)overlayY, (float)overlayW, (float)overlayH}, 0.08f, 4, 1.5f, Color{55, 72, 98, 255});

    drawText("REAL-TIME ALGORITHM PERFORMANCE BENCHMARK & THROUGHPUT ENGINE", overlayX + 16, overlayY + 10, 16, Color{100, 200, 255, 255});

    auto drawMetricCard = [&](int cardX, int cardY, const PathResult& r, Color themeColor) {
        DrawRectangleRounded(Rectangle{(float)cardX, (float)cardY, 340.0f, 130.0f}, 0.12f, 4, Color{26, 34, 48, 255});
        DrawRectangleRoundedLines(Rectangle{(float)cardX, (float)cardY, 340.0f, 130.0f}, 0.12f, 4, 1.5f, themeColor);

        drawText(r.algorithmName.c_str(), cardX + 14, cardY + 6, 16, themeColor);

        if (!r.found) {
            std::string alertText = "STATUS: NO VIABLE ROUTE";
            if (r.bottleneckStatus == "ALL EXITS ENGULFED IN HAZARD") {
                alertText = "ALERT: ALL EXIT GATES OVERRUN";
            }
            drawText(alertText.c_str(), cardX + 14, cardY + 28, 14, Color{245, 75, 75, 255});
            drawText("Survival Rate: 0.0% (Exit Overrun)", cardX + 14, cardY + 48, 14, Color{240, 70, 70, 255});
            drawText("Est. Clearance: N/A (Shelter In Place)", cardX + 14, cardY + 68, 13, Color{250, 160, 160, 255});
            
            std::stringstream s4;
            s4 << "Visited: " << r.nodesVisited << " nodes | Latency: " << std::fixed << std::setprecision(1) << r.executionMicroseconds << " us";
            drawText(s4.str().c_str(), cardX + 14, cardY + 88, 13, Color{150, 175, 205, 255});
            return;
        }

        std::stringstream s1, s2, s3;
        s1 << "Hops: " << (r.pathNodes.empty() ? 0 : r.pathNodes.size() - 1) << " | Distance: " << std::fixed << std::setprecision(0) << r.totalDistance << "m";
        s2 << "Survival Rate: " << std::fixed << std::setprecision(1) << r.survivalRatePercent << "%";
        s3 << "Est. Clearance: " << std::fixed << std::setprecision(1) << r.estimatedClearanceTimeMins << " mins (" << r.bottleneckStatus << ")";

        drawText(s1.str().c_str(), cardX + 14, cardY + 28, 14, WHITE);

        Color survivalColor = (r.survivalRatePercent > 80.0f) ? Color{60, 220, 120, 255} : (r.survivalRatePercent > 50.0f ? Color{240, 180, 40, 255} : Color{240, 70, 70, 255});
        drawText(s2.str().c_str(), cardX + 14, cardY + 48, 14, survivalColor);
        drawText(s3.str().c_str(), cardX + 14, cardY + 68, 13, Color{190, 210, 235, 255});

        std::stringstream s4;
        s4 << "Visited: " << r.nodesVisited << " nodes | Latency: " << std::fixed << std::setprecision(1) << r.executionMicroseconds << " us";
        if (currentAnimStep_ > 0) {
            s4 << " [Step " << currentAnimStep_ << "]";
        }
        drawText(s4.str().c_str(), cardX + 14, cardY + 88, 13, Color{150, 175, 205, 255});
    };

    drawMetricCard(overlayX + 15, overlayY + 38, bfsResult_, Color{0, 210, 255, 255});
    drawMetricCard(overlayX + 370, overlayY + 38, greedyResult_, Color{210, 85, 255, 255});
    drawMetricCard(overlayX + 725, overlayY + 38, dijkstraResult_, Color{80, 235, 90, 255});
}

void EvacPlannerApp::draw() {
    BeginDrawing();
    ClearBackground(Color{18, 22, 30, 255});

    drawCanvas();
    drawSidebar();
    drawHeader();
    drawMetricsOverlay();

    // Status bar at bottom
    DrawRectangle(260, 760, 1120, 80, Color{15, 18, 25, 255});
    drawText(("SYSTEM LOG: " + statusMessage_).c_str(), 275, 780, 16, Color{200, 220, 245, 255});

    EndDrawing();
}

} // namespace Evac
