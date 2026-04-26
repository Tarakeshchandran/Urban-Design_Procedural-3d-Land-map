#define _MAIN_
#ifdef _MAIN_

#include "main.h"

//#include "alice/spatialBin.h"
//// zSpace Core Headers
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
Alice::vec zVecToAliceVec(zVector& in)
{
    return Alice::vec(in.x, in.y, in.z);
}

zVector AliceVecToZvec(Alice::vec& in)
{
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

// Enumeration for edge conditions
enum EdgeCondition {
    NO_SPECIAL_EDGE,
    MAIN_STREET_EDGE,
    WATERFRONT_EDGE,
    PARK_EDGE
};

// Enumeration for corner conditions
enum CornerCondition {
    NO_SPECIAL_CORNER,
    PLAZA_CORNER,
    LANDMARK_CORNER,
    TERRACED_CORNER
};

// Context names for display
const vector<string> contextNames = {
    "High Density Mixed Use",
    "Low Density Residential",
    "Commercial Core",
    "Cultural District",
    "Transit Oriented"
};

// Edge condition names for display
const vector<string> edgeConditionNames = {
    "No Special Edge",
    "Main Street Edge",
    "Waterfront Edge",
    "Park Edge"
};

// Corner condition names for display
const vector<string> cornerConditionNames = {
    "No Special Corner",
    "Plaza Corner",
    "Landmark Corner",
    "Terraced Corner"
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
        function<bool(const Voxel&, const Voxel&, TransitionType, const vector<Voxel>&)> condition;
        function<Voxel(const Voxel&, const Voxel&, TransitionType, const vector<Voxel>&)> generator;
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

    // Check if a LIVE voxel is part of a 3x3 cluster
    bool isPartOf3x3LiveCluster(const Voxel& voxel, const vector<Voxel>& allVoxels) {
        if (voxel.type != LIVE) return false;

        // Find bounds of a potential 3x3 cluster
        float voxelSize = voxel.dimensions.x; // Assuming uniform dimensions for LIVE

        for (int startX = -2; startX <= 0; startX++) {
            for (int startZ = -2; startZ <= 0; startZ++) {
                // Check if this starting position could form a 3x3 with our voxel
                bool hasAll9 = true;

                for (int dx = 0; dx < 3 && hasAll9; dx++) {
                    for (int dz = 0; dz < 3 && hasAll9; dz++) {
                        float checkX = voxel.position.x + (startX + dx) * voxelSize;
                        float checkZ = voxel.position.z + (startZ + dz) * voxelSize;

                        bool found = false;
                        for (const auto& v : allVoxels) {
                            if (v.type == LIVE && v.level == voxel.level &&
                                abs(v.position.x - checkX) < 0.1f &&
                                abs(v.position.z - checkZ) < 0.1f) {
                                found = true;
                                break;
                            }
                        }

                        if (!found) hasAll9 = false;
                    }
                }

                if (hasAll9) return true;
            }
        }

        return false;
    }

    // Check if a LIVE voxel is a corner of a 3x3 cluster
    bool isCornerOf3x3LiveCluster(const Voxel& voxel, const vector<Voxel>& allVoxels,
        Alice::vec& cornerPosition) {
        if (voxel.type != LIVE) return false;

        float voxelSize = voxel.dimensions.x;

        // Check if this voxel is at a corner of any 3x3 cluster
        // Try all four possible corner positions
        struct CornerCheck {
            int offsetX, offsetZ;
            string cornerType;
        };

        vector<CornerCheck> cornerChecks = {
            {0, 0, "bottom_left"},    // This voxel is bottom-left of a 3x3
            {-2, 0, "bottom_right"},  // This voxel is bottom-right of a 3x3
            {0, -2, "top_left"},      // This voxel is top-left of a 3x3
            {-2, -2, "top_right"}     // This voxel is top-right of a 3x3
        };

        for (const auto& check : cornerChecks) {
            bool hasAll9 = true;

            for (int dx = 0; dx < 3 && hasAll9; dx++) {
                for (int dz = 0; dz < 3 && hasAll9; dz++) {
                    float checkX = voxel.position.x + (check.offsetX + dx) * voxelSize;
                    float checkZ = voxel.position.z + (check.offsetZ + dz) * voxelSize;

                    bool found = false;
                    for (const auto& v : allVoxels) {
                        if (v.type == LIVE && v.level == voxel.level &&
                            abs(v.position.x - checkX) < 0.1f &&
                            abs(v.position.z - checkZ) < 0.1f) {
                            found = true;
                            break;
                        }
                    }

                    if (!found) hasAll9 = false;
                }
            }

            if (hasAll9) {
                // Calculate position for the terrace based on which corner this is
                if (check.cornerType == "bottom_left") {
                    cornerPosition = Alice::vec(voxel.position.x - voxelSize * 0.5f,
                        voxel.position.y + voxel.dimensions.y,
                        voxel.position.z - voxelSize * 0.5f);
                }
                else if (check.cornerType == "bottom_right") {
                    cornerPosition = Alice::vec(voxel.position.x + voxel.dimensions.x + voxelSize * 0.5f,
                        voxel.position.y + voxel.dimensions.y,
                        voxel.position.z - voxelSize * 0.5f);
                }
                else if (check.cornerType == "top_left") {
                    cornerPosition = Alice::vec(voxel.position.x - voxelSize * 0.5f,
                        voxel.position.y + voxel.dimensions.y,
                        voxel.position.z + voxel.dimensions.z + voxelSize * 0.5f);
                }
                else if (check.cornerType == "top_right") {
                    cornerPosition = Alice::vec(voxel.position.x + voxel.dimensions.x + voxelSize * 0.5f,
                        voxel.position.y + voxel.dimensions.y,
                        voxel.position.z + voxel.dimensions.z + voxelSize * 0.5f);
                }
                return true;
            }
        }

        return false;
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
 
// Check if a community garden already exists within a 3x3 grid area
    bool communityGardenExistsInArea(const Voxel& centerVoxel, const vector<Voxel>& allVoxels);

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
    // Make sure this is the first rule in the initializeRules() method
// Rule 1: Sky Lobby Creation
        transitionRules.push_back({
            "SkyLobbyRule",
            [](const Voxel& below, const Voxel& above, TransitionType type, const vector<Voxel>& allVoxels) {
                return below.type == WORK && above.type == LIVE &&
                       type == VERTICAL_DIRECT;
            },
            [this](const Voxel& below, const Voxel& above, TransitionType type, const vector<Voxel>& allVoxels) {
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
            [this](const Voxel& below, const Voxel& above, TransitionType type, const vector<Voxel>& allVoxels) {
                return below.type == WORK && above.type == LIVE &&
                       type == VERTICAL_DIRECT;
            },
            [this](const Voxel& below, const Voxel& above, TransitionType type, const vector<Voxel>& allVoxels) {
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

        // Rule 3: Shared Terrace for LIVE voxel 3x3 clusters only
        transitionRules.push_back({
            "SharedTerraceRule",
            [this](const Voxel& voxelA, const Voxel& voxelB, TransitionType type, const vector<Voxel>& allVoxels) {
                // Only apply to LIVE voxels on same level with horizontal face connection
                if (voxelA.type != LIVE || voxelB.type != LIVE ||
                    voxelA.level != voxelB.level || type != HORIZONTAL_FACE) {
                    return false;
                }

                // BOTH voxels must be part of corner positions to trigger
                Alice::vec cornerPosA, cornerPosB;
                bool aIsCorner = isCornerOf3x3LiveCluster(voxelA, allVoxels, cornerPosA);
                bool bIsCorner = isCornerOf3x3LiveCluster(voxelB, allVoxels, cornerPosB);

                return aIsCorner && bIsCorner;
            },
            [this](const Voxel& voxelA, const Voxel& voxelB, TransitionType type, const vector<Voxel>& allVoxels) {
                Voxel terrace;
                terrace.type = TRANSITION;
                terrace.program = SHARED_TERRACE;

                // Place terrace between the two corner voxels
                Alice::vec cornerPositionA, cornerPositionB;
                bool aIsCorner = isCornerOf3x3LiveCluster(voxelA, allVoxels, cornerPositionA);
                bool bIsCorner = isCornerOf3x3LiveCluster(voxelB, allVoxels, cornerPositionB);

                if (aIsCorner && bIsCorner) {
                    // Place terrace at the midpoint between the corner positions
                    terrace.position = calculateMidpoint(cornerPositionA, cornerPositionB);
                }
 else {
                    // Fallback (shouldn't happen if condition passed)
                    terrace.position = calculateMidpoint(voxelA.position, voxelB.position);
                }

                terrace.dimensions = Alice::vec(2.5f, 1.5f, 2.5f); // Even smaller terraces
                terrace.level = voxelA.level;
                terrace.features = {"corner_garden", "seating_area", "sculpture"};
                return terrace;
            },
            60, 60  // Lower priority
            });

        // Rule 4: Service Floor Between Visit and Work
        transitionRules.push_back({
            "ServiceFloorRule",
            [](const Voxel& below, const Voxel& above, TransitionType type, const vector<Voxel>& allVoxels) {
                return below.type == VISIT && above.type == WORK &&
                       type == VERTICAL_DIRECT;
            },
            [this](const Voxel& below, const Voxel& above, TransitionType type, const vector<Voxel>& allVoxels) {
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
            [](const Voxel& below, const Voxel& above, TransitionType type, const vector<Voxel>& allVoxels) {
                return below.type == VISIT && above.type == WORK &&
                       type == VERTICAL_DIRECT && below.level == 0;
            },
            [this](const Voxel& below, const Voxel& above, TransitionType type, const vector<Voxel>& allVoxels) {
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

        // Rule 6: Community Garden - Between Live voxels with proximity restriction
        transitionRules.push_back({
            "CommunityGardenRule",
            [this](const Voxel& voxelA, const Voxel& voxelB, TransitionType type, const vector<Voxel>& allVoxels) {
                // Basic checks: must be LIVE voxels on same level with horizontal face connection
                if (!(type == HORIZONTAL_FACE &&
                      voxelA.type == LIVE && voxelB.type == LIVE &&
                      voxelA.level == voxelB.level)) {
                    return false;
                }

                // NEW CHECK: Ensure no other community garden exists in a 3x3 grid around either voxel
                if (communityGardenExistsInArea(voxelA, allVoxels) ||
                    communityGardenExistsInArea(voxelB, allVoxels)) {
                    return false; // Garden already exists nearby
                }

                // All checks passed
                return true;
            },
            [this](const Voxel& voxelA, const Voxel& voxelB, TransitionType type, const vector<Voxel>& allVoxels) {
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
            [](const Voxel& voxelA, const Voxel& voxelB, TransitionType type, const vector<Voxel>& allVoxels) {
                return type == HORIZONTAL_FACE &&
                       voxelA.type == WORK && voxelB.type == WORK &&
                       voxelA.level > 3;
            },
            [this](const Voxel& voxelA, const Voxel& voxelB, TransitionType type, const vector<Voxel>& allVoxels) {
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
            [](const Voxel& below, const Voxel& above, TransitionType type, const vector<Voxel>& allVoxels) {
                return below.type == VISIT && above.type == WORK &&
                       type == VERTICAL_DIRECT && below.level == 0;
            },
            [this](const Voxel& below, const Voxel& above, TransitionType type, const vector<Voxel>& allVoxels) {
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
            [](const Voxel& below, const Voxel& above, TransitionType type, const vector<Voxel>& allVoxels) {
                return below.type == VISIT && above.type == LIVE &&
                       type == VERTICAL_DIRECT;
            },
            [this](const Voxel& below, const Voxel& above, TransitionType type, const vector<Voxel>& allVoxels) {
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
            [](const Voxel& voxelA, const Voxel& voxelB, TransitionType type, const vector<Voxel>& allVoxels) {
                return type == HORIZONTAL_FACE &&
                       voxelA.type == WORK && voxelB.type == WORK;
            },
            [this](const Voxel& voxelA, const Voxel& voxelB, TransitionType type, const vector<Voxel>& allVoxels) {
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
    Voxel generateTransition(const Voxel& voxelA, const Voxel& voxelB, TransitionType type, const vector<Voxel>& allVoxels) {
        // Sort rules by CURRENT priority
        sort(transitionRules.begin(), transitionRules.end(),
            [](const TransitionRule& a, const TransitionRule& b) {
                return a.currentPriority > b.currentPriority;
            });

        // Try each rule
        for (const auto& rule : transitionRules) {
            if (rule.condition(voxelA, voxelB, type, allVoxels)) {
                return rule.generator(voxelA, voxelB, type, allVoxels);
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
// Implementation of the communityGardenExistsInArea function
bool TransitionGenerator::communityGardenExistsInArea(const Voxel& centerVoxel, const vector<Voxel>& allVoxels) {
    // Define the area to check (3x3 grid centered at the voxel)
    float gridSize = centerVoxel.dimensions.x; // Assuming uniform LIVE voxel size
    float minX = centerVoxel.position.x - gridSize * 1.5f;
    float maxX = centerVoxel.position.x + gridSize * 1.5f;
    float minZ = centerVoxel.position.z - gridSize * 1.5f;
    float maxZ = centerVoxel.position.z + gridSize * 1.5f;
    int level = centerVoxel.level;

    // Check all voxels to see if any COMMUNITY_GARDEN transitions exist in this area
    for (const auto& voxel : allVoxels) {
        // Only check transitions with COMMUNITY_GARDEN program at the same level
        if (voxel.type == TRANSITION && voxel.program == COMMUNITY_GARDEN && voxel.level == level) {
            // Check if this garden is within our 3x3 grid area
            if (voxel.position.x <= maxX && (voxel.position.x + voxel.dimensions.x) >= minX &&
                voxel.position.z <= maxZ && (voxel.position.z + voxel.dimensions.z) >= minZ) {
                return true; // Found an existing garden in the area
            }
        }
    }

    return false; // No existing garden found in the area
}
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
    UrbanContext currentContext = HIGH_DENSITY_MIXED_USE;
    EdgeCondition currentEdge = NO_SPECIAL_EDGE;
    CornerCondition currentCorner = NO_SPECIAL_CORNER;

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
        // Initialize with default HIGH_DENSITY_MIXED_USE context
        currentContext = HIGH_DENSITY_MIXED_USE;
        currentEdge = NO_SPECIAL_EDGE;
        currentCorner = NO_SPECIAL_CORNER;

        // Create base building according to current context
        rebuildWithCurrentConditions();
    }

    void updateContext(UrbanContext context) {
        // Update current context
        currentContext = context;

        cout << "Context changed to: " << contextNames[currentContext] << endl;

        // Reset the building voxels completely
        rebuildWithCurrentConditions();
    }

    void updateEdgeCondition(EdgeCondition edge) {
        // Update current edge condition
        currentEdge = edge;

        // Rebuild the building with new conditions
        rebuildWithCurrentConditions();
    }

    void updateCornerCondition(CornerCondition corner) {
        // Update current corner condition
        currentCorner = corner;

        // Rebuild the building with new conditions
        rebuildWithCurrentConditions();
    }

    void rebuildWithCurrentConditions() {
        // Clear existing building
        buildingVoxels.clear();
        nextVoxelId = 0;

        // Create base building according to context
        switch (currentContext) {
        case HIGH_DENSITY_MIXED_USE:
            initializeHighDensityMixedUse();
            break;
        case LOW_DENSITY_RESIDENTIAL:
            initializeLowDensityResidential();
            break;
        case COMMERCIAL_CORE:
            initializeCommercialCore();
            break;
        case CULTURAL_DISTRICT:
            initializeCulturalDistrict();
            break;
        case TRANSIT_ORIENTED:
            initializeTransitOriented();
            break;
        default:
            cout << "Warning: Invalid context - defaulting to HIGH_DENSITY_MIXED_USE" << endl;
            initializeHighDensityMixedUse();
            break;
        }

        // Apply edge condition modifications
        applyEdgeCondition();

        // Apply corner condition modifications
        applyCornerCondition();

        // Update generator priorities for this context
        generator.updatePrioritiesForContext(currentContext);

        // Generate transitions for the new building
        generateTransitions();
    }

    // Context-specific building initializations
    void initializeHighDensityMixedUse() {
        cout << "\n=== CREATING HIGH DENSITY MIXED-USE BUILDING ===" << endl;

        // Ground floor - Visit (Purple) - 3x3 grid - NO GAPS
        for (int x = 0; x < 3; ++x) {
            for (int z = 0; z < 3; ++z) {
                Voxel v;
                v.type = VISIT;
                v.position = Alice::vec(x * 12 - 18, 0, z * 12 - 18); // Centered at origin
                v.dimensions = Alice::vec(12, 6, 12);
                v.level = 0;
                v.id = nextVoxelId++;
                buildingVoxels.push_back(v);
            }
        }

        // Middle floors - Work (Blue) - DIRECTLY STACKED
        for (int level = 1; level <= 8; ++level) { // Taller commercial section (8 floors)
            for (int x = 0; x < 3; ++x) {
                for (int z = 0; z < 3; ++z) {
                    Voxel v;
                    v.type = WORK;
                    v.position = Alice::vec(x * 8 - 12, 6 + (level - 1) * 4, z * 8 - 12);
                    v.dimensions = Alice::vec(8, 4, 8);
                    v.level = level;
                    v.id = nextVoxelId++;
                    buildingVoxels.push_back(v);
                }
            }
        }

        // Top floors - Live (Yellow) - DIRECTLY ON TOP OF WORK
        // Work ends at Y = 6 + 8*4 = 38
        for (int level = 9; level <= 15; ++level) { // Taller residential section (7 floors)
            for (int x = 0; x < 6; ++x) {
                for (int z = 0; z < 6; ++z) {
                    Voxel v;
                    v.type = LIVE;
                    v.position = Alice::vec(x * 4 - 12, 38 + (level - 9) * 4, z * 4 - 12);
                    v.dimensions = Alice::vec(4, 4, 4);
                    v.level = level;
                    v.id = nextVoxelId++;
                    buildingVoxels.push_back(v);
                }
            }
        }

        cout << "High Density Mixed-Use Building created with:" << endl;
        cout << "- VISIT: Y = 0 to 6 (Ground floor)" << endl;
        cout << "- WORK: Y = 6 to 38 (8 floors)" << endl;
        cout << "- LIVE: Y = 38 to 66 (7 floors)" << endl;
    }

    void initializeLowDensityResidential() {
        cout << "\n=== CREATING LOW DENSITY RESIDENTIAL BUILDING (G+4) ===" << endl;

        // Create a simple G+4 residential building
        // Base layout - 4x4 grid of residential units with some gaps for yards
        bool layout[4][4] = {
            {true, true, true, false},
            {true, true, false, true},
            {true, false, true, true},
            {false, true, true, true}
        };

        // Ground + 4 floors of LIVE voxels
        for (int level = 0; level <= 4; ++level) {
            for (int x = 0; x < 4; ++x) {
                for (int z = 0; z < 4; ++z) {
                    // Skip positions that are yards/open space
                    if (!layout[x][z]) continue;

                    Voxel v;
                    v.type = LIVE;
                    v.position = Alice::vec(x * 5 - 10, level * 4, z * 5 - 10); // Centered at origin
                    v.dimensions = Alice::vec(4, 4, 4);
                    v.level = level;
                    v.id = nextVoxelId++;
                    buildingVoxels.push_back(v);
                }
            }
        }

        // Small community facilities at ground level - just one VISIT voxel
        Voxel community;
        community.type = VISIT;
        community.position = Alice::vec(10, 0, 0);
        community.dimensions = Alice::vec(8, 4, 8);
        community.level = 0;
        community.id = nextVoxelId++;
        buildingVoxels.push_back(community);

        cout << "Low Density Residential Building created:" << endl;
        cout << "- LIVE units: G+4 floors with yards/gaps" << endl;
        cout << "- Small VISIT facility at ground level" << endl;
    }

    void initializeCommercialCore() {
        cout << "\n=== CREATING COMMERCIAL CORE BUILDING ===" << endl;

        // Ground floor - Visit (Purple) - 2x4 grid
        for (int x = 0; x < 2; ++x) {
            for (int z = 0; z < 4; ++z) {
                Voxel v;
                v.type = VISIT;
                v.position = Alice::vec(x * 12 - 12, 0, z * 8 - 16);
                v.dimensions = Alice::vec(12, 6, 8);
                v.level = 0;
                v.id = nextVoxelId++;
                buildingVoxels.push_back(v);
            }
        }

        // Commercial Tower 1 - Tall central tower (15 floors)
        for (int level = 1; level <= 15; ++level) {
            for (int x = 0; x < 2; ++x) {
                for (int z = 1; z < 3; ++z) { // Central portion
                    Voxel v;
                    v.type = WORK;
                    v.position = Alice::vec(x * 12 - 12, 6 + (level - 1) * 4, z * 8 - 16);
                    v.dimensions = Alice::vec(12, 4, 8);
                    v.level = level;
                    v.id = nextVoxelId++;
                    buildingVoxels.push_back(v);
                }
            }
        }

        // Commercial Tower 2 - Lower surrounding podium (5 floors)
        for (int level = 1; level <= 5; ++level) {
            // Front section
            for (int x = 0; x < 2; ++x) {
                Voxel v;
                v.type = WORK;
                v.position = Alice::vec(x * 12 - 12, 6 + (level - 1) * 4, -16);
                v.dimensions = Alice::vec(12, 4, 8);
                v.level = level;
                v.id = nextVoxelId++;
                buildingVoxels.push_back(v);
            }

            // Back section
            for (int x = 0; x < 2; ++x) {
                Voxel v;
                v.type = WORK;
                v.position = Alice::vec(x * 12 - 12, 6 + (level - 1) * 4, 8);
                v.dimensions = Alice::vec(12, 4, 8);
                v.level = level;
                v.id = nextVoxelId++;
                buildingVoxels.push_back(v);
            }
        }

        // Minimal residential component (2 floors at top of podium)
        for (int level = 6; level <= 7; ++level) {
            for (int x = 0; x < 3; ++x) {
                Voxel v;
                v.type = LIVE;
                v.position = Alice::vec(x * 6 - 9, 6 + (level - 1) * 4, 8);
                v.dimensions = Alice::vec(6, 4, 6);
                v.level = level;
                v.id = nextVoxelId++;
                buildingVoxels.push_back(v);
            }
        }

        cout << "Commercial Core Building created with:" << endl;
        cout << "- VISIT: Ground floor retail" << endl;
        cout << "- WORK: 15-floor central tower and 5-floor podium" << endl;
        cout << "- LIVE: Small residential component (2 floors)" << endl;
    }

    void initializeCulturalDistrict() {
        cout << "\n=== CREATING CULTURAL DISTRICT BUILDING ===" << endl;

        // Large cultural facilities (VISIT) - spread out with plazas
        vector<tuple<int, int, int, int>> culturalFacilities = {
            {0, 0, 3, 3},    // Main exhibition hall: position(0,0), size(3x3)
            {5, 0, 2, 2},    // Performance space: position(5,0), size(2x2)
            {5, 5, 2, 1},    // Gallery: position(5,5), size(2x1)
            {0, 5, 2, 2}     // Education center: position(0,5), size(2x2)
        };

        // Create large VISIT voxels for cultural facilities
        for (const auto& facility : culturalFacilities) {
            int posX = get<0>(facility);
            int posZ = get<1>(facility);
            int sizeX = get<2>(facility);
            int sizeZ = get<3>(facility);

            Voxel v;
            v.type = VISIT;
            v.position = Alice::vec(posX * 8 - 24, 0, posZ * 8 - 24);
            v.dimensions = Alice::vec(sizeX * 8, 10, sizeZ * 8); // Taller spaces for cultural venues
            v.level = 0;
            v.id = nextVoxelId++;
            buildingVoxels.push_back(v);
        }

        // Support spaces (WORK) - offices, administration (floors 1-3)
        vector<tuple<int, int, int, int>> workSpaces = {
            {0, 3, 2, 2},    // Admin offices: position(0,3), size(2x2)
            {3, 0, 2, 2},    // Support services: position(3,0), size(2x2)
            {3, 5, 2, 1}     // Research area: position(3,5), size(2x1)
        };

        for (int level = 1; level <= 3; ++level) {
            for (const auto& workspace : workSpaces) {
                int posX = get<0>(workspace);
                int posZ = get<1>(workspace);
                int sizeX = get<2>(workspace);
                int sizeZ = get<3>(workspace);

                Voxel v;
                v.type = WORK;
                v.position = Alice::vec(posX * 8 - 24, 10 + (level - 1) * 4, posZ * 8 - 24);
                v.dimensions = Alice::vec(sizeX * 8, 4, sizeZ * 8);
                v.level = level;
                v.id = nextVoxelId++;
                buildingVoxels.push_back(v);
            }
        }

        // Artist residences (LIVE) - small units above some facilities (floors 4-5)
        vector<tuple<int, int>> liveSpaces = {
            {0, 3},    // Above admin
            {1, 3},
            {0, 4},
            {1, 4},
            {3, 5},    // Above research
            {4, 5}
        };

        for (int level = 4; level <= 5; ++level) {
            for (const auto& liveSpace : liveSpaces) {
                int posX = get<0>(liveSpace);
                int posZ = get<1>(liveSpace);

                Voxel v;
                v.type = LIVE;
                v.position = Alice::vec(posX * 4 - 24, 10 + (level - 1) * 4, posZ * 4 - 24);
                v.dimensions = Alice::vec(4, 4, 4);
                v.level = level;
                v.id = nextVoxelId++;
                buildingVoxels.push_back(v);
            }
        }

        cout << "Cultural District Building created with:" << endl;
        cout << "- VISIT: Large cultural venues at ground level" << endl;
        cout << "- WORK: Support spaces (floors 1-3)" << endl;
        cout << "- LIVE: Artist residences (floors 4-5)" << endl;
    }

    void initializeTransitOriented() {
        cout << "\n=== CREATING TRANSIT-ORIENTED DEVELOPMENT ===" << endl;

        // Large transit hub at center (VISIT)
        Voxel transitHub;
        transitHub.type = VISIT;
        transitHub.position = Alice::vec(-8, 0, -8);
        transitHub.dimensions = Alice::vec(16, 8, 16);
        transitHub.level = 0;
        transitHub.id = nextVoxelId++;
        buildingVoxels.push_back(transitHub);

        // Commercial spine along "transit corridor" (WORK, floors 1-5)
        for (int level = 1; level <= 5; ++level) {
            // North corridor
            for (int z = 2; z <= 5; ++z) {
                Voxel v;
                v.type = WORK;
                v.position = Alice::vec(4, 8 + (level - 1) * 4, z * 8 - 24);
                v.dimensions = Alice::vec(8, 4, 8);
                v.level = level;
                v.id = nextVoxelId++;
                buildingVoxels.push_back(v);
            }

            // East corridor
            for (int x = 2; x <= 5; ++x) {
                Voxel v;
                v.type = WORK;
                v.position = Alice::vec(x * 8 - 24, 8 + (level - 1) * 4, 4);
                v.dimensions = Alice::vec(8, 4, 8);
                v.level = level;
                v.id = nextVoxelId++;
                buildingVoxels.push_back(v);
            }
        }

        // Residential above office (LIVE, floors 6-12)
        for (int level = 6; level <= 12; ++level) {
            // North residential tower
            for (int x = 0; x < 2; ++x) {
                for (int z = 3; z < 5; ++z) {
                    Voxel v;
                    v.type = LIVE;
                    v.position = Alice::vec(4 + x * 4, 8 + (level - 1) * 4, z * 8 - 24);
                    v.dimensions = Alice::vec(4, 4, 4);
                    v.level = level;
                    v.id = nextVoxelId++;
                    buildingVoxels.push_back(v);
                }
            }

            // East residential tower
            for (int x = 3; x < 5; ++x) {
                for (int z = 0; z < 2; ++z) {
                    Voxel v;
                    v.type = LIVE;
                    v.position = Alice::vec(x * 8 - 24, 8 + (level - 1) * 4, 4 + z * 4);
                    v.dimensions = Alice::vec(4, 4, 4);
                    v.level = level;
                    v.id = nextVoxelId++;
                    buildingVoxels.push_back(v);
                }
            }
        }

        // Additional ground-level VISIT spaces (retail, services)
        vector<tuple<int, int>> retailLocations = {
            {-2, 2}, {-2, 3}, {2, -2}, {3, -2}
        };

        for (const auto& loc : retailLocations) {
            int x = get<0>(loc);
            int z = get<1>(loc);

            Voxel v;
            v.type = VISIT;
            v.position = Alice::vec(x * 8, 0, z * 8);
            v.dimensions = Alice::vec(8, 6, 8);
            v.level = 0;
            v.id = nextVoxelId++;
            buildingVoxels.push_back(v);
        }

        cout << "Transit-Oriented Development created with:" << endl;
        cout << "- VISIT: Central transit hub and ground-level retail" << endl;
        cout << "- WORK: Commercial corridor along transit lines (floors 1-5)" << endl;
        cout << "- LIVE: Two residential towers (floors 6-12)" << endl;
    }

   
    // Helper methods

    // Helper method to get a copy of voxels with a specific type
    vector<Voxel> getVoxelsOfType(VoxelType type) {
        vector<Voxel> result;
        for (const auto& voxel : buildingVoxels) {
            if (voxel.type == type) {
                result.push_back(voxel);
            }
        }
        return result;
    }

    // Helper method to remove voxels in a specific area
    void removeVoxelsInArea(const Alice::vec& minBound, const Alice::vec& maxBound) {
        vector<int> voxelsToRemove;
        for (int i = 0; i < buildingVoxels.size(); ++i) {
            const Voxel& v = buildingVoxels[i];

            // Check if this voxel overlaps with the area
            bool overlaps = true;

            // Check X bounds
            if (v.position.x + v.dimensions.x <= minBound.x || v.position.x >= maxBound.x) {
                overlaps = false;
            }

            // Check Y bounds
            if (v.position.y + v.dimensions.y <= minBound.y || v.position.y >= maxBound.y) {
                overlaps = false;
            }

            // Check Z bounds
            if (v.position.z + v.dimensions.z <= minBound.z || v.position.z >= maxBound.z) {
                overlaps = false;
            }

            if (overlaps) {
                voxelsToRemove.push_back(i);
            }
        }

        // Remove voxels in reverse order to maintain indices
        sort(voxelsToRemove.rbegin(), voxelsToRemove.rend());
        for (int idx : voxelsToRemove) {
            buildingVoxels.erase(buildingVoxels.begin() + idx);
        }
    }

    // Apply current edge condition to the building
    void applyEdgeCondition() {
        // Skip if no special edge
        if (currentEdge == NO_SPECIAL_EDGE) {
            return;
        }

        // Determine which edge (south edge in this simplified example)
        // In a real implementation, we would consider different edges
        float southEdgeZ = -15.0f; // Approximate south edge Z position

        // Apply different edge treatments based on edge condition
        switch (currentEdge) {
        case MAIN_STREET_EDGE:
            applyMainStreetEdge(southEdgeZ);
            break;
        case WATERFRONT_EDGE:
            applyWaterfrontEdge(southEdgeZ);
            break;
        case PARK_EDGE:
            applyParkEdge(southEdgeZ);
            break;
        default:
            break;
        }
    }

    // Apply Main Street edge condition
    void applyMainStreetEdge(float edgeZ) {
        cout << "Applying Main Street Edge condition..." << endl;

        // 1. Clear existing voxels along the edge
        float edgeDepth = 10.0f;
        removeVoxelsInArea(
            Alice::vec(-50.0f, 0.0f, edgeZ - edgeDepth),
            Alice::vec(50.0f, 8.0f, edgeZ + 2.0f)
        );

        // 2. Add retail podium along the edge
        if (currentContext != LOW_DENSITY_RESIDENTIAL) {
            // Determine width based on context
            float width = 40.0f;
            if (currentContext == HIGH_DENSITY_MIXED_USE || currentContext == COMMERCIAL_CORE) {
                width = 50.0f;
            }

            // Add continuous retail with display windows
            for (float x = -width / 2; x < width / 2; x += 10.0f) {
                float segmentWidth = min(10.0f, width / 2 - x);

                Voxel retail;
                retail.type = VISIT;
                retail.position = Alice::vec(x, 0.0f, edgeZ - edgeDepth);
                retail.dimensions = Alice::vec(segmentWidth, 7.0f, edgeDepth);
                retail.level = 0;
                retail.id = nextVoxelId++;
                retail.features = { "street_retail", "display_windows", "main_entrance" };
                buildingVoxels.push_back(retail);

                // Add awning/canopy
                Voxel awning;
                awning.type = TRANSITION;
                awning.program = RETAIL_PODIUM;
                awning.position = Alice::vec(x, 6.0f, edgeZ - edgeDepth - 1.5f);
                awning.dimensions = Alice::vec(segmentWidth, 1.0f, 2.0f);
                awning.level = 1;
                awning.id = nextVoxelId++;
                awning.features = { "weather_protection", "signage", "street_presence" };
                buildingVoxels.push_back(awning);
            }
        }
        else {
            // For residential, add small-scale storefronts
            for (float x = -20.0f; x < 20.0f; x += 8.0f) {
                float segmentWidth = min(7.0f, 20.0f - x);

                Voxel retail;
                retail.type = VISIT;
                retail.position = Alice::vec(x, 0.0f, edgeZ - edgeDepth);
                retail.dimensions = Alice::vec(segmentWidth, 5.0f, edgeDepth / 2);
                retail.level = 0;
                retail.id = nextVoxelId++;
                retail.features = { "neighborhood_retail", "cafe", "local_services" };
                buildingVoxels.push_back(retail);
            }
        }
    }

    // Apply Waterfront edge condition
    void applyWaterfrontEdge(float edgeZ) {
        cout << "Applying Waterfront Edge condition..." << endl;

        // 1. Clear existing voxels along the edge
        float edgeDepth = 15.0f;
        removeVoxelsInArea(
            Alice::vec(-50.0f, 0.0f, edgeZ - edgeDepth),
            Alice::vec(50.0f, 20.0f, edgeZ + 2.0f)
        );

        // 2. Add waterfront promenade
        Voxel promenade;
        promenade.type = VISIT;
        promenade.position = Alice::vec(-25.0f, 0.0f, edgeZ - 6.0f);
        promenade.dimensions = Alice::vec(50.0f, 4.0f, 6.0f);
        promenade.level = 0;
        promenade.id = nextVoxelId++;
        promenade.features = { "public_promenade", "waterfront_access", "seating" };
        buildingVoxels.push_back(promenade);

        // 3. Add terraced form based on context
        int numTerraces = 0;
        VoxelType terraceType = TRANSITION;

        if (currentContext == CULTURAL_DISTRICT) {
            numTerraces = 3;
            terraceType = VISIT;
        }
        else if (currentContext == LOW_DENSITY_RESIDENTIAL) {
            numTerraces = 2;
            terraceType = LIVE;
        }
        else {
            numTerraces = 4;
            terraceType = WORK;
        }

        // Create stepped terraces
        for (int i = 0; i < numTerraces; i++) {
            float setback = 6.0f + i * 5.0f;

            Voxel terrace;
            terrace.type = terraceType;
            terrace.position = Alice::vec(-25.0f, (i + 1) * 4.0f, edgeZ - setback - 10.0f);
            terrace.dimensions = Alice::vec(50.0f, 4.0f, 10.0f);
            terrace.level = i + 1;
            terrace.id = nextVoxelId++;
            terrace.features = { "water_views", "outdoor_space", "premium_position" };
            buildingVoxels.push_back(terrace);

            // Add terrace/balcony at each level
            Voxel terraceDeck;
            terraceDeck.type = TRANSITION;
            terraceDeck.program = SHARED_TERRACE;
            terraceDeck.position = Alice::vec(-25.0f, (i + 1) * 4.0f, edgeZ - i * 5.0f - 6.0f);
            terraceDeck.dimensions = Alice::vec(50.0f, 1.5f, 5.0f);
            terraceDeck.level = i + 1;
            terraceDeck.id = nextVoxelId++;
            terraceDeck.features = { "waterfront_terrace", "shading_devices", "planters" };
            buildingVoxels.push_back(terraceDeck);
        }
    }

    // Apply Park edge condition
    void applyParkEdge(float edgeZ) {
        cout << "Applying Park Edge condition..." << endl;

        // 1. Clear existing voxels along the edge
        float edgeDepth = 12.0f;
        removeVoxelsInArea(
            Alice::vec(-50.0f, 0.0f, edgeZ - edgeDepth),
            Alice::vec(50.0f, 16.0f, edgeZ + 2.0f)
        );

        // 2. Different park interfaces based on context
        if (currentContext == CULTURAL_DISTRICT) {
            // Indoor/outdoor cultural space facing park
            Voxel culturalSpace;
            culturalSpace.type = VISIT;
            culturalSpace.position = Alice::vec(-25.0f, 0.0f, edgeZ - edgeDepth);
            culturalSpace.dimensions = Alice::vec(50.0f, 8.0f, edgeDepth);
            culturalSpace.level = 0;
            culturalSpace.id = nextVoxelId++;
            culturalSpace.features = { "indoor_outdoor_gallery", "operable_facade", "park_extension" };
            buildingVoxels.push_back(culturalSpace);

        }
        else if (currentContext == LOW_DENSITY_RESIDENTIAL) {
            // Townhouse units facing park
            for (float x = -20.0f; x < 20.0f; x += 8.0f) {
                float unitWidth = min(7.0f, 20.0f - x);

                Voxel townhouse;
                townhouse.type = LIVE;
                townhouse.position = Alice::vec(x, 0.0f, edgeZ - edgeDepth + 4.0f);
                townhouse.dimensions = Alice::vec(unitWidth, 12.0f, 8.0f);
                townhouse.level = 0;
                townhouse.id = nextVoxelId++;
                townhouse.features = { "park_facing", "private_garden", "multiple_levels" };
                buildingVoxels.push_back(townhouse);

                // Private garden connection to park
                Voxel garden;
                garden.type = TRANSITION;
                garden.program = COMMUNITY_GARDEN;
                garden.position = Alice::vec(x, 0.0f, edgeZ - 4.0f);
                garden.dimensions = Alice::vec(unitWidth, 1.0f, 4.0f);
                garden.level = 0;
                garden.id = nextVoxelId++;
                garden.features = { "private_garden", "park_transition", "green_buffer" };
                buildingVoxels.push_back(garden);
            }
        }
        else {
            // Commercial/mixed-use park interface with green terraces
            Voxel retail;
            retail.type = VISIT;
            retail.position = Alice::vec(-25.0f, 0.0f, edgeZ - edgeDepth);
            retail.dimensions = Alice::vec(50.0f, 6.0f, 8.0f);
            retail.level = 0;
            retail.id = nextVoxelId++;
            retail.features = { "park_facing_retail", "outdoor_dining", "transparent_facade" };
            buildingVoxels.push_back(retail);

            // Green terraces stepping up
            for (int level = 1; level <= 3; level++) {
                Voxel terrace;
                terrace.type = TRANSITION;
                terrace.program = GREEN_ROOF;
                terrace.position = Alice::vec(-25.0f, level * 4.0f, edgeZ - edgeDepth + (level - 1) * 2.0f);
                terrace.dimensions = Alice::vec(50.0f, 1.5f, 6.0f);
                terrace.level = level;
                terrace.id = nextVoxelId++;
                terrace.features = { "cascading_gardens", "rainwater_management", "habitat_creation" };
                buildingVoxels.push_back(terrace);
            }
        }
    }

    // Apply current corner condition to the building
    void applyCornerCondition() {
        // Skip if no special corner
        if (currentCorner == NO_SPECIAL_CORNER) {
            return;
        }

        // In this example, we'll use the southwest corner (could be parameterized for different corners)
        Alice::vec cornerPos = Alice::vec(-25.0f, 0.0f, -15.0f); // Southwest corner position

        // Apply different corner treatments based on corner condition
        switch (currentCorner) {
        case PLAZA_CORNER:
            applyPlazaCorner(cornerPos);
            break;
        case LANDMARK_CORNER:
            applyLandmarkCorner(cornerPos);
            break;
        case TERRACED_CORNER:
            applyTerracedCorner(cornerPos);
            break;
        default:
            break;
        }
    }

    // Apply Plaza corner condition
    void applyPlazaCorner(const Alice::vec& cornerPos) {
        cout << "Applying Plaza Corner condition..." << endl;

        // 1. Clear existing voxels in the corner area
        float cornerSize = 15.0f;
        removeVoxelsInArea(
            Alice::vec(cornerPos.x - cornerSize / 2, 0.0f, cornerPos.z - cornerSize / 2),
            Alice::vec(cornerPos.x + cornerSize / 2, 20.0f, cornerPos.z + cornerSize / 2)
        );

        // 2. Create recessed corner plaza
        // The plaza itself is just a void (no voxel)

        // 3. Add entrance lobby adjacent to plaza
        Voxel lobby;
        lobby.type = VISIT;
        lobby.position = Alice::vec(cornerPos.x + cornerSize / 2 - 10.0f, 0.0f, cornerPos.z + cornerSize / 2 - 10.0f);
        lobby.dimensions = Alice::vec(10.0f, 8.0f, 10.0f);
        lobby.level = 0;
        lobby.id = nextVoxelId++;
        lobby.features = { "plaza_entrance", "double_height_lobby", "public_access" };
        buildingVoxels.push_back(lobby);

        // 4. Add cantilevered upper floors over plaza (dependent on context)
        if (currentContext != LOW_DENSITY_RESIDENTIAL) {
            int numFloors = 6; // Default
            VoxelType upperType = WORK;

            if (currentContext == HIGH_DENSITY_MIXED_USE) {
                numFloors = 8;
            }
            else if (currentContext == COMMERCIAL_CORE) {
                numFloors = 10;
            }
            else if (currentContext == CULTURAL_DISTRICT) {
                numFloors = 4;
                upperType = VISIT;
            }

            Voxel cantilever;
            cantilever.type = upperType;
            cantilever.position = Alice::vec(cornerPos.x - cornerSize / 2, 8.0f, cornerPos.z - cornerSize / 2);
            cantilever.dimensions = Alice::vec(cornerSize, 4.0f * numFloors, cornerSize);
            cantilever.level = 2;
            cantilever.id = nextVoxelId++;
            cantilever.features = { "cantilever", "corner_offices", "structural_expression" };
            buildingVoxels.push_back(cantilever);

            // Add signature architectural element to cantilever edge
            Voxel edgeFeature;
            edgeFeature.type = TRANSITION;
            edgeFeature.program = SKY_LOBBY;
            edgeFeature.position = Alice::vec(cornerPos.x - cornerSize / 2, 8.0f, cornerPos.z - cornerSize / 2);
            edgeFeature.dimensions = Alice::vec(cornerSize, 2.0f, 1.0f);
            edgeFeature.level = 2;
            edgeFeature.id = nextVoxelId++;
            edgeFeature.features = { "edge_detail", "lighting_feature", "facade_articulation" };
            buildingVoxels.push_back(edgeFeature);
        }
    }

    // Apply Landmark corner condition
    void applyLandmarkCorner(const Alice::vec& cornerPos) {
        cout << "Applying Landmark Corner condition..." << endl;

        // 1. Clear existing voxels in the corner area
        float cornerSize = 18.0f;
        removeVoxelsInArea(
            Alice::vec(cornerPos.x - cornerSize / 2, 0.0f, cornerPos.z - cornerSize / 2),
            Alice::vec(cornerPos.x + cornerSize / 2, 50.0f, cornerPos.z + cornerSize / 2)
        );

        // 2. Create a taller, signature element at the corner - context specific
        if (currentContext == COMMERCIAL_CORE || currentContext == HIGH_DENSITY_MIXED_USE) {
            // Tall signature tower
            int towerHeight = (currentContext == COMMERCIAL_CORE) ? 25 : 20;
            float towerWidth = (currentContext == COMMERCIAL_CORE) ? 16.0f : 14.0f;

            // Base of signature tower
            Voxel towerBase;
            towerBase.type = VISIT;
            towerBase.position = Alice::vec(cornerPos.x - towerWidth / 2, 0.0f, cornerPos.z - towerWidth / 2);
            towerBase.dimensions = Alice::vec(towerWidth, 8.0f, towerWidth);
            towerBase.level = 0;
            towerBase.id = nextVoxelId++;
            towerBase.features = { "grand_entrance", "atrium", "feature_stair" };
            buildingVoxels.push_back(towerBase);

            // Tower shaft
            Voxel towerShaft;
            towerShaft.type = WORK;
            towerShaft.position = Alice::vec(cornerPos.x - towerWidth / 2, 8.0f, cornerPos.z - towerWidth / 2);
            towerShaft.dimensions = Alice::vec(towerWidth, 4.0f * towerHeight, towerWidth);
            towerShaft.level = 2;
            towerShaft.id = nextVoxelId++;
            towerShaft.features = { "signature_element", "premium_office", "corner_views" };
            buildingVoxels.push_back(towerShaft);

            // Crown element
            Voxel crown;
            crown.type = TRANSITION;
            crown.program = SKY_LOBBY;
            crown.position = Alice::vec(cornerPos.x - towerWidth / 2, 8.0f + 4.0f * towerHeight, cornerPos.z - towerWidth / 2);
            crown.dimensions = Alice::vec(towerWidth, 8.0f, towerWidth);
            crown.level = towerHeight + 2;
            crown.id = nextVoxelId++;
            crown.features = { "crown_lighting", "observation_deck", "mechanical_penthouse" };
            buildingVoxels.push_back(crown);

        }
        else if (currentContext == CULTURAL_DISTRICT) {
            // Sculptural cultural landmark
            Voxel cultural;
            cultural.type = VISIT;
            cultural.position = Alice::vec(cornerPos.x - cornerSize / 2, 0.0f, cornerPos.z - cornerSize / 2);
            cultural.dimensions = Alice::vec(cornerSize, 16.0f, cornerSize);
            cultural.level = 0;
            cultural.id = nextVoxelId++;
            cultural.features = { "iconic_element", "cultural_venue", "public_attraction" };
            buildingVoxels.push_back(cultural);

        }
        else if (currentContext == LOW_DENSITY_RESIDENTIAL) {
            // Community focal point
            Voxel community;
            community.type = VISIT;
            community.position = Alice::vec(cornerPos.x - cornerSize / 3, 0.0f, cornerPos.z - cornerSize / 3);
            community.dimensions = Alice::vec(cornerSize * 2 / 3, 10.0f, cornerSize * 2 / 3);
            community.level = 0;
            community.id = nextVoxelId++;
            community.features = { "community_center", "neighborhood_amenity", "local_landmark" };
            buildingVoxels.push_back(community);

        }
        else { // TRANSIT_ORIENTED
            // Transit connection feature
            Voxel transit;
            transit.type = VISIT;
            transit.position = Alice::vec(cornerPos.x - cornerSize / 2, 0.0f, cornerPos.z - cornerSize / 2);
            transit.dimensions = Alice::vec(cornerSize, 12.0f, cornerSize);
            transit.level = 0;
            transit.id = nextVoxelId++;
            transit.features = { "transit_entrance", "multi_level_concourse", "interchange_point" };
            buildingVoxels.push_back(transit);

            Voxel transitTower;
            transitTower.type = VISIT;
            transitTower.position = Alice::vec(cornerPos.x - cornerSize / 4, 12.0f, cornerPos.z - cornerSize / 4);
            transitTower.dimensions = Alice::vec(cornerSize / 2, 16.0f, cornerSize / 2);
            transitTower.level = 3;
            transitTower.id = nextVoxelId++;
            transitTower.features = { "clock_tower", "wayfinding_element", "transit_indicator" };
            buildingVoxels.push_back(transitTower);
        }
    }

    // Apply Terraced corner condition
    void applyTerracedCorner(const Alice::vec& cornerPos) {
        cout << "Applying Terraced Corner condition..." << endl;

        // 1. Clear existing voxels in the corner area
        float cornerSize = 16.0f;
        removeVoxelsInArea(
            Alice::vec(cornerPos.x - cornerSize / 2, 0.0f, cornerPos.z - cornerSize / 2),
            Alice::vec(cornerPos.x + cornerSize / 2, 30.0f, cornerPos.z + cornerSize / 2)
        );

        // 2. Create stepped terraces - context specific
        // Calculate number of terraces based on context and sizes
        int numTerraces = 0;
        float baseSize = 0;
        VoxelType terraceVoxelType = LIVE;

        if (currentContext == LOW_DENSITY_RESIDENTIAL) {
            numTerraces = 4;
            baseSize = cornerSize;
        }
        else if (currentContext == HIGH_DENSITY_MIXED_USE) {
            numTerraces = 6;
            baseSize = cornerSize;
            terraceVoxelType = WORK;
        }
        else if (currentContext == COMMERCIAL_CORE) {
            numTerraces = 5;
            baseSize = cornerSize;
            terraceVoxelType = WORK;
        }
        else if (currentContext == CULTURAL_DISTRICT) {
            numTerraces = 3;
            baseSize = cornerSize;
            terraceVoxelType = VISIT;
        }
        else { // TRANSIT_ORIENTED
            numTerraces = 5;
            baseSize = cornerSize;
            terraceVoxelType = WORK;
        }

        // Create staggered terraced form
        for (int i = 0; i < numTerraces; i++) {
            float reduction = (float)i / numTerraces * baseSize;
            float currentSize = baseSize - reduction;

            Voxel terracedFloor;
            terracedFloor.type = terraceVoxelType;
            terracedFloor.position = Alice::vec(
                cornerPos.x - baseSize / 2 + reduction / 2,
                i * 4.0f,
                cornerPos.z - baseSize / 2 + reduction / 2
            );
            terracedFloor.dimensions = Alice::vec(currentSize, 4.0f, currentSize);
            terracedFloor.level = i;
            terracedFloor.id = nextVoxelId++;
            terracedFloor.features = { "terraced_form", "corner_element", "stepped_massing" };
            buildingVoxels.push_back(terracedFloor);

            // Add terrace garden at the step
            if (i > 0) {
                Voxel terrace;
                terrace.type = TRANSITION;
                terrace.program = SHARED_TERRACE;
                terrace.position = Alice::vec(
                    cornerPos.x - baseSize / 2 + (reduction - baseSize / numTerraces) / 2,
                    i * 4.0f,
                    cornerPos.z - baseSize / 2 + (reduction - baseSize / numTerraces) / 2
                );
                terrace.dimensions = Alice::vec(baseSize / numTerraces, 1.0f, baseSize / numTerraces);
                terrace.level = i;
                terrace.id = nextVoxelId++;
                terrace.features = { "corner_garden", "view_terrace", "green_step" };
                buildingVoxels.push_back(terrace);
            }
        }
    }

    void generateTransitions() {
        vector<TransitionOpportunity> opportunities = findTransitionOpportunities();
        cout << "Found " << opportunities.size() << " transition opportunities" << endl;

        // Process top opportunities
        int processedCount = 0;
        int skippedCount = 0;
        for (const auto& opp : opportunities) {
            if (processedCount >= 10) break; // Limit for testing

            Voxel transition = generator.generateTransition(*opp.voxelA, *opp.voxelB, opp.transitionType, buildingVoxels);

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
        // Draw context name in 3D space
        string contextLabel = "Context: " + contextNames[currentContext];
        glColor3f(0.0f, 0.0f, 0.0f);
        glRasterPos3f(-20, 50, -20);
        for (char c : contextLabel) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        }

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

    // New method to visualize active edge and corner conditions
    void visualizeActiveConditions();
};

// Global instance
ProgramTransitionSystem* transitionSystem = nullptr;

// Global UI variables
UrbanContext currentContext = HIGH_DENSITY_MIXED_USE;
EdgeCondition currentEdge = NO_SPECIAL_EDGE;
CornerCondition currentCorner = NO_SPECIAL_CORNER;
float contextSliderValue = 0.0f; // 0.0 to 1.0
float edgeSliderValue = 0.0f;    // 0.0 to 1.0
float cornerSliderValue = 0.0f;  // 0.0 to 1.0
bool contextSliderDragging = false;
bool edgeSliderDragging = false;
bool cornerSliderDragging = false;

// UI positioning constants - positioned at bottom left
const float SLIDER_X = 20.0f;
const float SLIDER_Y_CONTEXT = 60.0f;   // From bottom of screen
const float SLIDER_Y_EDGE = 120.0f;     // From bottom of screen
const float SLIDER_Y_CORNER = 180.0f;   // From bottom of screen
const float SLIDER_WIDTH = 1000.0f;
const float SLIDER_HEIGHT = 20.0f;
const float SLIDER_MARKER_WIDTH = 10.0f;

// ===================== UI DRAWING FUNCTIONS =====================

void drawSliders() {
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

    // 1. Draw Context Slider
    // Draw slider background
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(SLIDER_X, SLIDER_Y_CONTEXT);
    glVertex2f(SLIDER_X + SLIDER_WIDTH, SLIDER_Y_CONTEXT);
    glVertex2f(SLIDER_X + SLIDER_WIDTH, SLIDER_Y_CONTEXT + SLIDER_HEIGHT);
    glVertex2f(SLIDER_X, SLIDER_Y_CONTEXT + SLIDER_HEIGHT);
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
        glVertex2f(x + 1, SLIDER_Y_CONTEXT + 1);
        glVertex2f(x + sectionWidth - 1, SLIDER_Y_CONTEXT + 1);
        glVertex2f(x + sectionWidth - 1, SLIDER_Y_CONTEXT + SLIDER_HEIGHT - 1);
        glVertex2f(x + 1, SLIDER_Y_CONTEXT + SLIDER_HEIGHT - 1);
        glEnd();
    }

    // Draw context slider marker
    float markerX = SLIDER_X + contextSliderValue * SLIDER_WIDTH - SLIDER_MARKER_WIDTH / 2;
    glColor3f(1.0f, 1.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(markerX, SLIDER_Y_CONTEXT - 5);
    glVertex2f(markerX + SLIDER_MARKER_WIDTH, SLIDER_Y_CONTEXT - 5);
    glVertex2f(markerX + SLIDER_MARKER_WIDTH, SLIDER_Y_CONTEXT + SLIDER_HEIGHT + 5);
    glVertex2f(markerX, SLIDER_Y_CONTEXT + SLIDER_HEIGHT + 5);
    glEnd();

    // Draw context label
    glColor3f(0.0f, 0.0f, 0.0f);
    glRasterPos2f(SLIDER_X, SLIDER_Y_CONTEXT + SLIDER_HEIGHT + 15);
    string contextTitle = "Urban Context: " + contextNames[currentContext];
    for (char c : contextTitle) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }

    // Draw context options labels
    for (int i = 0; i < 5; ++i) {
        float x = SLIDER_X + i * sectionWidth + 5;
        float y = SLIDER_Y_CONTEXT - 15;  // Below the slider

        // Simple text rendering using bitmap characters
        string text = contextNames[i];
        glRasterPos2f(x, y);
        for (char c : text) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        }
    }

    // 2. Draw Edge Condition Slider
    // Draw slider background
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(SLIDER_X, SLIDER_Y_EDGE);
    glVertex2f(SLIDER_X + SLIDER_WIDTH, SLIDER_Y_EDGE);
    glVertex2f(SLIDER_X + SLIDER_WIDTH, SLIDER_Y_EDGE + SLIDER_HEIGHT);
    glVertex2f(SLIDER_X, SLIDER_Y_EDGE + SLIDER_HEIGHT);
    glEnd();

    // Draw slider sections for each edge condition
    sectionWidth = SLIDER_WIDTH / 4.0f; // 4 edge conditions
    for (int i = 0; i < 4; ++i) {
        float x = SLIDER_X + i * sectionWidth;

        // Highlight current edge section
        if (currentEdge == i) {
            glColor3f(0.8f, 0.4f, 0.4f); // Red tone for edge
        }
        else {
            glColor3f(0.3f, 0.3f, 0.3f);
        }

        glBegin(GL_QUADS);
        glVertex2f(x + 1, SLIDER_Y_EDGE + 1);
        glVertex2f(x + sectionWidth - 1, SLIDER_Y_EDGE + 1);
        glVertex2f(x + sectionWidth - 1, SLIDER_Y_EDGE + SLIDER_HEIGHT - 1);
        glVertex2f(x + 1, SLIDER_Y_EDGE + SLIDER_HEIGHT - 1);
        glEnd();
    }

    // Draw edge slider marker
    markerX = SLIDER_X + edgeSliderValue * SLIDER_WIDTH - SLIDER_MARKER_WIDTH / 2;
    glColor3f(1.0f, 0.5f, 0.5f); // Light red
    glBegin(GL_QUADS);
    glVertex2f(markerX, SLIDER_Y_EDGE - 5);
    glVertex2f(markerX + SLIDER_MARKER_WIDTH, SLIDER_Y_EDGE - 5);
    glVertex2f(markerX + SLIDER_MARKER_WIDTH, SLIDER_Y_EDGE + SLIDER_HEIGHT + 5);
    glVertex2f(markerX, SLIDER_Y_EDGE + SLIDER_HEIGHT + 5);
    glEnd();

    // Draw edge label
    glColor3f(0.0f, 0.0f, 0.0f);
    glRasterPos2f(SLIDER_X, SLIDER_Y_EDGE + SLIDER_HEIGHT + 15);
    string edgeTitle = "Edge Condition: " + edgeConditionNames[currentEdge];
    for (char c : edgeTitle) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }

    // Draw edge options labels
    for (int i = 0; i < 4; ++i) {
        float x = SLIDER_X + i * sectionWidth + 5;
        float y = SLIDER_Y_EDGE - 15;  // Below the slider

        string text = edgeConditionNames[i];
        glRasterPos2f(x, y);
        for (char c : text) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        }
    }

    // 3. Draw Corner Condition Slider
    // Draw slider background
    glColor3f(0.2f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(SLIDER_X, SLIDER_Y_CORNER);
    glVertex2f(SLIDER_X + SLIDER_WIDTH, SLIDER_Y_CORNER);
    glVertex2f(SLIDER_X + SLIDER_WIDTH, SLIDER_Y_CORNER + SLIDER_HEIGHT);
    glVertex2f(SLIDER_X, SLIDER_Y_CORNER + SLIDER_HEIGHT);
    glEnd();

    // Draw slider sections for each corner condition
    sectionWidth = SLIDER_WIDTH / 4.0f; // 4 corner conditions
    for (int i = 0; i < 4; ++i) {
        float x = SLIDER_X + i * sectionWidth;

        // Highlight current corner section
        if (currentCorner == i) {
            glColor3f(0.4f, 0.8f, 0.4f); // Green tone for corner
        }
        else {
            glColor3f(0.3f, 0.3f, 0.3f);
        }

        glBegin(GL_QUADS);
        glVertex2f(x + 1, SLIDER_Y_CORNER + 1);
        glVertex2f(x + sectionWidth - 1, SLIDER_Y_CORNER + 1);
        glVertex2f(x + sectionWidth - 1, SLIDER_Y_CORNER + SLIDER_HEIGHT - 1);
        glVertex2f(x + 1, SLIDER_Y_CORNER + SLIDER_HEIGHT - 1);
        glEnd();
    }

    // Draw corner slider marker
    markerX = SLIDER_X + cornerSliderValue * SLIDER_WIDTH - SLIDER_MARKER_WIDTH / 2;
    glColor3f(0.5f, 1.0f, 0.5f); // Light green
    glBegin(GL_QUADS);
    glVertex2f(markerX, SLIDER_Y_CORNER - 5);
    glVertex2f(markerX + SLIDER_MARKER_WIDTH, SLIDER_Y_CORNER - 5);
    glVertex2f(markerX + SLIDER_MARKER_WIDTH, SLIDER_Y_CORNER + SLIDER_HEIGHT + 5);
    glVertex2f(markerX, SLIDER_Y_CORNER + SLIDER_HEIGHT + 5);
    glEnd();

    // Draw corner label
    glColor3f(0.0f, 0.0f, 0.0f);
    glRasterPos2f(SLIDER_X, SLIDER_Y_CORNER + SLIDER_HEIGHT + 15);
    string cornerTitle = "Corner Condition: " + cornerConditionNames[currentCorner];
    for (char c : cornerTitle) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }

    // Draw corner options labels
    for (int i = 0; i < 4; ++i) {
        float x = SLIDER_X + i * sectionWidth + 5;
        float y = SLIDER_Y_CORNER - 15;  // Below the slider

        string text = cornerConditionNames[i];
        glRasterPos2f(x, y);
        for (char c : text) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        }
    }

    // Restore matrices
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void updateContextFromSlider() {
    int newContext = (int)(contextSliderValue * 4.999f); // Convert to 0-4
    if (newContext != currentContext) {
        currentContext = (UrbanContext)newContext;
        if (transitionSystem) {
            transitionSystem->updateContext(currentContext);
        }
    }
}

void updateEdgeFromSlider() {
    int newEdge = (int)(edgeSliderValue * 3.999f); // Convert to 0-3
    if (newEdge != currentEdge) {
        currentEdge = (EdgeCondition)newEdge;
        if (transitionSystem) {
            transitionSystem->updateEdgeCondition(currentEdge);
        }
    }
}

void updateCornerFromSlider() {
    int newCorner = (int)(cornerSliderValue * 3.999f); // Convert to 0-3
    if (newCorner != currentCorner) {
        currentCorner = (CornerCondition)newCorner;
        if (transitionSystem) {
            transitionSystem->updateCornerCondition(currentCorner);
        }
    }
}

// ============== ALICE FRAMEWORK FUNCTIONS ==============

void setup()
{
    cout << "=== Program Transition Logic System - SITE-SPECIFIC VERSION ===" << endl;
    cout << "Initializing transition system..." << endl;

    transitionSystem = new ProgramTransitionSystem();

    // Initialize with default context and conditions
    transitionSystem->updateContext(currentContext);

    cout << "Setup complete!" << endl;
    cout << "*** CONTROLS ***" << endl;
    cout << "Use the THREE SLIDERS (bottom-left) to change:" << endl;
    cout << "1. Urban Context - Overall building typology" << endl;
    cout << "2. Edge Condition - How the building meets the street/waterfront/park" << endl;
    cout << "3. Corner Condition - Special treatment for building corners" << endl;
    cout << "Press 'g' to regenerate transitions" << endl;
    cout << "Press 'r' to reset" << endl;

    cout << "\n*** URBAN CONTEXTS & BUILDING TYPES ***" << endl;
    cout << "1. HIGH DENSITY MIXED-USE: Tall towers with retail base, office mid-section, residential top" << endl;
    cout << "2. LOW DENSITY RESIDENTIAL: G+4 residential building with garden spaces" << endl;
    cout << "3. COMMERCIAL CORE: Tall office tower with minimal residential" << endl;
    cout << "4. CULTURAL DISTRICT: Cultural venues with support offices and artist residences" << endl;
    cout << "5. TRANSIT-ORIENTED: Development centered around transit hub with commercial corridor" << endl;

    cout << "\n*** EDGE CONDITIONS ***" << endl;
    cout << "1. NO SPECIAL EDGE: Standard building edge" << endl;
    cout << "2. MAIN STREET EDGE: Active retail frontage with awnings" << endl;
    cout << "3. WATERFRONT EDGE: Terraced form stepping to water with promenade" << endl;
    cout << "4. PARK EDGE: Green interface with recreational spaces" << endl;

    cout << "\n*** CORNER CONDITIONS ***" << endl;
    cout << "1. NO SPECIAL CORNER: Standard building corner" << endl;
    cout << "2. PLAZA CORNER: Recessed corner with public space" << endl;
    cout << "3. LANDMARK CORNER: Distinctive architectural element at corner" << endl;
    cout << "4. TERRACED CORNER: Stepped gardens at building corner" << endl;

    cout << "\n*** COLOR SCHEME ***" << endl;
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

    cout << "\nAs you change the sliders, the building will adapt to different conditions!" << endl;
}

bool compute = false;

void update(int value)
{
    // Update logic can be added here
}

void draw()
{
    // Clear the background with a light color
    backGround(1.0);
    //drawGrid(50);

    // First draw the main building
    if (transitionSystem) {
        transitionSystem->drawVoxels();

        // Then add our new visualization for active edge and corner conditions
        transitionSystem->visualizeActiveConditions();
    }

    // Draw UI sliders on top
    drawSliders();
}

void keyPress(unsigned char k, int xm, int ym)
{
    if (k == 'g') {
        cout << "Generating transitions..." << endl;
        if (transitionSystem) {
            transitionSystem->generateTransitions();
        }
    }
    else if (k == 'r') {
        cout << "Resetting system with current context settings..." << endl;

        // Store current settings before deleting the system
        UrbanContext currentCtx = currentContext;
        EdgeCondition currentEdgeType = currentEdge;
        CornerCondition currentCornerType = currentCorner;

        // Delete and recreate the system
        delete transitionSystem;
        transitionSystem = new ProgramTransitionSystem();

        // Apply the stored settings
        transitionSystem->updateContext(currentCtx);
        transitionSystem->updateEdgeCondition(currentEdgeType);
        transitionSystem->updateCornerCondition(currentCornerType);
    }
}

void mousePress(int b, int state, int x, int y)
{
    if (state == GLUT_DOWN && b == GLUT_LEFT_BUTTON) {
        // In OpenGL, y=0 is at bottom of window
        int viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        y = viewport[3] - y;  // Convert to OpenGL coordinates

        // Check if click is on context slider
        if (x >= SLIDER_X && x <= SLIDER_X + SLIDER_WIDTH &&
            y >= SLIDER_Y_CONTEXT - 5 && y <= SLIDER_Y_CONTEXT + SLIDER_HEIGHT + 5) {
            contextSliderDragging = true;
            contextSliderValue = (x - SLIDER_X) / SLIDER_WIDTH;
            contextSliderValue = max(0.0f, min(1.0f, contextSliderValue));
            updateContextFromSlider();
        }
        // Check if click is on edge slider
        else if (x >= SLIDER_X && x <= SLIDER_X + SLIDER_WIDTH &&
            y >= SLIDER_Y_EDGE - 5 && y <= SLIDER_Y_EDGE + SLIDER_HEIGHT + 5) {
            edgeSliderDragging = true;
            edgeSliderValue = (x - SLIDER_X) / SLIDER_WIDTH;
            edgeSliderValue = max(0.0f, min(1.0f, edgeSliderValue));
            updateEdgeFromSlider();
        }
        // Check if click is on corner slider
        else if (x >= SLIDER_X && x <= SLIDER_X + SLIDER_WIDTH &&
            y >= SLIDER_Y_CORNER - 5 && y <= SLIDER_Y_CORNER + SLIDER_HEIGHT + 5) {
            cornerSliderDragging = true;
            cornerSliderValue = (x - SLIDER_X) / SLIDER_WIDTH;
            cornerSliderValue = max(0.0f, min(1.0f, cornerSliderValue));
            updateCornerFromSlider();
        }
    }
    else if (state == GLUT_UP && b == GLUT_LEFT_BUTTON) {
        contextSliderDragging = false;
        edgeSliderDragging = false;
        cornerSliderDragging = false;
    }
}

void mouseMotion(int x, int y)
{
    int viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    y = viewport[3] - y;  // Convert to OpenGL coordinates

    if (contextSliderDragging) {
        contextSliderValue = (x - SLIDER_X) / SLIDER_WIDTH;
        contextSliderValue = max(0.0f, min(1.0f, contextSliderValue));
        updateContextFromSlider();
    }
    else if (edgeSliderDragging) {
        edgeSliderValue = (x - SLIDER_X) / SLIDER_WIDTH;
        edgeSliderValue = max(0.0f, min(1.0f, edgeSliderValue));
        updateEdgeFromSlider();
    }
    else if (cornerSliderDragging) {
        cornerSliderValue = (x - SLIDER_X) / SLIDER_WIDTH;
        cornerSliderValue = max(0.0f, min(1.0f, cornerSliderValue));
        updateCornerFromSlider();
    }
}
void ProgramTransitionSystem::visualizeActiveConditions() {
    // Skip visualization if no special conditions are set
    if (currentEdge == NO_SPECIAL_EDGE && currentCorner == NO_SPECIAL_CORNER) {
        return;
    }

    // Constants for visualization
    float arrowSize = 5.0f;
    float arrowHeight = 3.0f;
    float cornerMarkerSize = 8.0f;
    float arrowOffset = 20.0f;  // Distance from building
    float labelHeight = 5.0f;   // Height for text labels
    float edgePos = -15.0f;     // South edge Z position (should match code's internal value)

    // ============== Edge Condition Visualization ==============
    if (currentEdge != NO_SPECIAL_EDGE) {
        // Edge color based on condition type
        float r = 0.0f, g = 0.0f, b = 0.0f;
        switch (currentEdge) {
        case MAIN_STREET_EDGE:
            r = 1.0f; g = 0.4f; b = 0.4f; // Red tone
            break;
        case WATERFRONT_EDGE:
            r = 0.4f; g = 0.4f; b = 1.0f; // Blue tone
            break;
        case PARK_EDGE:
            r = 0.4f; g = 1.0f; b = 0.4f; // Green tone
            break;
        }

        // Draw edge highlight - line along the south edge
        glLineWidth(4.0f);
        glColor3f(r, g, b);
        glBegin(GL_LINES);
        glVertex3f(-25.0f, 0.1f, edgePos);
        glVertex3f(25.0f, 0.1f, edgePos);
        glEnd();

        // Draw arrows pointing at the edge
        for (float x = -20.0f; x <= 20.0f; x += 10.0f) {
            glBegin(GL_TRIANGLES);
            // Arrow head
            glVertex3f(x, labelHeight, edgePos - arrowOffset);
            glVertex3f(x - arrowSize / 2, labelHeight, edgePos - arrowOffset - arrowSize);
            glVertex3f(x + arrowSize / 2, labelHeight, edgePos - arrowOffset - arrowSize);
            glEnd();

            // Arrow line
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            glVertex3f(x, labelHeight, edgePos - arrowOffset - arrowSize);
            glVertex3f(x, labelHeight, edgePos - 2.0f); // Point to just before the edge
            glEnd();
        }

        // Edge condition label
        glColor3f(0.0f, 0.0f, 0.0f);
        glRasterPos3f(0.0f, labelHeight + 5.0f, edgePos - arrowOffset);
        string edgeLabel = edgeConditionNames[currentEdge] + " Active";
        for (char c : edgeLabel) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        }
    }

    // ============== Corner Condition Visualization ==============
    if (currentCorner != NO_SPECIAL_CORNER) {
        // Southwest corner position
        float cornerX = -25.0f;
        float cornerZ = -15.0f;

        // Corner color based on condition type
        float r = 0.0f, g = 0.0f, b = 0.0f;
        switch (currentCorner) {
        case PLAZA_CORNER:
            r = 1.0f; g = 0.6f; b = 0.0f; // Orange
            break;
        case LANDMARK_CORNER:
            r = 0.8f; g = 0.0f; b = 0.8f; // Purple
            break;
        case TERRACED_CORNER:
            r = 0.0f; g = 0.8f; b = 0.8f; // Cyan
            break;
        }

        // Draw corner highlight
        glColor3f(r, g, b);

        // Corner marker - draw circle or cube around corner point
        glPushMatrix();
        glTranslatef(cornerX, arrowHeight, cornerZ);

        // Rotating wireframe cube for corner marker
        static float rotAngle = 0.0f;
        rotAngle += 0.5f; // Increment for animation effect
        if (rotAngle > 360.0f) rotAngle -= 360.0f;

        glRotatef(rotAngle, 0.0f, 1.0f, 0.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(3.0f);

        // Draw wireframe cube
        glBegin(GL_QUADS);
        // Front face
        glVertex3f(-cornerMarkerSize / 2, -cornerMarkerSize / 2, cornerMarkerSize / 2);
        glVertex3f(cornerMarkerSize / 2, -cornerMarkerSize / 2, cornerMarkerSize / 2);
        glVertex3f(cornerMarkerSize / 2, cornerMarkerSize / 2, cornerMarkerSize / 2);
        glVertex3f(-cornerMarkerSize / 2, cornerMarkerSize / 2, cornerMarkerSize / 2);

        // Back face
        glVertex3f(-cornerMarkerSize / 2, -cornerMarkerSize / 2, -cornerMarkerSize / 2);
        glVertex3f(-cornerMarkerSize / 2, cornerMarkerSize / 2, -cornerMarkerSize / 2);
        glVertex3f(cornerMarkerSize / 2, cornerMarkerSize / 2, -cornerMarkerSize / 2);
        glVertex3f(cornerMarkerSize / 2, -cornerMarkerSize / 2, -cornerMarkerSize / 2);

        // Top face
        glVertex3f(-cornerMarkerSize / 2, cornerMarkerSize / 2, -cornerMarkerSize / 2);
        glVertex3f(-cornerMarkerSize / 2, cornerMarkerSize / 2, cornerMarkerSize / 2);
        glVertex3f(cornerMarkerSize / 2, cornerMarkerSize / 2, cornerMarkerSize / 2);
        glVertex3f(cornerMarkerSize / 2, cornerMarkerSize / 2, -cornerMarkerSize / 2);

        // Bottom face
        glVertex3f(-cornerMarkerSize / 2, -cornerMarkerSize / 2, -cornerMarkerSize / 2);
        glVertex3f(cornerMarkerSize / 2, -cornerMarkerSize / 2, -cornerMarkerSize / 2);
        glVertex3f(cornerMarkerSize / 2, -cornerMarkerSize / 2, cornerMarkerSize / 2);
        glVertex3f(-cornerMarkerSize / 2, -cornerMarkerSize / 2, cornerMarkerSize / 2);

        // Left face
        glVertex3f(-cornerMarkerSize / 2, -cornerMarkerSize / 2, -cornerMarkerSize / 2);
        glVertex3f(-cornerMarkerSize / 2, -cornerMarkerSize / 2, cornerMarkerSize / 2);
        glVertex3f(-cornerMarkerSize / 2, cornerMarkerSize / 2, cornerMarkerSize / 2);
        glVertex3f(-cornerMarkerSize / 2, cornerMarkerSize / 2, -cornerMarkerSize / 2);

        // Right face
        glVertex3f(cornerMarkerSize / 2, -cornerMarkerSize / 2, -cornerMarkerSize / 2);
        glVertex3f(cornerMarkerSize / 2, cornerMarkerSize / 2, -cornerMarkerSize / 2);
        glVertex3f(cornerMarkerSize / 2, cornerMarkerSize / 2, cornerMarkerSize / 2);
        glVertex3f(cornerMarkerSize / 2, -cornerMarkerSize / 2, cornerMarkerSize / 2);
        glEnd();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glPopMatrix();

        // Corner arrows pointing to corner from 45 degree angle
        float arrowDist = 25.0f;
        glLineWidth(2.0f);

        glBegin(GL_TRIANGLES);
        // Arrow head
        glVertex3f(cornerX + arrowDist * 0.7071f, labelHeight, cornerZ + arrowDist * 0.7071f);
        glVertex3f(cornerX + (arrowDist + arrowSize) * 0.7071f, labelHeight, cornerZ + arrowDist * 0.7071f);
        glVertex3f(cornerX + arrowDist * 0.7071f, labelHeight, cornerZ + (arrowDist + arrowSize) * 0.7071f);
        glEnd();

        // Arrow line
        glBegin(GL_LINES);
        glVertex3f(cornerX + arrowDist * 0.7071f, labelHeight, cornerZ + arrowDist * 0.7071f);
        glVertex3f(cornerX + 2.0f, labelHeight, cornerZ + 2.0f);
        glEnd();

        // Corner condition label
        glColor3f(0.0f, 0.0f, 0.0f);
        glRasterPos3f(cornerX + arrowDist * 0.7071f, labelHeight + 5.0f, cornerZ + arrowDist * 0.7071f);
        string cornerLabel = cornerConditionNames[currentCorner] + " Active";
        for (char c : cornerLabel) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        }
    }
}
#endif // _MAIN_