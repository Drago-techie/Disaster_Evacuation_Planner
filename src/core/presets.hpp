#ifndef PRESETS_HPP
#define PRESETS_HPP

#include "graph.hpp"

namespace Evac {

class PresetScenarios {
public:
    static void loadHighRiseFloorPlan(Graph& g) {
        g.clear();
        // Upper floor offices & labs
        int n1 = g.addNode("Office 101", {360.0f, 130.0f}, NodeType::EvacueeSpawn);
        int n2 = g.addNode("Office 102", {580.0f, 130.0f}, NodeType::EvacueeSpawn);
        int n3 = g.addNode("Conference A", {800.0f, 130.0f}, NodeType::Normal);
        int n4 = g.addNode("Lab 105", {1020.0f, 130.0f}, NodeType::EvacueeSpawn);

        // Central Corridors & Atrium
        int c1 = g.addNode("Corridor West", {450.0f, 260.0f}, NodeType::Normal);
        int c2 = g.addNode("Central Atrium", {690.0f, 260.0f}, NodeType::HazardZone); // Smoke hazard
        int c3 = g.addNode("Corridor East", {930.0f, 260.0f}, NodeType::Normal);

        // Lower Stairwells
        int s1 = g.addNode("West Stairwell", {420.0f, 400.0f}, NodeType::Normal);
        int s2 = g.addNode("East Stairwell", {960.0f, 400.0f}, NodeType::Normal);

        // Safe Exit Gates
        int e1 = g.addNode("Main Exit Gate A", {420.0f, 510.0f}, NodeType::SafeZone);
        int e2 = g.addNode("Emergency Fire Exit B", {960.0f, 510.0f}, NodeType::SafeZone);

        // Corridors & Edges
        g.addEdge(n1, c1, 15.0f);
        g.addEdge(n2, c1, 12.0f);
        g.addEdge(n2, c2, 18.0f, 0.4f);
        g.addEdge(n3, c2, 10.0f, 0.5f);
        g.addEdge(n4, c3, 14.0f);

        g.addEdge(c1, c2, 25.0f, 0.6f);
        g.addEdge(c2, c3, 25.0f, 0.6f);

        g.addEdge(c1, s1, 20.0f);
        g.addEdge(c3, s2, 20.0f);

        g.addEdge(s1, e1, 15.0f);
        g.addEdge(s2, e2, 15.0f);

        g.addEdge(s1, s2, 50.0f, 0.1f);
    }

    static void loadCityFlashFlood(Graph& g) {
        g.clear();
        // City District suffering severe flash flooding & river overflow
        
        // Row 1: High Ground & Relief Shelters
        int a1 = g.addNode("Residential Sector A", {350.0f, 130.0f}, NodeType::EvacueeSpawn);
        int a2 = g.addNode("North Bridge Plaza", {600.0f, 130.0f}, NodeType::Normal);
        int a3 = g.addNode("Central Square", {850.0f, 130.0f}, NodeType::Normal);
        int a4 = g.addNode("Downtown Heights", {1100.0f, 130.0f}, NodeType::EvacueeSpawn);

        // Row 2: Inundated Low-Lying Streets & Flood Hazards
        int b1 = g.addNode("West Boulevard", {350.0f, 310.0f}, NodeType::Normal);
        int b2 = g.addNode("River Overflow Surge", {600.0f, 310.0f}, NodeType::HazardZone);
        int b3 = g.addNode("Subway Flood Point", {850.0f, 310.0f}, NodeType::HazardZone);
        int b4 = g.addNode("East Underpass", {1100.0f, 310.0f}, NodeType::Normal);

        // Row 3: Elevated Safe Zones & High Relief Centers
        int c1 = g.addNode("Hilltop Relief Gate 1", {350.0f, 490.0f}, NodeType::SafeZone);
        int c2 = g.addNode("Commercial Highway", {600.0f, 490.0f}, NodeType::Normal);
        int c3 = g.addNode("East Ridge Pass", {850.0f, 490.0f}, NodeType::Normal);
        int c4 = g.addNode("Evacuation Center 2", {1100.0f, 490.0f}, NodeType::SafeZone);

        // Grid road connections with flood weights
        g.addEdge(a1, a2, 120.0f); g.addEdge(a2, a3, 140.0f); g.addEdge(a3, a4, 130.0f);
        g.addEdge(b1, b2, 110.0f, 0.8f); g.addEdge(b2, b3, 90.0f, 0.95f); g.addEdge(b3, b4, 110.0f, 0.5f);
        g.addEdge(c1, c2, 150.0f); g.addEdge(c2, c3, 160.0f); g.addEdge(c3, c4, 140.0f);

        // Vertical avenues
        g.addEdge(a1, b1, 100.0f); g.addEdge(b1, c1, 120.0f);
        g.addEdge(a2, b2, 100.0f, 0.7f); g.addEdge(b2, c2, 110.0f, 0.75f);
        g.addEdge(a3, b3, 100.0f, 0.85f); g.addEdge(b3, c3, 110.0f, 0.4f);
        g.addEdge(a4, b4, 100.0f); g.addEdge(b4, c4, 120.0f);

        // Simulate flooded impassable bridge between B2 and B3
        g.toggleEdgeBlock(b2, b3);
    }

    static void loadStadiumEvacuation(Graph& g) {
        g.clear();
        // Massive Arena & Stadium Evacuation Layout
        
        // Arena Floor & Standing Pit Spawns
        int arena = g.addNode("Arena Field Ground", {725.0f, 260.0f}, NodeType::EvacueeSpawn);
        int vip = g.addNode("VIP Suite Tier 3", {380.0f, 150.0f}, NodeType::EvacueeSpawn);
        int press = g.addNode("Press Box North", {1070.0f, 150.0f}, NodeType::EvacueeSpawn);

        // Inner Stadium Ring Concourse
        int concNorth = g.addNode("Concourse North Gate", {725.0f, 140.0f}, NodeType::Normal);
        int concWest  = g.addNode("West Crowd Bottleneck", {480.0f, 260.0f}, NodeType::HazardZone); // High congestion
        int concEast  = g.addNode("East Ramp Hazard", {970.0f, 260.0f}, NodeType::HazardZone); // Fire alarm zone
        int concSouth = g.addNode("Concourse South Gate", {725.0f, 380.0f}, NodeType::Normal);

        // External Perimeter Ramps
        int rampWest = g.addNode("Ramp West Exit", {380.0f, 380.0f}, NodeType::Normal);
        int rampEast = g.addNode("Ramp East Exit", {1070.0f, 380.0f}, NodeType::Normal);

        // Exterior Assembly Safe Zones
        int safeNorth = g.addNode("North Plaza Shelter", {725.0f, 510.0f}, NodeType::SafeZone);
        int safeWest  = g.addNode("West Parking Safe Zone", {380.0f, 510.0f}, NodeType::SafeZone);
        int safeEast  = g.addNode("East Transit Hub", {1070.0f, 510.0f}, NodeType::SafeZone);

        // Arena field exits to concourse ring
        g.addEdge(arena, concNorth, 45.0f);
        g.addEdge(arena, concWest,  40.0f, 0.6f);
        g.addEdge(arena, concEast,  40.0f, 0.7f);
        g.addEdge(arena, concSouth, 45.0f);

        // Tiered seating corridors
        g.addEdge(vip, concWest, 30.0f);
        g.addEdge(vip, concNorth, 35.0f);
        g.addEdge(press, concEast, 30.0f);
        g.addEdge(press, concNorth, 35.0f);

        // Outer Concourse ring corridors
        g.addEdge(concNorth, concWest,  65.0f, 0.4f);
        g.addEdge(concNorth, concEast,  65.0f, 0.5f);
        g.addEdge(concWest,  concSouth, 60.0f, 0.7f);
        g.addEdge(concEast,  concSouth, 60.0f, 0.8f);

        // Concourse to External Ramps
        g.addEdge(concWest, rampWest, 35.0f);
        g.addEdge(concEast, rampEast, 35.0f);
        g.addEdge(concSouth, safeNorth, 50.0f);

        // Ramp connections to safe exits
        g.addEdge(rampWest, safeWest, 30.0f);
        g.addEdge(rampEast, safeEast, 30.0f);

        // Block east ramp due to structural hazard
        g.toggleEdgeBlock(concEast, rampEast);
    }
};

} // namespace Evac

#endif // PRESETS_HPP
