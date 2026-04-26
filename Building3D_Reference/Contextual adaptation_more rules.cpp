#define _MAIN_
#ifdef _MAIN_

#include "main.h"

// zSpace Core Headers
#include <headers/zApp/include/zObjects.h>
#include <headers/zApp/include/zFnSets.h>
#include <headers/zApp/include/zViewer.h>

// Standard Library Includes
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <map>
#include <functional>

using namespace zSpace;
using namespace std;

// Conversion helpers
Alice::vec zVecToAliceVec(zVector& in) {
    return Alice::vec(in.x, in.y, in.z);
}

zVector AliceVecToZvec(Alice::vec& in) {
    return zVector(in.x, in.y, in.z);
}

// ===================== PROGRAM TRANSITION SYSTEM =====================

// Enumeration for urban contexts
enum UrbanContext {
    HIGH_DENSITY_MIXED_USE,
    LOW_DENSITY_RESIDENTIAL,
    COMMERCIAL_CORE,
    CULTURAL_DISTRICT,
    TRANSIT_ORIENTED
};

// Context names for display
const vector<string> contextNames = {
    "High Density Mixed Use",
    "Low Density Residential",
    "Commercial Core",
    "Cultural District",
    "Transit Oriented"
};

// Enumeration for voxel types
enum VoxelType {
    VISIT,      // 12x8x6m public spaces
    WORK,       // 8x8x4m office spaces
    LIVE,       // 4x4x6m residential spaces
    TRANSITION, // Variable dimensions
    CIRCULATION,
    GREEN,
    NONE
};

// Enumeration for transition types
enum TransitionType {
    VERTICAL_DIRECT,      // Directly above/below
    VERTICAL_OFFSET,      // Above/below but offset
    HORIZONTAL_FACE,      // Sharing a full face
    HORIZONTAL_EDGE,      // Sharing only an edge
    DIAGONAL_VERTEX,      // Sharing only a vertex
    PROXIMITY_BASED       // Within influence radius
};

// Enumeration for transition programs
enum TransitionProgram {
    SKY_LOBBY,
    AMENITY_FLOOR,
    BUFFER_ZONE,
    SHARED_TERRACE,
    CIRCULATION_CORE,
    TRANSITION_VOID,
    MEZZANINE,
    SERVICE_FLOOR,
    PROGRAM_BRIDGE,
    // New transition types
    RETAIL_PODIUM,
    COMMUNITY_GARDEN,
    SKY_BRIDGE,
    TRANSIT_HUB,
    EXHIBITION_SPACE,
    COWORKING_PLAZA,
    GREEN_ROOF,
    PARKING_DECK
};

// Voxel structure
struct Voxel {
    VoxelType type;
    TransitionProgram program; // Only used if type == TRANSITION
    Alice::vec position;
    Alice::vec dimensions; // x, y, z dimensions
    int level;
    int id;
    vector<string> features;
    float structuralSupport = 1.0f;

    // Helper functions
    Alice::vec getCenter() const {
        Alice::vec center;
        center.x = position.x + dimensions.x / 2;
        center.y = position.y + dimensions.y / 2;
        center.z = position.z + dimensions.z / 2;
        return center;
    }

    float getVolume() const {
        return dimensions.x * dimensions.y * dimensions.z;
    }

    // Get world-space bounding box
    pair<Alice::vec, Alice::vec> getBoundingBox() const {
        Alice::vec min_pt = position;
        Alice::vec max_pt;
        max_pt.x = position.x + dimensions.x;
        max_pt.y = position.y + dimensions.y;
        max_pt.z = position.z + dimensions.z;
        return make_pair(min_pt, max_pt);
    }
};

// Program compatibility structure
struct ProgramCompatibility {
    VoxelType from;
    VoxelType to;
    TransitionType transitionType;
    vector<TransitionProgram> possibleTransitions;
    float compatibilityScore;
    bool requiresBuffer;
};

// Transition opportunity structure
struct TransitionOpportunity {
    Voxel* voxelA;
    Voxel* voxelB;
    TransitionType transitionType;
    float score;
};

// ==================== TRANSITION GENERATOR CLASS ====================
class TransitionGenerator {
private:
    // Transition rules
    struct TransitionRule {
        string ruleName;
        function<bool(const Voxel&, const Voxel&, TransitionType)> condition;
        function<Voxel(const Voxel&, const Voxel&, TransitionType)> generator;
        int basePriority;
        int currentPriority;
    };

    vector<TransitionRule> transitionRules;
    vector<ProgramCompatibility> compatibilityMatrix;

    // Helper functions
    Alice::vec calculateMidpoint(Alice::vec a, Alice::vec b) {
        Alice::vec mid;
        mid.x = (a.x + b.x) * 0.5f;
        mid.y = (a.y + b.y) * 0.5f;
        mid.z = (a.z + b.z) * 0.5f;
        return mid;
    }

    bool hasAdjacentVoxels(const Voxel& voxel, VoxelType type, int count) {
        // This would check neighboring voxels in actual implementation
        return true; // Simplified for now
    }

    float calculateOverlapRatio(const Voxel& a, const Voxel& b) {
        // Calculate how much two voxels overlap
        pair<Alice::vec, Alice::vec> boundsA = a.getBoundingBox();
        pair<Alice::vec, Alice::vec> boundsB = b.getBoundingBox();

        Alice::vec minA = boundsA.first;
        Alice::vec maxA = boundsA.second;
        Alice::vec minB = boundsB.first;
        Alice::vec maxB = boundsB.second;

        // Fixed: Ensure both arguments have the same type (float)
        float overlapX = std::max(0.0f, static_cast<float>(std::min(maxA.x, maxB.x) - std::max(minA.x, minB.x)));
        float overlapZ = std::max(0.0f, static_cast<float>(std::min(maxA.z, maxB.z) - std::max(minA.z, minB.z)));

        float overlapArea = overlapX * overlapZ;
        float minArea = std::min(a.dimensions.x * a.dimensions.z, b.dimensions.x * b.dimensions.z);

        return minArea > 0 ? overlapArea / minArea : 0.0f;
    }

public:
    TransitionGenerator() {
        initializeCompatibilityMatrix();
        initializeRules();
    }

    void updatePrioritiesForContext(UrbanContext context) {
        // Update priorities based on selected context with WIDER GAPS
        switch (context) {
        case HIGH_DENSITY_MIXED_USE:
            // Dense mixed-use: strong vertical emphasis, minimal gardens
            for (auto& rule : transitionRules) {
                if (rule.ruleName == "SkyLobbyRule") rule.currentPriority = 100;
                else if (rule.ruleName == "AmenityFloorRule") rule.currentPriority = 95;
                else if (rule.ruleName == "SharedTerraceRule") rule.currentPriority = 50;
                else if (rule.ruleName == "ServiceFloorRule") rule.currentPriority = 85;
                else if (rule.ruleName == "RetailPodiumRule") rule.currentPriority = 90;
                else if (rule.ruleName == "CommunityGardenRule") rule.currentPriority = 10;  // Very low
                else if (rule.ruleName == "SkyBridgeRule") rule.currentPriority = 85;
                else if (rule.ruleName == "TransitHubRule") rule.currentPriority = 80;
                else if (rule.ruleName == "ExhibitionSpaceRule") rule.currentPriority = 45;
                else if (rule.ruleName == "CoworkingPlazaRule") rule.currentPriority = 75;
            }
            break;

        case LOW_DENSITY_RESIDENTIAL:
            // Residential: gardens and terraces only, no commercial/transit
            for (auto& rule : transitionRules) {
                if (rule.ruleName == "SkyLobbyRule") rule.currentPriority = 5;       // Almost never
                else if (rule.ruleName == "AmenityFloorRule") rule.currentPriority = 60;
                else if (rule.ruleName == "SharedTerraceRule") rule.currentPriority = 100;
                else if (rule.ruleName == "ServiceFloorRule") rule.currentPriority = 40;
                else if (rule.ruleName == "RetailPodiumRule") rule.currentPriority = 10;   // Very rare
                else if (rule.ruleName == "CommunityGardenRule") rule.currentPriority = 95;
                else if (rule.ruleName == "SkyBridgeRule") rule.currentPriority = 5;       // Almost never
                else if (rule.ruleName == "TransitHubRule") rule.currentPriority = 5;      // Almost never
                else if (rule.ruleName == "ExhibitionSpaceRule") rule.currentPriority = 50;
                else if (rule.ruleName == "CoworkingPlazaRule") rule.currentPriority = 10; // Very rare
            }
            break;

        case COMMERCIAL_CORE:
            // Commercial: all business, no residential amenities
            for (auto& rule : transitionRules) {
                if (rule.ruleName == "SkyLobbyRule") rule.currentPriority = 100;
                else if (rule.ruleName == "AmenityFloorRule") rule.currentPriority = 65;
                else if (rule.ruleName == "SharedTerraceRule") rule.currentPriority = 25;
                else if (rule.ruleName == "ServiceFloorRule") rule.currentPriority = 95;
                else if (rule.ruleName == "RetailPodiumRule") rule.currentPriority = 85;
                else if (rule.ruleName == "CommunityGardenRule") rule.currentPriority = 5;  // Almost never
                else if (rule.ruleName == "SkyBridgeRule") rule.currentPriority = 90;
                else if (rule.ruleName == "TransitHubRule") rule.currentPriority = 88;
                else if (rule.ruleName == "ExhibitionSpaceRule") rule.currentPriority = 15; // Very rare
                else if (rule.ruleName == "CoworkingPlazaRule") rule.currentPriority = 100;
            }
            break;

        case CULTURAL_DISTRICT:
            // Cultural: public spaces and exhibitions dominate
            for (auto& rule : transitionRules) {
                if (rule.ruleName == "SkyLobbyRule") rule.currentPriority = 70;
                else if (rule.ruleName == "AmenityFloorRule") rule.currentPriority = 80;
                else if (rule.ruleName == "SharedTerraceRule") rule.currentPriority = 85;
                else if (rule.ruleName == "ServiceFloorRule") rule.currentPriority = 60;
                else if (rule.ruleName == "RetailPodiumRule") rule.currentPriority = 75;
                else if (rule.ruleName == "CommunityGardenRule") rule.currentPriority = 75;
                else if (rule.ruleName == "SkyBridgeRule") rule.currentPriority = 40;
                else if (rule.ruleName == "TransitHubRule") rule.currentPriority = 50;
                else if (rule.ruleName == "ExhibitionSpaceRule") rule.currentPriority = 100;
                else if (rule.ruleName == "CoworkingPlazaRule") rule.currentPriority = 25;  // Less commercial
            }
            break;

        case TRANSIT_ORIENTED:
            // Transit: connectivity dominates, minimal leisure
            for (auto& rule : transitionRules) {
                if (rule.ruleName == "SkyLobbyRule") rule.currentPriority = 85;
                else if (rule.ruleName == "AmenityFloorRule") rule.currentPriority = 60;
                else if (rule.ruleName == "SharedTerraceRule") rule.currentPriority = 30;
                else if (rule.ruleName == "ServiceFloorRule") rule.currentPriority = 90;
                else if (rule.ruleName == "RetailPodiumRule") rule.currentPriority = 88;
                else if (rule.ruleName == "CommunityGardenRule") rule.currentPriority = 10;  // Very rare
                else if (rule.ruleName == "SkyBridgeRule") rule.currentPriority = 95;
                else if (rule.ruleName == "TransitHubRule") rule.currentPriority = 100;
                else if (rule.ruleName == "ExhibitionSpaceRule") rule.currentPriority = 20;  // Rare
                else if (rule.ruleName == "CoworkingPlazaRule") rule.currentPriority = 75;
            }
            break;
        }

        cout << "\nUpdated priorities for context: " << contextNames[context] << endl;
        cout << "Priority range: ";

        // Find min and max priorities
        int minPriority = 100;
        int maxPriority = 0;
        for (const auto& rule : transitionRules) {
            minPriority = min(minPriority, rule.currentPriority);
            maxPriority = max(maxPriority, rule.currentPriority);
        }
        cout << minPriority << " to " << maxPriority << " (gap: " << maxPriority - minPriority << ")" << endl;

        cout << "\nDetailed priorities:" << endl;
        // Sort by priority for display
        vector<pair<string, int>> sortedPriorities;
        for (const auto& rule : transitionRules) {
            sortedPriorities.push_back({ rule.ruleName, rule.currentPriority });
        }
        sort(sortedPriorities.begin(), sortedPriorities.end(),
            [](const pair<string, int>& a, const pair<string, int>& b) {
                return a.second > b.second;
            });

        for (const auto& p : sortedPriorities) {
            cout << "  " << p.first << ": " << p.second << endl;
        }
    }

    void initializeCompatibilityMatrix() {
        compatibilityMatrix = {
            // Visit to Work transitions
            {VISIT, WORK, VERTICAL_DIRECT, {MEZZANINE, CIRCULATION_CORE}, 0.9f, false},
            {VISIT, WORK, HORIZONTAL_FACE, {CIRCULATION_CORE}, 0.8f, false},

            // Work to Live transitions  
            {WORK, LIVE, VERTICAL_DIRECT, {SKY_LOBBY, AMENITY_FLOOR}, 0.7f, true},
            {WORK, LIVE, HORIZONTAL_FACE, {BUFFER_ZONE}, 0.3f, true},

            // Visit to Live transitions
            {VISIT, LIVE, VERTICAL_DIRECT, {SERVICE_FLOOR, BUFFER_ZONE}, 0.5f, true},
            {VISIT, LIVE, HORIZONTAL_FACE, {BUFFER_ZONE}, 0.1f, true},

            // Same type transitions
            {WORK, WORK, HORIZONTAL_FACE, {SHARED_TERRACE, CIRCULATION_CORE}, 1.0f, false},
            {LIVE, LIVE, HORIZONTAL_FACE, {SHARED_TERRACE}, 1.0f, false},
            {VISIT, VISIT, HORIZONTAL_FACE, {CIRCULATION_CORE}, 1.0f, false}
        };
    }

    void initializeRules() {
        // Rule 1: Sky Lobby Creation
        transitionRules.push_back({
            "SkyLobbyRule",
            [](const Voxel& below, const Voxel& above, TransitionType type) {
                return below.type == WORK && above.type == LIVE &&
                       type == VERTICAL_DIRECT;
            },
            [this](const Voxel& below, const Voxel& above, TransitionType type) {
                Voxel skyLobby;
                skyLobby.type = TRANSITION;
                skyLobby.program = SKY_LOBBY;
                skyLobby.position = above.position;
                skyLobby.position.y = below.position.y + below.dimensions.y;
                skyLobby.dimensions = Alice::vec(12, 8, 8);
                skyLobby.level = below.level + 1;
                skyLobby.features = {"panoramic_views", "lounge_area", "elevator_lobby"};
                return skyLobby;
            },
            100, 100
            });

        // Rule 2: Amenity Floor Creation
        transitionRules.push_back({
            "AmenityFloorRule",
            [this](const Voxel& below, const Voxel& above, TransitionType type) {
                return below.type == WORK && above.type == LIVE &&
                       type == VERTICAL_DIRECT;
            },
            [this](const Voxel& below, const Voxel& above, TransitionType type) {
                Voxel amenityFloor;
                amenityFloor.type = TRANSITION;
                amenityFloor.program = AMENITY_FLOOR;
                amenityFloor.position = above.position;
                amenityFloor.position.y = below.position.y + below.dimensions.y;
                amenityFloor.dimensions = Alice::vec(16, 6, 12);
                amenityFloor.level = below.level + 1;
                amenityFloor.features = {"gym", "coworking_space", "outdoor_terrace"};
                return amenityFloor;
            },
            90, 90
            });

        // Rule 3: Shared Terrace for Horizontal Connections
        transitionRules.push_back({
            "SharedTerraceRule",
            [](const Voxel& voxelA, const Voxel& voxelB, TransitionType type) {
                return type == HORIZONTAL_FACE &&
                       voxelA.type == voxelB.type && voxelA.level == voxelB.level;
            },
            [this](const Voxel& voxelA, const Voxel& voxelB, TransitionType type) {
                Voxel terrace;
                terrace.type = TRANSITION;
                terrace.program = SHARED_TERRACE;
                terrace.position = calculateMidpoint(voxelA.position, voxelB.position);
                terrace.dimensions = Alice::vec(4, 2, 4);
                terrace.level = voxelA.level;
                terrace.features = {"green_roof", "seating_area", "pergola"};
                return terrace;
            },
            70, 70
            });

        // Rule 4: Service Floor Between Visit and Work
        transitionRules.push_back({
            "ServiceFloorRule",
            [](const Voxel& below, const Voxel& above, TransitionType type) {
                return below.type == VISIT && above.type == WORK &&
                       type == VERTICAL_DIRECT;
            },
            [this](const Voxel& below, const Voxel& above, TransitionType type) {
                Voxel serviceFloor;
                serviceFloor.type = TRANSITION;
                serviceFloor.program = SERVICE_FLOOR;
                serviceFloor.position = above.position;
                serviceFloor.position.y = below.position.y + below.dimensions.y;
                serviceFloor.dimensions = Alice::vec(10, 3, 8);
                serviceFloor.level = below.level + 1;
                serviceFloor.features = {"mailroom", "security_desk", "package_storage"};
                return serviceFloor;
            },
            85, 85
            });

        // Rule 5: Retail Podium - Between Visit and Work
        transitionRules.push_back({
            "RetailPodiumRule",
            [](const Voxel& below, const Voxel& above, TransitionType type) {
                return below.type == VISIT && above.type == WORK &&
                       type == VERTICAL_DIRECT && below.level == 0;
            },
            [this](const Voxel& below, const Voxel& above, TransitionType type) {
                Voxel retailPodium;
                retailPodium.type = TRANSITION;
                retailPodium.program = RETAIL_PODIUM;
                retailPodium.position = above.position;
                retailPodium.position.y = below.position.y + below.dimensions.y;
                retailPodium.dimensions = Alice::vec(14, 4, 10);
                retailPodium.level = below.level + 1;
                retailPodium.features = {"shops", "restaurants", "public_atrium"};
                return retailPodium;
            },
            80, 80
            });

        // Rule 6: Community Garden - Between Live voxels
        transitionRules.push_back({
            "CommunityGardenRule",
            [](const Voxel& voxelA, const Voxel& voxelB, TransitionType type) {
                return type == HORIZONTAL_FACE &&
                       voxelA.type == LIVE && voxelB.type == LIVE;
            },
            [this](const Voxel& voxelA, const Voxel& voxelB, TransitionType type) {
                Voxel garden;
                garden.type = TRANSITION;
                garden.program = COMMUNITY_GARDEN;
                garden.position = calculateMidpoint(voxelA.position, voxelB.position);
                garden.dimensions = Alice::vec(6, 1, 6);
                garden.level = voxelA.level;
                garden.features = {"planters", "seating", "pergolas", "water_feature"};
                return garden;
            },
            75, 75
            });

        // Rule 7: Sky Bridge - Between Work voxels at height
        transitionRules.push_back({
            "SkyBridgeRule",
            [](const Voxel& voxelA, const Voxel& voxelB, TransitionType type) {
                return type == HORIZONTAL_FACE &&
                       voxelA.type == WORK && voxelB.type == WORK &&
                       voxelA.level > 3;
            },
            [this](const Voxel& voxelA, const Voxel& voxelB, TransitionType type) {
                Voxel bridge;
                bridge.type = TRANSITION;
                bridge.program = SKY_BRIDGE;
                bridge.position = calculateMidpoint(voxelA.position, voxelB.position);
                bridge.dimensions = Alice::vec(8, 3, 4);
                bridge.level = voxelA.level;
                bridge.features = {"enclosed_walkway", "viewing_panels", "climate_control"};
                return bridge;
            },
            65, 65
            });

        // Rule 8: Transit Hub - At ground level
        transitionRules.push_back({
            "TransitHubRule",
            [](const Voxel& below, const Voxel& above, TransitionType type) {
                return below.type == VISIT && above.type == WORK &&
                       type == VERTICAL_DIRECT && below.level == 0;
            },
            [this](const Voxel& below, const Voxel& above, TransitionType type) {
                Voxel transitHub;
                transitHub.type = TRANSITION;
                transitHub.program = TRANSIT_HUB;
                transitHub.position = above.position;
                transitHub.position.y = below.position.y + below.dimensions.y;
                transitHub.dimensions = Alice::vec(12, 5, 12);
                transitHub.level = below.level + 1;
                transitHub.features = {"ticket_machines", "waiting_area", "info_desk", "bike_storage"};
                return transitHub;
            },
            95, 95
            });

        // Rule 9: Exhibition Space - Between Visit and Live
        transitionRules.push_back({
            "ExhibitionSpaceRule",
            [](const Voxel& below, const Voxel& above, TransitionType type) {
                return below.type == VISIT && above.type == LIVE &&
                       type == VERTICAL_DIRECT;
            },
            [this](const Voxel& below, const Voxel& above, TransitionType type) {
                Voxel exhibition;
                exhibition.type = TRANSITION;
                exhibition.program = EXHIBITION_SPACE;
                exhibition.position = above.position;
                exhibition.position.y = below.position.y + below.dimensions.y;
                exhibition.dimensions = Alice::vec(14, 6, 10);
                exhibition.level = below.level + 1;
                exhibition.features = {"gallery_walls", "lighting_track", "reception_desk"};
                return exhibition;
            },
            85, 85
            });

        // Rule 10: Coworking Plaza - Between Work voxels
        transitionRules.push_back({
            "CoworkingPlazaRule",
            [](const Voxel& voxelA, const Voxel& voxelB, TransitionType type) {
                return type == HORIZONTAL_FACE &&
                       voxelA.type == WORK && voxelB.type == WORK;
            },
            [this](const Voxel& voxelA, const Voxel& voxelB, TransitionType type) {
                Voxel plaza;
                plaza.type = TRANSITION;
                plaza.program = COWORKING_PLAZA;
                plaza.position = calculateMidpoint(voxelA.position, voxelB.position);
                plaza.dimensions = Alice::vec(8, 3, 6);
                plaza.level = voxelA.level;
                plaza.features = {"hot_desks", "meeting_pods", "coffee_bar"};
                return plaza;
            },
            70, 70
            });
    }

    // Generate transition based on two voxels
    Voxel generateTransition(const Voxel& voxelA, const Voxel& voxelB, TransitionType type) {
        // Sort rules by CURRENT priority
        sort(transitionRules.begin(), transitionRules.end(),
            [](const TransitionRule& a, const TransitionRule& b) {
                return a.currentPriority > b.currentPriority;
            });

        // Try each rule
        for (const auto& rule : transitionRules) {
            if (rule.condition(voxelA, voxelB, type)) {
                return rule.generator(voxelA, voxelB, type);
            }
        }

        // No rule matched - return invalid transition
        Voxel invalidTransition;
        invalidTransition.type = NONE; // Mark as invalid
        return invalidTransition;
    }

    // Create default transition when no specific rule applies
    Voxel createDefaultTransition(const Voxel& voxelA, const Voxel& voxelB, TransitionType type) {
        Voxel transition;
        transition.type = TRANSITION;
        transition.program = TRANSITION_VOID;
        transition.position = calculateMidpoint(voxelA.position, voxelB.position);
        transition.dimensions = Alice::vec(4, 3, 4);
        transition.level = std::max(voxelA.level, voxelB.level);
        return transition;
    }

    // Get compatibility score between two voxel types
    float getCompatibilityScore(VoxelType from, VoxelType to, TransitionType transType) {
        for (const auto& comp : compatibilityMatrix) {
            if (comp.from == from && comp.to == to && comp.transitionType == transType) {
                return comp.compatibilityScore;
            }
        }
        return 0.0f;
    }
};

// ==================== TRANSITION VALIDATOR CLASS ====================
class TransitionValidator {
private:
    float calculateSupportRatio(const Voxel& transition, const vector<Voxel>& existingVoxels) {
        float supportedArea = 0.0f;
        float totalArea = transition.dimensions.x * transition.dimensions.z;

        for (const auto& voxel : existingVoxels) {
            if (voxel.level == transition.level - 1) {
                // Check if this voxel is below the transition
                float overlapArea = calculateFootprintOverlap(transition, voxel);
                supportedArea += overlapArea;
            }
        }

        return totalArea > 0 ? supportedArea / totalArea : 0.0f;
    }

    float calculateFootprintOverlap(const Voxel& above, const Voxel& below) {
        pair<Alice::vec, Alice::vec> boundsA = above.getBoundingBox();
        pair<Alice::vec, Alice::vec> boundsB = below.getBoundingBox();

        Alice::vec minA = boundsA.first;
        Alice::vec maxA = boundsA.second;
        Alice::vec minB = boundsB.first;
        Alice::vec maxB = boundsB.second;

        // Fixed: Ensure both arguments have the same type (float)
        float overlapX = std::max(0.0f, static_cast<float>(std::min(maxA.x, maxB.x) - std::max(minA.x, minB.x)));
        float overlapZ = std::max(0.0f, static_cast<float>(std::min(maxA.z, maxB.z) - std::max(minA.z, minB.z)));

        return overlapX * overlapZ;
    }

public:
    bool validateTransition(const Voxel& transition, const vector<Voxel>& existingVoxels) {
        // Check structural support
        float supportRatio = calculateSupportRatio(transition, existingVoxels);
        float requiredSupport = 0.6f; // Default requirement

        // Adjust requirements based on transition type
        if (transition.program == SKY_LOBBY || transition.program == AMENITY_FLOOR) {
            requiredSupport = 0.75f; // More conservative for public spaces
        }

        if (supportRatio < requiredSupport) {
            return false;
        }

        // Additional checks can be added here
        return true;
    }
};

// ==================== MAIN TRANSITION SYSTEM CLASS ====================
class ProgramTransitionSystem {
private:
    TransitionGenerator generator;
    TransitionValidator validator;
    vector<Voxel> buildingVoxels;
    int nextVoxelId = 0;

    // Find transition opportunities between voxels
    vector<TransitionOpportunity> findTransitionOpportunities() {
        vector<TransitionOpportunity> opportunities;

        for (size_t i = 0; i < buildingVoxels.size(); ++i) {
            for (size_t j = i + 1; j < buildingVoxels.size(); ++j) {
                Voxel& voxelA = buildingVoxels[i];
                Voxel& voxelB = buildingVoxels[j];

                // Skip if both are transitions
                if (voxelA.type == TRANSITION || voxelB.type == TRANSITION) continue;

                TransitionType transType = determineTransitionType(voxelA, voxelB);
                if (transType != PROXIMITY_BASED) { // Only consider connected voxels
                    float score = generator.getCompatibilityScore(voxelA.type, voxelB.type, transType);
                    if (score > 0.1f) { // Minimum threshold
                        opportunities.push_back({ &voxelA, &voxelB, transType, score });
                    }
                }
            }
        }

        // Sort by score
        sort(opportunities.begin(), opportunities.end(),
            [](const TransitionOpportunity& a, const TransitionOpportunity& b) {
                return a.score > b.score;
            });

        return opportunities;
    }

    TransitionType determineTransitionType(const Voxel& a, const Voxel& b) {
        // Get bounding boxes
        pair<Alice::vec, Alice::vec> boundsA = a.getBoundingBox();
        pair<Alice::vec, Alice::vec> boundsB = b.getBoundingBox();

        Alice::vec minA = boundsA.first;
        Alice::vec maxA = boundsA.second;
        Alice::vec minB = boundsB.first;
        Alice::vec maxB = boundsB.second;

        // Check if vertically aligned (more lenient check)
        float minX_overlap = std::max(static_cast<float>(minA.x), static_cast<float>(minB.x));
        float maxX_overlap = std::min(static_cast<float>(maxA.x), static_cast<float>(maxB.x));
        float overlapX = std::max(0.0f, maxX_overlap - minX_overlap);

        float minZ_overlap = std::max(static_cast<float>(minA.z), static_cast<float>(minB.z));
        float maxZ_overlap = std::min(static_cast<float>(maxA.z), static_cast<float>(maxB.z));
        float overlapZ = std::max(0.0f, maxZ_overlap - minZ_overlap);

        if (overlapX > 0.1f && overlapZ > 0.1f) {
            // Check if directly above/below
            if (abs(a.level - b.level) == 1) {
                // Check if one is directly above the other
                if ((minB.y >= maxA.y - 1.0f && minB.y <= maxA.y + 1.0f) ||
                    (minA.y >= maxB.y - 1.0f && minA.y <= maxB.y + 1.0f)) {
                    return VERTICAL_DIRECT;
                }
            }
        }

        // Check if on same level
        if (a.level == b.level) {
            // Check if sharing a face (more lenient)
            bool touchingX = (abs(maxA.x - minB.x) < 1.0f || abs(maxB.x - minA.x) < 1.0f) &&
                overlapZ > a.dimensions.z * 0.5f;
            bool touchingZ = (abs(maxA.z - minB.z) < 1.0f || abs(maxB.z - minA.z) < 1.0f) &&
                overlapX > a.dimensions.x * 0.5f;

            if (touchingX || touchingZ) {
                return HORIZONTAL_FACE;
            }

            // Check if sharing an edge
            bool cornerTouchX = (abs(maxA.x - minB.x) < 1.0f || abs(maxB.x - minA.x) < 1.0f);
            bool cornerTouchZ = (abs(maxA.z - minB.z) < 1.0f || abs(maxB.z - minA.z) < 1.0f);

            if (cornerTouchX && cornerTouchZ) {
                return DIAGONAL_VERTEX;
            }
        }

        return PROXIMITY_BASED;
    }

    bool sharesFullFace(const Voxel& a, const Voxel& b) {
        // Simplified check - would need proper implementation
        return (abs(a.position.x + a.dimensions.x - b.position.x) < 0.1f ||
            abs(b.position.x + b.dimensions.x - a.position.x) < 0.1f) &&
            abs(a.position.z - b.position.z) < 0.1f;
    }

    bool sharesEdge(const Voxel& a, const Voxel& b) {
        // Simplified check
        return false; // To be implemented
    }

    bool sharesDiagonalVertex(const Voxel& a, const Voxel& b) {
        // Simplified check
        return false; // To be implemented
    }

public:
    ProgramTransitionSystem() {
        // Initialize with some test voxels
        initializeTestBuilding();
    }

    void updateContext(UrbanContext context) {
        generator.updatePrioritiesForContext(context);
        // Clear any existing transitions
        vector<Voxel> originalVoxels;
        for (const auto& voxel : buildingVoxels) {
            if (voxel.type != TRANSITION) {
                originalVoxels.push_back(voxel);
            }
        }
        buildingVoxels = originalVoxels;
        // Generate new transitions with updated priorities
        generateTransitions();
    }

    void initializeTestBuilding() {
        cout << "\n=== CREATING BUILDING WITH NO GAPS ===" << endl;

        // Ground floor - Visit (Purple) - 3x3 grid - NO GAPS
        for (int x = 0; x < 3; ++x) {
            for (int z = 0; z < 3; ++z) {
                Voxel v;
                v.type = VISIT;
                v.position = Alice::vec(x * 12, 0, z * 12);
                v.dimensions = Alice::vec(12, 6, 12); // Changed from 12x6x8 to 12x6x12 to fill gaps
                v.level = 0;
                v.id = nextVoxelId++;
                buildingVoxels.push_back(v);
            }
        }

        // Middle floors - Work (Blue) - DIRECTLY STACKED
        for (int level = 1; level <= 5; ++level) {
            for (int x = 0; x < 3; ++x) {
                for (int z = 0; z < 3; ++z) {
                    Voxel v;
                    v.type = WORK;
                    v.position = Alice::vec(x * 8, 6 + (level - 1) * 4, z * 8);
                    v.dimensions = Alice::vec(8, 4, 8);
                    v.level = level;
                    v.id = nextVoxelId++;
                    buildingVoxels.push_back(v);
                }
            }
        }

        // Top floors - Live (Yellow) - DIRECTLY ON TOP OF WORK
        // Work ends at Y = 6 + 5*4 = 26
        for (int level = 6; level <= 8; ++level) {
            for (int x = 0; x < 6; ++x) {  // More LIVE voxels in X direction
                for (int z = 0; z < 6; ++z) {  // More LIVE voxels in Z direction
                    Voxel v;
                    v.type = LIVE;
                    v.position = Alice::vec(x * 4, 26 + (level - 6) * 4, z * 4);  // 4-unit height spacing
                    v.dimensions = Alice::vec(4, 4, 4);  // CHANGED TO 4x4x4 (length x height x width)
                    v.level = level;
                    v.id = nextVoxelId++;
                    buildingVoxels.push_back(v);
                }
            }
        }

        cout << "Building created with:" << endl;
        cout << "- VISIT: Y = 0 to 6 (12x6x12 voxels - no gaps)" << endl;
        cout << "- WORK: Y = 6 to 26 (8x4x8 voxels)" << endl;
        cout << "- LIVE: Y = 26 to 38 (4x4x4 voxels)" << endl;
        cout << "NO GAPS BETWEEN VOXEL TYPES!" << endl;
    }

    void generateTransitions() {
        vector<TransitionOpportunity> opportunities = findTransitionOpportunities();
        cout << "Found " << opportunities.size() << " transition opportunities" << endl;

        // Process top opportunities
        int processedCount = 0;
        int skippedCount = 0;
        for (const auto& opp : opportunities) {
            if (processedCount >= 10) break; // Limit for testing

            Voxel transition = generator.generateTransition(*opp.voxelA, *opp.voxelB, opp.transitionType);

            // Skip if no rule matched (invalid transition)
            if (transition.type == NONE) {
                skippedCount++;
                continue;
            }

            if (validator.validateTransition(transition, buildingVoxels)) {
                transition.id = nextVoxelId++;

                // Find voxels to replace based on the transition position and dimensions
                vector<int> voxelsToReplace;
                for (int i = 0; i < buildingVoxels.size(); ++i) {
                    const Voxel& v = buildingVoxels[i];

                    // Check if this voxel overlaps with the transition
                    if (v.type != TRANSITION) { // Don't replace existing transitions
                        // Check vertical overlap
                        bool verticalOverlap = (v.position.y < transition.position.y + transition.dimensions.y) &&
                            (v.position.y + v.dimensions.y > transition.position.y);

                        // Check horizontal overlap
                        bool horizontalOverlap = (v.position.x < transition.position.x + transition.dimensions.x) &&
                            (v.position.x + v.dimensions.x > transition.position.x) &&
                            (v.position.z < transition.position.z + transition.dimensions.z) &&
                            (v.position.z + v.dimensions.z > transition.position.z);

                        if (verticalOverlap && horizontalOverlap) {
                            voxelsToReplace.push_back(i);
                        }
                    }
                }

                // Remove overlapping voxels
                sort(voxelsToReplace.rbegin(), voxelsToReplace.rend()); // Sort in reverse to remove from back
                for (int idx : voxelsToReplace) {
                    cout << "Removing voxel at index " << idx << " of type "
                        << getVoxelTypeName(buildingVoxels[idx].type) << endl;
                    buildingVoxels.erase(buildingVoxels.begin() + idx);
                }

                // Add the transition
                buildingVoxels.push_back(transition);
                processedCount++;

                cout << "Added transition: " << getTransitionProgramName(transition.program)
                    << " between " << getVoxelTypeName(opp.voxelA->type)
                    << " and " << getVoxelTypeName(opp.voxelB->type)
                    << " at position (" << transition.position.x << ", "
                    << transition.position.y << ", " << transition.position.z << ")"
                    << " with dimensions (" << transition.dimensions.x << ", "
                    << transition.dimensions.y << ", " << transition.dimensions.z << ")" << endl;
                cout << "Replaced " << voxelsToReplace.size() << " voxels" << endl;
            }
        }

        cout << "Generated " << processedCount << " transitions" << endl;
        cout << "Skipped " << skippedCount << " opportunities (no matching rule)" << endl;
        cout << "Total voxels in building: " << buildingVoxels.size() << endl;
    }

    void drawVoxels() {
        // First pass: Draw all non-transition voxels
        for (const auto& voxel : buildingVoxels) {
            if (voxel.type != TRANSITION) {
                Alice::vec color = getVoxelColor(voxel);
                glColor3f(color.x, color.y, color.z);

                // Draw voxel as a box
                glPushMatrix();
                glTranslatef(voxel.position.x + voxel.dimensions.x / 2,
                    voxel.position.y + voxel.dimensions.y / 2,
                    voxel.position.z + voxel.dimensions.z / 2);
                glScalef(voxel.dimensions.x, voxel.dimensions.y, voxel.dimensions.z);

                // Draw as wireframe
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                drawBox(1.0f);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                // Draw solid with transparency
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4f(color.x, color.y, color.z, 0.3f);
                drawBox(1.0f);
                glDisable(GL_BLEND);

                glPopMatrix();
            }
        }

        // Second pass: Draw transitions on top with more visibility
        for (const auto& voxel : buildingVoxels) {
            if (voxel.type == TRANSITION) {
                Alice::vec color = getVoxelColor(voxel);

                // Draw transition with thicker lines and more opacity
                glPushMatrix();
                glTranslatef(voxel.position.x + voxel.dimensions.x / 2,
                    voxel.position.y + voxel.dimensions.y / 2,
                    voxel.position.z + voxel.dimensions.z / 2);
                glScalef(voxel.dimensions.x, voxel.dimensions.y, voxel.dimensions.z);

                // Draw as thicker wireframe
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glLineWidth(3.0f);
                glColor3f(color.x, color.y, color.z);
                drawBox(1.0f);
                glLineWidth(1.0f);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                // Draw solid with more opacity
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4f(color.x, color.y, color.z, 0.7f); // More opaque
                drawBox(1.0f);
                glDisable(GL_BLEND);

                glPopMatrix();
            }
        }
    }

    Alice::vec getVoxelColor(const Voxel& voxel) {
        switch (voxel.type) {
        case VISIT: return Alice::vec(0.8f, 0.4f, 0.8f); // Purple
        case WORK: return Alice::vec(0.2f, 0.6f, 1.0f);  // Blue
        case LIVE: return Alice::vec(1.0f, 0.8f, 0.2f);  // Yellow
        case TRANSITION:
            // Color based on transition program
            switch (voxel.program) {
            case SKY_LOBBY: return Alice::vec(1.0f, 0.0f, 0.0f);      // Bright Red
            case AMENITY_FLOOR: return Alice::vec(0.0f, 1.0f, 0.0f);  // Bright Green
            case SERVICE_FLOOR: return Alice::vec(0.0f, 1.0f, 1.0f);  // Bright Cyan
            case SHARED_TERRACE: return Alice::vec(0.0f, 0.8f, 0.0f); // Green
            case RETAIL_PODIUM: return Alice::vec(1.0f, 0.6f, 0.0f);  // Orange
            case COMMUNITY_GARDEN: return Alice::vec(0.2f, 0.9f, 0.2f); // Light Green
            case SKY_BRIDGE: return Alice::vec(0.7f, 0.7f, 1.0f);     // Light Blue
            case TRANSIT_HUB: return Alice::vec(0.8f, 0.0f, 0.8f);    // Magenta
            case EXHIBITION_SPACE: return Alice::vec(0.9f, 0.9f, 0.3f); // Light Yellow
            case COWORKING_PLAZA: return Alice::vec(0.5f, 0.5f, 0.9f); // Periwinkle
            default: return Alice::vec(1.0f, 1.0f, 0.0f);             // Bright Yellow
            }
        default: return Alice::vec(0.5f, 0.5f, 0.5f);
        }
    }

    string getVoxelTypeName(VoxelType type) {
        switch (type) {
        case VISIT: return "VISIT";
        case WORK: return "WORK";
        case LIVE: return "LIVE";
        case TRANSITION: return "TRANSITION";
        default: return "UNKNOWN";
        }
    }

    string getTransitionProgramName(TransitionProgram program) {
        switch (program) {
        case SKY_LOBBY: return "SKY_LOBBY";
        case AMENITY_FLOOR: return "AMENITY_FLOOR";
        case SERVICE_FLOOR: return "SERVICE_FLOOR";
        case SHARED_TERRACE: return "SHARED_TERRACE";
        case RETAIL_PODIUM: return "RETAIL_PODIUM";
        case COMMUNITY_GARDEN: return "COMMUNITY_GARDEN";
        case SKY_BRIDGE: return "SKY_BRIDGE";
        case TRANSIT_HUB: return "TRANSIT_HUB";
        case EXHIBITION_SPACE: return "EXHIBITION_SPACE";
        case COWORKING_PLAZA: return "COWORKING_PLAZA";
        default: return "TRANSITION";
        }
    }

    void drawBox(float size) {
        glBegin(GL_QUADS);
        // Front face
        glVertex3f(-size / 2, -size / 2, size / 2);
        glVertex3f(size / 2, -size / 2, size / 2);
        glVertex3f(size / 2, size / 2, size / 2);
        glVertex3f(-size / 2, size / 2, size / 2);

        // Back face
        glVertex3f(-size / 2, -size / 2, -size / 2);
        glVertex3f(-size / 2, size / 2, -size / 2);
        glVertex3f(size / 2, size / 2, -size / 2);
        glVertex3f(size / 2, -size / 2, -size / 2);

        // Top face
        glVertex3f(-size / 2, size / 2, -size / 2);
        glVertex3f(-size / 2, size / 2, size / 2);
        glVertex3f(size / 2, size / 2, size / 2);
        glVertex3f(size / 2, size / 2, -size / 2);

        // Bottom face
        glVertex3f(-size / 2, -size / 2, -size / 2);
        glVertex3f(size / 2, -size / 2, -size / 2);
        glVertex3f(size / 2, -size / 2, size / 2);
        glVertex3f(-size / 2, -size / 2, size / 2);

        // Right face
        glVertex3f(size / 2, -size / 2, -size / 2);
        glVertex3f(size / 2, size / 2, -size / 2);
        glVertex3f(size / 2, size / 2, size / 2);
        glVertex3f(size / 2, -size / 2, size / 2);

        // Left face
        glVertex3f(-size / 2, -size / 2, -size / 2);
        glVertex3f(-size / 2, -size / 2, size / 2);
        glVertex3f(-size / 2, size / 2, size / 2);
        glVertex3f(-size / 2, size / 2, -size / 2);
        glEnd();
    }
};

// Global instance
ProgramTransitionSystem* transitionSystem = nullptr;

// Global UI variables
UrbanContext currentContext = HIGH_DENSITY_MIXED_USE;
float sliderValue = 0.0f; // 0.0 to 1.0
bool sliderDragging = false;

// UI positioning constants - positioned at bottom left
const float SLIDER_X = 20.0f;
const float SLIDER_Y = 60.0f;  // From bottom of screen
const float SLIDER_WIDTH = 300.0f;
const float SLIDER_HEIGHT = 20.0f;
const float SLIDER_MARKER_WIDTH = 10.0f;

// ===================== UI DRAWING FUNCTIONS =====================

void drawSlider() {
    // Save current matrices
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    gluOrtho2D(0, viewport[2], 0, viewport[3]); // Note: origin at bottom-left

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Draw slider background
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(SLIDER_X, SLIDER_Y);
    glVertex2f(SLIDER_X + SLIDER_WIDTH, SLIDER_Y);
    glVertex2f(SLIDER_X + SLIDER_WIDTH, SLIDER_Y + SLIDER_HEIGHT);
    glVertex2f(SLIDER_X, SLIDER_Y + SLIDER_HEIGHT);
    glEnd();

    // Draw slider sections for each context
    float sectionWidth = SLIDER_WIDTH / 5.0f;
    for (int i = 0; i < 5; ++i) {
        float x = SLIDER_X + i * sectionWidth;

        // Highlight current context section
        if (currentContext == i) {
            glColor3f(0.4f, 0.6f, 0.8f);
        }
        else {
            glColor3f(0.3f, 0.3f, 0.3f);
        }

        glBegin(GL_QUADS);
        glVertex2f(x + 1, SLIDER_Y + 1);
        glVertex2f(x + sectionWidth - 1, SLIDER_Y + 1);
        glVertex2f(x + sectionWidth - 1, SLIDER_Y + SLIDER_HEIGHT - 1);
        glVertex2f(x + 1, SLIDER_Y + SLIDER_HEIGHT - 1);
        glEnd();
    }

    // Draw slider marker
    float markerX = SLIDER_X + sliderValue * SLIDER_WIDTH - SLIDER_MARKER_WIDTH / 2;
    glColor3f(1.0f, 1.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(markerX, SLIDER_Y - 5);
    glVertex2f(markerX + SLIDER_MARKER_WIDTH, SLIDER_Y - 5);
    glVertex2f(markerX + SLIDER_MARKER_WIDTH, SLIDER_Y + SLIDER_HEIGHT + 5);
    glVertex2f(markerX, SLIDER_Y + SLIDER_HEIGHT + 5);
    glEnd();

    // Draw context labels
    glColor3f(1.0f, 1.0f, 1.0f);
    for (int i = 0; i < 5; ++i) {
        float x = SLIDER_X + i * sectionWidth + 5;
        float y = SLIDER_Y - 15;  // Below the slider

        // Simple text rendering using bitmap characters
        string text = contextNames[i];
        glRasterPos2f(x, y);
        for (char c : text) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
        }
    }

    // Draw current context title
    glRasterPos2f(SLIDER_X, SLIDER_Y + SLIDER_HEIGHT + 10);
    string title = "Urban Context: " + contextNames[currentContext];
    for (char c : title) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }

    // Restore matrices
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void updateContextFromSlider() {
    int newContext = (int)(sliderValue * 4.999f); // Convert to 0-4
    if (newContext != currentContext) {
        currentContext = (UrbanContext)newContext;
        if (transitionSystem) {
            transitionSystem->updateContext(currentContext);
        }
    }
}

// ===================== ALICE FRAMEWORK FUNCTIONS =====================

void setup() {
    cout << "=== Program Transition Logic System - CONTEXT ADAPTIVE VERSION ===" << endl;
    cout << "Initializing transition system..." << endl;

    transitionSystem = new ProgramTransitionSystem();

    // Initialize with default context
    transitionSystem->updateContext(currentContext);

    cout << "Setup complete!" << endl;
    cout << "*** CONTROLS ***" << endl;
    cout << "Use SLIDER (bottom-left) to change urban context" << endl;
    cout << "Press 'g' to regenerate transitions" << endl;
    cout << "Press 'r' to reset" << endl;
    cout << "Color scheme:" << endl;
    cout << "  WORK = Blue" << endl;
    cout << "  VISIT = Purple" << endl;
    cout << "  LIVE = Yellow" << endl;
    cout << "  TRANSITIONS:" << endl;
    cout << "    SKY_LOBBY = Bright Red" << endl;
    cout << "    AMENITY_FLOOR = Bright Green" << endl;
    cout << "    SERVICE_FLOOR = Bright Cyan" << endl;
    cout << "    SHARED_TERRACE = Green" << endl;
    cout << "    RETAIL_PODIUM = Orange" << endl;
    cout << "    COMMUNITY_GARDEN = Light Green" << endl;
    cout << "    SKY_BRIDGE = Light Blue" << endl;
    cout << "    TRANSIT_HUB = Magenta" << endl;
    cout << "    EXHIBITION_SPACE = Light Yellow" << endl;
    cout << "    COWORKING_PLAZA = Periwinkle" << endl;
}

void update(int value) {
    // Update logic can be added here
}

void draw() {
    backGround(0.8);
    drawGrid(50);

    if (transitionSystem) {
        transitionSystem->drawVoxels();
    }

    // Draw UI on top
    drawSlider();
}

void keyPress(unsigned char k, int xm, int ym) {
    if (k == 'g') {
        cout << "Generating transitions..." << endl;
        if (transitionSystem) {
            transitionSystem->generateTransitions();
        }
    }
    else if (k == 'r') {
        cout << "Resetting system..." << endl;
        delete transitionSystem;
        transitionSystem = new ProgramTransitionSystem();
    }
}

void mousePress(int b, int state, int x, int y) {
    if (state == GLUT_DOWN && b == GLUT_LEFT_BUTTON) {
        // In OpenGL, y=0 is at bottom of window
        int viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        y = viewport[3] - y;  // Convert to OpenGL coordinates

        // Check if click is on slider
        if (x >= SLIDER_X && x <= SLIDER_X + SLIDER_WIDTH &&
            y >= SLIDER_Y - 5 && y <= SLIDER_Y + SLIDER_HEIGHT + 5) {
            sliderDragging = true;
            sliderValue = (x - SLIDER_X) / SLIDER_WIDTH;
            sliderValue = max(0.0f, min(1.0f, sliderValue));
            updateContextFromSlider();
        }
    }
    else if (state == GLUT_UP && b == GLUT_LEFT_BUTTON) {
        sliderDragging = false;
    }
}

void mouseMotion(int x, int y) {
    if (sliderDragging) {
        sliderValue = (x - SLIDER_X) / SLIDER_WIDTH;
        sliderValue = max(0.0f, min(1.0f, sliderValue));
        updateContextFromSlider();
    }
}

#endif // _MAIN_