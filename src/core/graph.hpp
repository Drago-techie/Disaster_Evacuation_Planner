#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <memory>

namespace Evac {

enum class NodeType {
    Normal,
    EvacueeSpawn,   // Start location of people needing evacuation
    SafeZone,       // Designated exit / emergency shelter
    HazardZone,     // High risk area (e.g. fire/smoke)
    Blocked         // Impassable node
};

struct Point2D {
    float x;
    float y;

    float distanceTo(const Point2D& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

struct Node {
    int id;
    std::string name;
    Point2D pos;
    NodeType type = NodeType::Normal;
    int capacity = 100;         // Max people throughput
    int currentOccupancy = 0;   // People present
    float customWeight = 1.0f;  // Delay factor
    float hazardRadius = 140.0f; // Radial heat danger zone for HazardZone type
};

struct Edge {
    int id;
    int fromNode;
    int toNode;
    float distance;             // Base length / distance (meters)
    float hazardLevel = 0.0f;   // 0.0 (safe) to 1.0 (extreme fire/smoke/debris)
    float congestion = 0.0f;    // 0.0 (clear) to 1.0 (jammed)
    bool isBlocked = false;     // Dynamic disaster collapse/blockage
    bool isBirectional = true;

    // Effective cost calculation incorporating dynamic distance, hazards, and congestion
    float getEffectiveCost(float extraHazardHeat = 0.0f) const {
        if (isBlocked) return 1e9f; // Impassable
        float totalHazard = std::min(1.0f, hazardLevel + extraHazardHeat);
        return distance * (1.0f + 3.5f * totalHazard) * (1.0f + 2.5f * congestion);
    }
};

class Graph {
private:
    std::unordered_map<int, Node> nodes_;
    std::unordered_map<int, std::vector<Edge>> adj_;
    int nextNodeId_ = 1;
    int nextEdgeId_ = 1;

public:
    Graph() = default;

    int addNode(const std::string& name, Point2D pos, NodeType type = NodeType::Normal) {
        int id = nextNodeId_++;
        Node node{id, name, pos, type};
        nodes_[id] = node;
        adj_[id] = {};
        return id;
    }

    void addNodeWithId(int id, const std::string& name, Point2D pos, NodeType type = NodeType::Normal) {
        Node node{id, name, pos, type};
        nodes_[id] = node;
        if (adj_.find(id) == adj_.end()) {
            adj_[id] = {};
        }
        if (id >= nextNodeId_) {
            nextNodeId_ = id + 1;
        }
    }

    int addEdge(int u, int v, float distance = -1.0f, float hazard = 0.0f, bool bidirectional = true, float congestion = 0.0f) {
        if (!hasNode(u) || !hasNode(v)) return -1;
        
        if (distance < 0.0f) {
            distance = nodes_[u].pos.distanceTo(nodes_[v].pos);
        }

        int id = nextEdgeId_++;
        Edge e1{id, u, v, distance, hazard, congestion, false, bidirectional};
        adj_[u].push_back(e1);

        if (bidirectional) {
            Edge e2{id, v, u, distance, hazard, congestion, false, bidirectional};
            adj_[v].push_back(e2);
        }

        return id;
    }

    void removeEdge(int u, int v) {
        auto removeInVec = [](std::vector<Edge>& edges, int targetV) {
            edges.erase(std::remove_if(edges.begin(), edges.end(), [targetV](const Edge& e) {
                return e.toNode == targetV;
            }), edges.end());
        };
        if (adj_.find(u) != adj_.end()) removeInVec(adj_[u], v);
        if (adj_.find(v) != adj_.end()) removeInVec(adj_[v], u);
    }

    void removeNode(int id) {
        nodes_.erase(id);
        adj_.erase(id);
        for (auto& pair : adj_) {
            auto& vec = pair.second;
            vec.erase(std::remove_if(vec.begin(), vec.end(), [id](const Edge& e) {
                return e.toNode == id;
            }), vec.end());
        }
    }

    bool hasNode(int id) const {
        return nodes_.find(id) != nodes_.end();
    }

    Node* getNode(int id) {
        auto it = nodes_.find(id);
        return (it != nodes_.end()) ? &(it->second) : nullptr;
    }

    const Node* getNode(int id) const {
        auto it = nodes_.find(id);
        return (it != nodes_.end()) ? &(it->second) : nullptr;
    }

    const std::unordered_map<int, Node>& getNodes() const { return nodes_; }
    const std::unordered_map<int, std::vector<Edge>>& getAdjacencyList() const { return adj_; }

    std::vector<Edge*> getOutEdges(int nodeId) {
        std::vector<Edge*> result;
        auto it = adj_.find(nodeId);
        if (it != adj_.end()) {
            for (auto& edge : it->second) {
                result.push_back(&edge);
            }
        }
        return result;
    }

    const std::vector<Edge>& getEdgesFrom(int nodeId) const {
        static const std::vector<Edge> empty;
        auto it = adj_.find(nodeId);
        return (it != adj_.end()) ? it->second : empty;
    }

    // Calculates extra hazard penalty on an edge mid-point due to proximity to any active Hazard Zone radius
    float getEdgeHazardRadiusInfluence(int uId, int vId) const {
        const Node* u = getNode(uId);
        const Node* v = getNode(vId);
        if (!u || !v) return 0.0f;

        Point2D mid{(u->pos.x + v->pos.x) / 2.0f, (u->pos.y + v->pos.y) / 2.0f};

        float maxHeat = 0.0f;
        for (const auto& pair : nodes_) {
            const Node& hz = pair.second;
            if (hz.type == NodeType::HazardZone) {
                float dist = mid.distanceTo(hz.pos);
                if (dist < hz.hazardRadius) {
                    float heat = (1.0f - dist / hz.hazardRadius) * 0.85f;
                    if (heat > maxHeat) maxHeat = heat;
                }
            }
        }
        return maxHeat;
    }

    void toggleEdgeBlock(int u, int v) {
        auto toggleInVec = [](std::vector<Edge>& edges, int targetV) {
            for (auto& e : edges) {
                if (e.toNode == targetV) {
                    e.isBlocked = !e.isBlocked;
                }
            }
        };
        if (adj_.find(u) != adj_.end()) toggleInVec(adj_[u], v);
        if (adj_.find(v) != adj_.end()) toggleInVec(adj_[v], u);
    }

    void setEdgeCongestion(int u, int v, float congestion) {
        auto setInVec = [congestion](std::vector<Edge>& edges, int targetV) {
            for (auto& e : edges) {
                if (e.toNode == targetV) {
                    e.congestion = congestion;
                }
            }
        };
        if (adj_.find(u) != adj_.end()) setInVec(adj_[u], v);
        if (adj_.find(v) != adj_.end()) setInVec(adj_[v], u);
    }

    void setEdgeHazard(int u, int v, float hazard) {
        auto setInVec = [hazard](std::vector<Edge>& edges, int targetV) {
            for (auto& e : edges) {
                if (e.toNode == targetV) {
                    e.hazardLevel = hazard;
                }
            }
        };
        if (adj_.find(u) != adj_.end()) setInVec(adj_[u], v);
        if (adj_.find(v) != adj_.end()) setInVec(adj_[v], u);
    }

    std::vector<int> getSafeZoneIds() const {
        std::vector<int> safeZones;
        for (const auto& pair : nodes_) {
            if (pair.second.type == NodeType::SafeZone) {
                safeZones.push_back(pair.first);
            }
        }
        return safeZones;
    }

    std::vector<int> getSpawnIds() const {
        std::vector<int> spawns;
        for (const auto& pair : nodes_) {
            if (pair.second.type == NodeType::EvacueeSpawn) {
                spawns.push_back(pair.first);
            }
        }
        return spawns;
    }

    void clear() {
        nodes_.clear();
        adj_.clear();
        nextNodeId_ = 1;
        nextEdgeId_ = 1;
    }
};

} // namespace Evac

#endif // GRAPH_HPP
