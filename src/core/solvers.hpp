#ifndef SOLVERS_HPP
#define SOLVERS_HPP

#include "graph.hpp"
#include <vector>
#include <string>
#include <chrono>

namespace Evac {

struct SearchStep {
    int currentNodeId = -1;
    std::vector<int> visitedNodes;
    std::vector<int> frontierNodes;
    std::vector<int> currentPath;
};

struct PathResult {
    std::string algorithmName;
    bool found = false;
    std::vector<int> pathNodes;
    float totalDistance = 0.0f;
    float totalCost = 0.0f;
    float avgHazardLevel = 0.0f;
    int nodesVisited = 0;
    double executionMicroseconds = 0.0;

    // Throughput & Survival Analytics
    float survivalRatePercent = 100.0f;
    float estimatedClearanceTimeMins = 0.0f;
    std::string bottleneckStatus = "Optimal Flow";

    // Step-by-Step Traversal Recording
    std::vector<SearchStep> steps;
};

class EvacSolver {
public:
    // Compute unweighted Hop Shortest Path via BFS avoiding blocked edges & hazard radii
    static PathResult solveBFS(const Graph& graph, int startNodeId);

    // Compute Greedy Best-First Search using Euclidean distance to nearest safe zone & risk factor
    static PathResult solveGreedy(const Graph& graph, int startNodeId);

    // Compute Dijkstra Optimal Weighted Safety Path (incorporating distance, congestion & spreading hazard heat)
    static PathResult solveDijkstra(const Graph& graph, int startNodeId);

private:
    static float minHeuristicDistance(const Graph& graph, int nodeId, const std::vector<int>& safeZones);
    static void computeAnalytics(const Graph& graph, PathResult& res);
};

} // namespace Evac

#endif // SOLVERS_HPP
