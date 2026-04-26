#ifndef BUILDING_3D_GENERATOR_H
#define BUILDING_3D_GENERATOR_H

// Include dependencies
#include "SiteGeneration.h"
#include "ParcelAnalyzer.h"
#include "RoadAnalyzer.h"
#include "ParcelSubdivider.h"
#include "BuildingFootprintGenerator.h"

// Standard Library Includes
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <map>
#include <functional>
#include <cmath>
#include <random>

// OpenGL includes
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

using namespace std;

// ===================== 3D BUILDING DATA STRUCTURES =====================

// 3D Point structure
struct Point3D {
    float x, y, z;

    Point3D() : x(0), y(0), z(0) {}
    Point3D(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Point3D(const Point2D& pt2d, float z_) : x(pt2d.x), y(pt2d.y), z(z_) {}

    Point3D operator+(const Point3D& other) const {
        return Point3D(x + other.x, y + other.y, z + other.z);
    }

    Point3D operator-(const Point3D& other) const {
        return Point3D(x - other.x, y - other.y, z - other.z);
    }

    Point3D operator*(float scalar) const {
        return Point3D(x * scalar, y * scalar, z * scalar);
    }

    float distance(const Point3D& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return sqrt(dx * dx + dy * dy + dz * dz);
    }

    Point2D toPoint2D() const {
        return Point2D(x, y);
    }
};

// Building program types (using similar structure to EdgeCorner code)
enum class VoxelType {
    VISIT,      // Retail/Commercial/Public spaces
    WORK,       // Office/Business spaces
    LIVE,       // Residential spaces
    TRANSITION, // Mechanical/Service/Circulation
    CIRCULATION,
    GREEN,
    NONE
};

// Transition program types for special spaces
enum class TransitionProgram {
    LOBBY,
    MECHANICAL,
    CIRCULATION_CORE,
    SERVICE_FLOOR,
    AMENITY_FLOOR,
    SKY_GARDEN,
    ROOFTOP_TERRACE,
    PARKING_DECK,
    RETAIL_MEZZANINE
};

// 3D Voxel structure - UPDATED with orientation support
struct Voxel3D {
    int id;
    VoxelType type;
    TransitionProgram program; // Only used if type == TRANSITION
    Point3D position;          // X,Y for footprint position, Z for height above ground
    Point3D dimensions;        // X=width, Y=depth, Z=height
    Point2D orientation;       // Orientation vector (from footprint's primaryOrientation)
    float rotationAngle;       // Rotation angle in radians (calculated from orientation)
    int level;
    int parentFootprintId;
    vector<string> features;
    float structuralSupport = 1.0f;

    Voxel3D() : id(-1), type(VoxelType::NONE), program(TransitionProgram::LOBBY),
        orientation(1.0f, 0.0f), rotationAngle(0.0f), level(0), parentFootprintId(-1) {
    }

    // Helper functions
    Point3D getCenter() const {
        Point3D center;
        center.x = position.x + dimensions.x / 2;
        center.y = position.y + dimensions.y / 2;
        center.z = position.z + dimensions.z / 2;
        return center;
    }

    float getVolume() const {
        return dimensions.x * dimensions.y * dimensions.z;
    }

    float getFootprintArea() const {
        return dimensions.x * dimensions.y;
    }

    // Calculate rotation angle from orientation vector
    void calculateRotationAngle() {
        rotationAngle = atan2(orientation.y, orientation.x);
    }

    // Get world-space bounding box (for non-rotated bounds)
    pair<Point3D, Point3D> getBoundingBox() const {
        Point3D min_pt = position;
        Point3D max_pt;
        max_pt.x = position.x + dimensions.x;
        max_pt.y = position.y + dimensions.y;
        max_pt.z = position.z + dimensions.z;
        return make_pair(min_pt, max_pt);
    }
};

// Program requirements for different voxel types
struct ProgramRequirements {
    VoxelType type;
    float minFloorHeight;     // Minimum floor-to-floor height
    float maxFloorHeight;     // Maximum floor-to-floor height
    Point3D preferredDimensions; // Preferred voxel dimensions (width, depth, height)
    bool allowsStacking;      // Can this program be stacked vertically
    int maxConsecutiveLevels; // Maximum levels of this program in sequence
    float structuralLoad;     // Structural load factor

    ProgramRequirements(VoxelType t = VoxelType::NONE) : type(t), minFloorHeight(0.3f),
        maxFloorHeight(0.5f), preferredDimensions(0.8f, 0.8f, 0.4f),
        allowsStacking(true), maxConsecutiveLevels(10), structuralLoad(1.0f) {
    }
};

// Height and density limits based on parcel categories
struct DensityLimits {
    ParcelCategory category;
    float maxBuildingHeight;  // Maximum total building height
    float maxFloorAreaRatio;  // Maximum FAR (floor area / lot area)
    int maxFloors;            // Maximum number of floors
    float minOpenSpaceRatio;  // Minimum open space ratio
    bool allowsMixedUse;      // Allows mixed-use development

    DensityLimits(ParcelCategory cat = SMALL_PARCEL) : category(cat),
        maxBuildingHeight(2.0f), maxFloorAreaRatio(2.0f), maxFloors(6),
        minOpenSpaceRatio(0.2f), allowsMixedUse(true) {
    }
};

// Building composition strategy
enum class BuildingStrategy {
    PODIUM_TOWER,      // Retail podium with office/residential tower
    UNIFORM_MIXED,     // Mixed programs throughout
    HORIZONTAL_ZONES,  // Programs separated horizontally
    VERTICAL_ZONES,    // Programs separated vertically
    COURTYARD_BLOCK,   // Building around courtyard
    STEPPED_MASSING    // Stepped building form
};

// ===================== BUILDING 3D GENERATOR CLASS =====================

class Building3DGenerator {
private:
    // Dependencies
    BuildingFootprintGenerator* footprintGenerator;
    ParcelSubdivider* parcelSubdivider;
    ParcelAnalyzer* parcelAnalyzer;
    RoadAnalyzer* roadAnalyzer;

    // Generated 3D voxels
    vector<Voxel3D> building3DVoxels;
    int nextVoxelId;

    // Program requirements
    map<VoxelType, ProgramRequirements> programRequirements;

    // Density limits by parcel category
    map<ParcelCategory, DensityLimits> densityLimits;

    // Generation parameters
    float defaultFloorHeight;
    float maxBuildingHeight;
    bool respectHeightLimits;
    bool enableHeightGradient;
    float gradientIntensity;

    // Building scaling parameters to keep buildings within footprints
    float buildingScaleFactor;    // Scale factor for building dimensions relative to footprint
    float buildingSetback;        // Additional setback from footprint edge
    bool centerBuildingsInFootprint; // Whether to center buildings in footprint

    // NEW: Orientation parameters
    bool enableVoxelOrientation;  // Whether to orient voxels to footprint direction
    bool showOrientationDebug;    // Whether to show orientation debug info

    // Random number generator for variations
    std::random_device rd;
    std::mt19937 rng;

    // Verbosity flag
    bool verboseLogging;

public:
    // ================== CONSTRUCTOR & DESTRUCTOR ==================

    Building3DGenerator(BuildingFootprintGenerator* footprintGen,
        ParcelSubdivider* subdivider,
        ParcelAnalyzer* parcelAnalyzer,
        RoadAnalyzer* roadAnalyzer)
        : footprintGenerator(footprintGen), parcelSubdivider(subdivider),
        parcelAnalyzer(parcelAnalyzer), roadAnalyzer(roadAnalyzer),
        nextVoxelId(0), defaultFloorHeight(0.4f), maxBuildingHeight(20.0f),
        respectHeightLimits(true), enableHeightGradient(true), gradientIntensity(20.0f),
        buildingScaleFactor(0.6f), buildingSetback(0.3f), centerBuildingsInFootprint(true),
        enableVoxelOrientation(true), showOrientationDebug(false),
        verboseLogging(false), rng(rd()) {

        cout << "=== Building3DGenerator Initialized (With Orientation Support) ===" << endl;
        initializeProgramRequirements();
        initializeDensityLimits();
    }

    ~Building3DGenerator() {
        cout << "=== Building3DGenerator Destroyed ===" << endl;
    }

    // ================== INITIALIZATION METHODS ==================

    void initializeProgramRequirements() {
        // VISIT program (Retail/Commercial/Public) - Ground level focused
        programRequirements[VoxelType::VISIT] = ProgramRequirements();
        programRequirements[VoxelType::VISIT].minFloorHeight = 0.4f; // 4m scaled
        programRequirements[VoxelType::VISIT].maxFloorHeight = 0.6f; // 6m scaled
        programRequirements[VoxelType::VISIT].preferredDimensions = Point3D(1.2f, 1.2f, 0.5f); // 12x12x5m
        programRequirements[VoxelType::VISIT].allowsStacking = true;
        programRequirements[VoxelType::VISIT].maxConsecutiveLevels = 3;
        programRequirements[VoxelType::VISIT].structuralLoad = 1.2f;

        // WORK program (Office spaces) - Middle levels
        programRequirements[VoxelType::WORK] = ProgramRequirements();
        programRequirements[VoxelType::WORK].minFloorHeight = 0.35f; // 3.5m scaled
        programRequirements[VoxelType::WORK].maxFloorHeight = 0.45f; // 4.5m scaled
        programRequirements[VoxelType::WORK].preferredDimensions = Point3D(0.8f, 0.8f, 0.4f); // 8x8x4m
        programRequirements[VoxelType::WORK].allowsStacking = true;
        programRequirements[VoxelType::WORK].maxConsecutiveLevels = 30;
        programRequirements[VoxelType::WORK].structuralLoad = 1.0f;

        // LIVE program (Residential) - Upper levels  
        programRequirements[VoxelType::LIVE] = ProgramRequirements();
        programRequirements[VoxelType::LIVE].minFloorHeight = 0.3f; // 3m scaled
        programRequirements[VoxelType::LIVE].maxFloorHeight = 0.4f; // 4m scaled
        programRequirements[VoxelType::LIVE].preferredDimensions = Point3D(0.4f, 0.4f, 0.35f); // 4x4x3.5m
        programRequirements[VoxelType::LIVE].allowsStacking = true;
        programRequirements[VoxelType::LIVE].maxConsecutiveLevels = 40;
        programRequirements[VoxelType::LIVE].structuralLoad = 0.8f;

        // TRANSITION program (Services/Circulation)
        programRequirements[VoxelType::TRANSITION] = ProgramRequirements();
        programRequirements[VoxelType::TRANSITION].minFloorHeight = 0.25f; // 2.5m scaled
        programRequirements[VoxelType::TRANSITION].maxFloorHeight = 0.8f;  // 8m scaled (for atriums)
        programRequirements[VoxelType::TRANSITION].preferredDimensions = Point3D(0.6f, 0.6f, 0.3f);
        programRequirements[VoxelType::TRANSITION].allowsStacking = true;
        programRequirements[VoxelType::TRANSITION].maxConsecutiveLevels = 1;
        programRequirements[VoxelType::TRANSITION].structuralLoad = 0.6f;
    }

    void initializeDensityLimits() {
        // Large parcels - Low density, taller buildings allowed
        densityLimits[LARGE_PARCEL] = DensityLimits(LARGE_PARCEL);
        densityLimits[LARGE_PARCEL].maxBuildingHeight = 100.0f; // 120m scaled
        densityLimits[LARGE_PARCEL].maxFloorAreaRatio = 20.0f;
        densityLimits[LARGE_PARCEL].maxFloors = 120;
        densityLimits[LARGE_PARCEL].minOpenSpaceRatio = 0.2f;
        densityLimits[LARGE_PARCEL].allowsMixedUse = true;

        // Medium parcels - Medium density
        densityLimits[MEDIUM_PARCEL] = DensityLimits(MEDIUM_PARCEL);
        densityLimits[MEDIUM_PARCEL].maxBuildingHeight = 40.0f; // 80m scaled
        densityLimits[MEDIUM_PARCEL].maxFloorAreaRatio = 4.0f;
        densityLimits[MEDIUM_PARCEL].maxFloors = 80;
        densityLimits[MEDIUM_PARCEL].minOpenSpaceRatio = 0.25f;
        densityLimits[MEDIUM_PARCEL].allowsMixedUse = true;

        // Small parcels - Higher density, lower buildings
        densityLimits[SMALL_PARCEL] = DensityLimits(SMALL_PARCEL);
        densityLimits[SMALL_PARCEL].maxBuildingHeight = 15.0f; // 50m scaled
        densityLimits[SMALL_PARCEL].maxFloorAreaRatio = 2.5f;
        densityLimits[SMALL_PARCEL].maxFloors = 20;
        densityLimits[SMALL_PARCEL].minOpenSpaceRatio = 0.1f;
        densityLimits[SMALL_PARCEL].allowsMixedUse = true;
    }

    // ================== FOOTPRINT ANALYSIS METHODS ==================

    // Calculate the scaled building dimensions that fit within the footprint
    Point3D calculateScaledBuildingDimensions(const BuildingFootprint& footprint) {
        if (footprint.boundary.size() < 3) {
            return Point3D(2.0f, 2.0f, 0.4f); // Fallback dimensions
        }

        // Calculate actual footprint bounding box
        float minX = footprint.boundary[0].x, maxX = footprint.boundary[0].x;
        float minY = footprint.boundary[0].y, maxY = footprint.boundary[0].y;

        for (const auto& pt : footprint.boundary) {
            minX = min(minX, pt.x);
            maxX = max(maxX, pt.x);
            minY = min(minY, pt.y);
            maxY = max(maxY, pt.y);
        }

        float footprintWidth = maxX - minX;
        float footprintDepth = maxY - minY;

        // Apply scaling factor and setback to ensure building fits within footprint
        float buildingWidth = (footprintWidth - 2 * buildingSetback) * buildingScaleFactor;
        float buildingDepth = (footprintDepth - 2 * buildingSetback) * buildingScaleFactor;

        // Ensure minimum building size
        buildingWidth = max(buildingWidth, 1.0f);  // Minimum 10m scaled
        buildingDepth = max(buildingDepth, 1.0f);  // Minimum 10m scaled

        // Don't exceed footprint dimensions
        buildingWidth = min(buildingWidth, footprintWidth * 0.95f);  // 95% max
        buildingDepth = min(buildingDepth, footprintDepth * 0.95f);  // 95% max

        if (verboseLogging) {
            cout << "Footprint " << footprint.id << ": footprint(" << footprintWidth << "x" << footprintDepth
                << ") -> building(" << buildingWidth << "x" << buildingDepth << ")" << endl;
        }

        return Point3D(buildingWidth, buildingDepth, defaultFloorHeight);
    }

    // Calculate the centered position within the footprint
    Point3D calculateCenteredBuildingPosition(const BuildingFootprint& footprint, const Point3D& buildingDimensions, float height = 0.0f) {
        if (footprint.boundary.size() < 3) {
            return Point3D(footprint.center.x - buildingDimensions.x / 2,
                footprint.center.y - buildingDimensions.y / 2, height);
        }

        if (centerBuildingsInFootprint) {
            // Center the building within the footprint
            return Point3D(footprint.center.x - buildingDimensions.x / 2,
                footprint.center.y - buildingDimensions.y / 2, height);
        }
        else {
            // Calculate footprint bounding box and position building at bottom-left + offset
            float minX = footprint.boundary[0].x, minY = footprint.boundary[0].y;
            for (const auto& pt : footprint.boundary) {
                minX = min(minX, pt.x);
                minY = min(minY, pt.y);
            }

            return Point3D(minX + buildingSetback, minY + buildingSetback, height);
        }
    }

    // ================== NEW: ORIENTATION METHODS ==================

    // Apply footprint orientation to a voxel
    void applyFootprintOrientation(Voxel3D& voxel, const BuildingFootprint& footprint) {
        if (enableVoxelOrientation) {
            voxel.orientation = footprint.primaryOrientation;
            voxel.calculateRotationAngle();

            if (verboseLogging) {
                cout << "Applied orientation to voxel " << voxel.id
                    << ": direction(" << voxel.orientation.x << ", " << voxel.orientation.y
                    << "), angle=" << (voxel.rotationAngle * 180.0f / M_PI) << "°" << endl;
            }
        }
    }

    // ================== MAIN GENERATION METHODS ==================

    // Generate 3D buildings for all footprints
    void generateAll3DBuildings() {
        cout << "\n=== GENERATING ORIENTED 3D BUILDINGS ===" << endl;

        if (!validateDependencies()) {
            cout << "Error: Required dependencies not available" << endl;
            return;
        }

        building3DVoxels.clear();
        nextVoxelId = 0;

        // Get all footprints
        const auto& footprints = footprintGenerator->getAllFootprints();

        for (const auto& footprint : footprints) {
            vector<Voxel3D> buildingVoxels = generate3DBuildingFromFootprint(footprint);

            // Add to main collection with unique IDs
            for (auto& voxel : buildingVoxels) {
                voxel.id = nextVoxelId++;
                building3DVoxels.push_back(voxel);
            }

            if (verboseLogging) {
                cout << "Generated oriented 3D building with " << buildingVoxels.size()
                    << " voxels for footprint " << footprint.id << endl;
            }
        }

        cout << "Generated " << building3DVoxels.size() << " oriented 3D voxels for "
            << footprints.size() << " buildings (aligned to footprint orientations)" << endl;
        print3DStatistics();
    }

    // Generate a 3D building from a single footprint
    vector<Voxel3D> generate3DBuildingFromFootprint(const BuildingFootprint& footprint) {
        vector<Voxel3D> buildingVoxels;

        // Get the plot information
        const Plot* parentPlot = getPlotFromFootprint(footprint);
        if (!parentPlot) {
            cout << "Warning: Could not find parent plot for footprint " << footprint.id << endl;
            return buildingVoxels;
        }

        // Determine building strategy based on plot characteristics
        BuildingStrategy strategy = determineBuildingStrategy(*parentPlot, footprint);

        // Generate building based on strategy
        switch (strategy) {
        case BuildingStrategy::PODIUM_TOWER:
            buildingVoxels = generatePodiumTowerBuilding(footprint, *parentPlot);
            break;

        case BuildingStrategy::UNIFORM_MIXED:
            buildingVoxels = generateUniformMixedBuilding(footprint, *parentPlot);
            break;

        case BuildingStrategy::HORIZONTAL_ZONES:
            buildingVoxels = generateHorizontalZoneBuilding(footprint, *parentPlot);
            break;

        case BuildingStrategy::VERTICAL_ZONES:
            buildingVoxels = generateVerticalZoneBuilding(footprint, *parentPlot);
            break;

        case BuildingStrategy::COURTYARD_BLOCK:
            buildingVoxels = generateCourtyardBuilding(footprint, *parentPlot);
            break;

        case BuildingStrategy::STEPPED_MASSING:
            buildingVoxels = generateSteppedBuilding(footprint, *parentPlot);
            break;

        default:
            buildingVoxels = generatePodiumTowerBuilding(footprint, *parentPlot);
            break;
        }

        // NEW: Apply orientation to all voxels after generation
        for (auto& voxel : buildingVoxels) {
            applyFootprintOrientation(voxel, footprint);
        }

        // Apply height limits
        applyHeightLimits(buildingVoxels, parentPlot->originalCategory);

        return buildingVoxels;
    }

    // ================== SPECIFIC GENERATION METHODS (UPDATED FOR ORIENTATION) ==================

    // Generate podium (retail/commercial base) with orientation
    Voxel3D generatePodium(const BuildingFootprint& footprint, VoxelType visitProgram) {
        Voxel3D podium;
        podium.type = visitProgram;
        podium.program = TransitionProgram::RETAIL_MEZZANINE;
        podium.parentFootprintId = footprint.id;

        // Use scaled dimensions that fit within footprint
        Point3D buildingDimensions = calculateScaledBuildingDimensions(footprint);
        buildingDimensions.z = programRequirements[VoxelType::VISIT].maxFloorHeight;

        // Position building centered within footprint at ground level (Z=0)
        podium.position = calculateCenteredBuildingPosition(footprint, buildingDimensions, 0.0f);
        podium.dimensions = buildingDimensions;

        podium.level = 0;
        podium.features = { "retail_spaces", "public_entrance", "display_windows" };

        // Apply orientation
        applyFootprintOrientation(podium, footprint);

        if (verboseLogging) {
            cout << "Generated oriented podium for footprint " << footprint.id
                << ": pos(" << podium.position.x << "," << podium.position.y << "," << podium.position.z
                << ") dim(" << podium.dimensions.x << "x" << podium.dimensions.y << "x" << podium.dimensions.z
                << ") angle=" << (podium.rotationAngle * 180.0f / M_PI) << "°" << endl;
        }

        return podium;
    }

    // Stack programs above podium with orientation
    vector<Voxel3D> stackAbovePodium(const BuildingFootprint& footprint,
        VoxelType workProgram, VoxelType liveProgram) {
        vector<Voxel3D> stackedVoxels;

        // Generate base podium (properly scaled and oriented)
        Voxel3D podium = generatePodium(footprint, VoxelType::VISIT);
        stackedVoxels.push_back(podium);

        // Get plot for height limits
        const Plot* parentPlot = getPlotFromFootprint(footprint);
        if (!parentPlot) return stackedVoxels;

        const DensityLimits& limits = densityLimits[parentPlot->originalCategory];

        float currentHeight = podium.dimensions.z; // Z is height
        int currentLevel = 1;

        // Stack work program (office) floors - use same footprint as podium
        int workFloors = calculateWorkFloors(*parentPlot, footprint);
        for (int i = 0; i < workFloors && currentHeight < limits.maxBuildingHeight; i++) {
            Voxel3D workVoxel = createScaledVoxelAbove(podium, workProgram, currentLevel, currentHeight);
            applyFootprintOrientation(workVoxel, footprint); // Apply orientation
            stackedVoxels.push_back(workVoxel);

            currentHeight += workVoxel.dimensions.z; // Z is height
            currentLevel++;
        }

        // Add transition floor between work and live
        if (currentHeight < limits.maxBuildingHeight - 1.0f) {
            Voxel3D transition = createScaledVoxelAbove(podium, VoxelType::TRANSITION, currentLevel, currentHeight);
            transition.program = TransitionProgram::AMENITY_FLOOR;
            transition.features = { "sky_lounge", "fitness_center", "communal_terrace" };
            applyFootprintOrientation(transition, footprint); // Apply orientation
            stackedVoxels.push_back(transition);

            currentHeight += transition.dimensions.z; // Z is height
            currentLevel++;
        }

        // Stack residential program (live) floors - potentially smaller than base
        int liveFloors = calculateLiveFloors(*parentPlot, footprint, limits.maxBuildingHeight - currentHeight);
        for (int i = 0; i < liveFloors && currentHeight < limits.maxBuildingHeight; i++) {
            Voxel3D liveVoxel = createScaledVoxelAbove(podium, liveProgram, currentLevel, currentHeight);
            // Make residential floors slightly smaller for stepped effect
            liveVoxel.dimensions.x *= 0.95f;
            liveVoxel.dimensions.y *= 0.95f;
            // Re-center the smaller voxel
            liveVoxel.position.x = podium.position.x + (podium.dimensions.x - liveVoxel.dimensions.x) / 2;
            liveVoxel.position.y = podium.position.y + (podium.dimensions.y - liveVoxel.dimensions.y) / 2;

            applyFootprintOrientation(liveVoxel, footprint); // Apply orientation
            stackedVoxels.push_back(liveVoxel);

            currentHeight += liveVoxel.dimensions.z; // Z is height
            currentLevel++;
        }

        // Add rooftop amenity if space allows
        if (currentHeight < limits.maxBuildingHeight - 0.5f) {
            Voxel3D rooftop = createScaledVoxelAbove(podium, VoxelType::TRANSITION, currentLevel, currentHeight);
            rooftop.program = TransitionProgram::ROOFTOP_TERRACE;
            rooftop.features = { "rooftop_garden", "city_views", "communal_space" };
            // Make rooftop smaller
            rooftop.dimensions.x *= 0.8f;
            rooftop.dimensions.y *= 0.8f;
            // Re-center
            rooftop.position.x = podium.position.x + (podium.dimensions.x - rooftop.dimensions.x) / 2;
            rooftop.position.y = podium.position.y + (podium.dimensions.y - rooftop.dimensions.y) / 2;

            applyFootprintOrientation(rooftop, footprint); // Apply orientation
            stackedVoxels.push_back(rooftop);
        }

        return stackedVoxels;
    }

    // Apply height limits based on plot category
    void applyHeightLimits(vector<Voxel3D>& voxelStack, ParcelCategory plotCategory) {
        if (!respectHeightLimits || voxelStack.empty()) return;

        const DensityLimits& limits = densityLimits[plotCategory];

        // Calculate current building height
        float maxHeight = 0;
        for (const auto& voxel : voxelStack) {
            float voxelTop = voxel.position.z + voxel.dimensions.z; // Z is height
            maxHeight = max(maxHeight, voxelTop);
        }

        if (maxHeight <= limits.maxBuildingHeight) {
            return; // Within limits, no adjustment needed
        }

        // Remove voxels that exceed height limit
        vector<Voxel3D> adjustedStack;
        for (const auto& voxel : voxelStack) {
            if (voxel.position.z < limits.maxBuildingHeight) {
                Voxel3D adjustedVoxel = voxel;

                // Trim voxel if it extends beyond limit
                float voxelTop = voxel.position.z + voxel.dimensions.z;
                if (voxelTop > limits.maxBuildingHeight) {
                    adjustedVoxel.dimensions.z = limits.maxBuildingHeight - voxel.position.z;

                    // Don't add voxels that become too small
                    if (adjustedVoxel.dimensions.z > 0.1f) {
                        adjustedStack.push_back(adjustedVoxel);
                    }
                }
                else {
                    adjustedStack.push_back(adjustedVoxel);
                }
            }
        }

        voxelStack = adjustedStack;

        if (verboseLogging) {
            cout << "Applied height limits for " << getParcelCategoryName(plotCategory)
                << " (limit: " << limits.maxBuildingHeight << "m scaled)" << endl;
        }
    }

    // ================== BUILDING STRATEGY IMPLEMENTATIONS (UPDATED FOR ORIENTATION) ==================

    BuildingStrategy determineBuildingStrategy(const Plot& plot, const BuildingFootprint& footprint) {
        // Determine strategy based on plot characteristics

        // Large plots with good access -> Podium Tower
        if (plot.originalCategory == LARGE_PARCEL && footprint.hasMajorRoadFrontage) {
            return BuildingStrategy::PODIUM_TOWER;
        }

        // Plots with mixed edge conditions -> Uniform Mixed
        if (footprint.primaryEdgeType == SiteEdgeType::MIXED_EDGE) {
            return BuildingStrategy::UNIFORM_MIXED;
        }

        // Plots with water/park frontage -> Stepped massing for views
        if (footprint.hasWaterFrontage || footprint.hasParkFrontage) {
            return BuildingStrategy::STEPPED_MASSING;
        }

        // Corner plots -> Courtyard or horizontal zones
        if (footprint.cornerType != SiteCornerType::NO_CORNER) {
            return plot.area > 80.0f ? BuildingStrategy::COURTYARD_BLOCK : BuildingStrategy::HORIZONTAL_ZONES;
        }

        // Small plots -> Vertical zones (simple stacking)
        if (plot.originalCategory == SMALL_PARCEL) {
            return BuildingStrategy::VERTICAL_ZONES;
        }

        // Default to podium tower
        return BuildingStrategy::PODIUM_TOWER;
    }

    vector<Voxel3D> generatePodiumTowerBuilding(const BuildingFootprint& footprint, const Plot& plot) {
        return stackAbovePodium(footprint, VoxelType::WORK, VoxelType::LIVE);
    }

    vector<Voxel3D> generateUniformMixedBuilding(const BuildingFootprint& footprint, const Plot& plot) {
        vector<Voxel3D> voxels;

        if (footprint.boundary.size() < 3) {
            return voxels; // Can't generate building without valid footprint
        }

        // Calculate scaled building dimensions that fit within footprint
        Point3D buildingDimensions = calculateScaledBuildingDimensions(footprint);

        int totalFloors = min(densityLimits[plot.originalCategory].maxFloors,
            (int)(densityLimits[plot.originalCategory].maxBuildingHeight / 0.4f));

        float currentHeight = 0;
        for (int floor = 0; floor < totalFloors; floor++) {
            VoxelType floorType;

            // Mix programs throughout
            if (floor == 0) {
                floorType = VoxelType::VISIT; // Ground floor always commercial
            }
            else if (floor % 3 == 1) {
                floorType = VoxelType::WORK;  // Office floors
            }
            else if (floor % 3 == 2) {
                floorType = VoxelType::LIVE;  // Residential floors
            }
            else {
                floorType = VoxelType::TRANSITION; // Service/amenity floors
            }

            Voxel3D floorVoxel;
            floorVoxel.type = floorType;
            floorVoxel.parentFootprintId = footprint.id;

            // Use scaled dimensions and centered position
            Point3D floorDimensions = buildingDimensions;
            floorDimensions.z = programRequirements[floorType].minFloorHeight;

            floorVoxel.position = calculateCenteredBuildingPosition(footprint, floorDimensions, currentHeight);
            floorVoxel.dimensions = floorDimensions;
            floorVoxel.level = floor;

            // Apply orientation
            applyFootprintOrientation(floorVoxel, footprint);

            voxels.push_back(floorVoxel);
            currentHeight += floorVoxel.dimensions.z; // Z is height
        }

        return voxels;
    }

    vector<Voxel3D> generateHorizontalZoneBuilding(const BuildingFootprint& footprint, const Plot& plot) {
        vector<Voxel3D> voxels;

        // Calculate scaled building dimensions
        Point3D buildingDimensions = calculateScaledBuildingDimensions(footprint);

        // Divide footprint into zones horizontally
        float zoneWidth = buildingDimensions.x / 3.0f; // 3 horizontal zones

        // Create different program zones
        VoxelType zones[3] = { VoxelType::VISIT, VoxelType::WORK, VoxelType::LIVE };

        Point3D baseBuildingPosition = calculateCenteredBuildingPosition(footprint, buildingDimensions, 0.0f);

        for (int zone = 0; zone < 3; zone++) {
            int zoneFloors = calculateFloorsForZone(plot, zones[zone]);
            float currentHeight = 0;

            for (int floor = 0; floor < zoneFloors; floor++) {
                Voxel3D zoneVoxel;
                zoneVoxel.type = zones[zone];
                zoneVoxel.parentFootprintId = footprint.id;

                // Position each zone side by side (X direction)
                zoneVoxel.position = Point3D(
                    baseBuildingPosition.x + zone * zoneWidth,  // X position for this zone
                    baseBuildingPosition.y,                     // Y position (full depth)
                    currentHeight                               // Z position (height)
                );
                zoneVoxel.dimensions = Point3D(
                    zoneWidth,                                  // Width of this zone
                    buildingDimensions.y,                       // Full depth
                    programRequirements[zones[zone]].minFloorHeight // Height
                );
                zoneVoxel.level = floor;

                // Apply orientation
                applyFootprintOrientation(zoneVoxel, footprint);

                voxels.push_back(zoneVoxel);
                currentHeight += zoneVoxel.dimensions.z;
            }
        }

        return voxels;
    }

    vector<Voxel3D> generateVerticalZoneBuilding(const BuildingFootprint& footprint, const Plot& plot) {
        // Simple vertical stacking - this is what stackAbovePodium does
        return stackAbovePodium(footprint, VoxelType::WORK, VoxelType::LIVE);
    }

    vector<Voxel3D> generateCourtyardBuilding(const BuildingFootprint& footprint, const Plot& plot) {
        vector<Voxel3D> voxels;

        // Calculate scaled building dimensions
        Point3D buildingDimensions = calculateScaledBuildingDimensions(footprint);
        Point3D baseBuildingPosition = calculateCenteredBuildingPosition(footprint, buildingDimensions, 0.0f);

        // Create building around perimeter with central courtyard
        float ringWidth = min(buildingDimensions.x, buildingDimensions.y) * 0.3f; // 30% of smaller dimension for building width

        int floors = min(8, densityLimits[plot.originalCategory].maxFloors);
        float currentHeight = 0;

        for (int floor = 0; floor < floors; floor++) {
            VoxelType floorType = (floor == 0) ? VoxelType::VISIT :
                (floor < floors / 2) ? VoxelType::WORK : VoxelType::LIVE;

            float floorHeight = programRequirements[floorType].minFloorHeight;

            // Create 4 sides of the courtyard building
            // North side
            Voxel3D northSide;
            northSide.type = floorType;
            northSide.parentFootprintId = footprint.id;
            northSide.position = Point3D(baseBuildingPosition.x, baseBuildingPosition.y + buildingDimensions.y - ringWidth, currentHeight);
            northSide.dimensions = Point3D(buildingDimensions.x, ringWidth, floorHeight);
            northSide.level = floor;
            northSide.features.push_back("courtyard_facing");
            applyFootprintOrientation(northSide, footprint);
            voxels.push_back(northSide);

            // South side
            Voxel3D southSide;
            southSide.type = floorType;
            southSide.parentFootprintId = footprint.id;
            southSide.position = Point3D(baseBuildingPosition.x, baseBuildingPosition.y, currentHeight);
            southSide.dimensions = Point3D(buildingDimensions.x, ringWidth, floorHeight);
            southSide.level = floor;
            southSide.features.push_back("courtyard_facing");
            applyFootprintOrientation(southSide, footprint);
            voxels.push_back(southSide);

            // East side (excluding corners to avoid overlap)
            Voxel3D eastSide;
            eastSide.type = floorType;
            eastSide.parentFootprintId = footprint.id;
            eastSide.position = Point3D(baseBuildingPosition.x + buildingDimensions.x - ringWidth, baseBuildingPosition.y + ringWidth, currentHeight);
            eastSide.dimensions = Point3D(ringWidth, buildingDimensions.y - 2 * ringWidth, floorHeight);
            eastSide.level = floor;
            eastSide.features.push_back("courtyard_facing");
            applyFootprintOrientation(eastSide, footprint);
            voxels.push_back(eastSide);

            // West side (excluding corners to avoid overlap)
            Voxel3D westSide;
            westSide.type = floorType;
            westSide.parentFootprintId = footprint.id;
            westSide.position = Point3D(baseBuildingPosition.x, baseBuildingPosition.y + ringWidth, currentHeight);
            westSide.dimensions = Point3D(ringWidth, buildingDimensions.y - 2 * ringWidth, floorHeight);
            westSide.level = floor;
            westSide.features.push_back("courtyard_facing");
            applyFootprintOrientation(westSide, footprint);
            voxels.push_back(westSide);

            currentHeight += floorHeight;
        }

        return voxels;
    }

    vector<Voxel3D> generateSteppedBuilding(const BuildingFootprint& footprint, const Plot& plot) {
        vector<Voxel3D> voxels;

        // Calculate base building dimensions
        Point3D baseBuildingDimensions = calculateScaledBuildingDimensions(footprint);
        Point3D baseBuildingPosition = calculateCenteredBuildingPosition(footprint, baseBuildingDimensions, 0.0f);

        // Create stepped building form for views
        int totalFloors = min(densityLimits[plot.originalCategory].maxFloors, 10);

        float currentHeight = 0;
        for (int floor = 0; floor < totalFloors; floor++) {
            // Each floor gets slightly smaller for stepped effect
            float stepReduction = floor * 0.05f; // 5% reduction per floor
            float floorWidth = max(baseBuildingDimensions.x * 0.3f, baseBuildingDimensions.x * (1.0f - stepReduction));
            float floorDepth = max(baseBuildingDimensions.y * 0.3f, baseBuildingDimensions.y * (1.0f - stepReduction));

            VoxelType floorType = (floor == 0) ? VoxelType::VISIT :
                (floor < totalFloors / 2) ? VoxelType::WORK : VoxelType::LIVE;

            Voxel3D stepVoxel;
            stepVoxel.type = floorType;
            stepVoxel.parentFootprintId = footprint.id;

            // Center the smaller floor within the base building footprint
            float offsetX = (baseBuildingDimensions.x - floorWidth) / 2;
            float offsetY = (baseBuildingDimensions.y - floorDepth) / 2;

            stepVoxel.position = Point3D(baseBuildingPosition.x + offsetX, baseBuildingPosition.y + offsetY, currentHeight);
            stepVoxel.dimensions = Point3D(floorWidth, floorDepth,
                programRequirements[floorType].minFloorHeight);
            stepVoxel.level = floor;
            stepVoxel.features.push_back("stepped_massing");

            // Add terrace features for upper floors
            if (floor > 2) {
                stepVoxel.features.push_back("private_terrace");
            }

            // Apply orientation
            applyFootprintOrientation(stepVoxel, footprint);

            voxels.push_back(stepVoxel);
            currentHeight += stepVoxel.dimensions.z;
        }

        return voxels;
    }

    // ================== HELPER METHODS (UPDATED FOR ORIENTATION) ==================

    const Plot* getPlotFromFootprint(const BuildingFootprint& footprint) {
        if (!parcelSubdivider) return nullptr;

        const auto& plots = parcelSubdivider->getAllPlots();
        for (const auto& plot : plots) {
            if (plot.id == footprint.parentPlotId) {
                return &plot;
            }
        }
        return nullptr;
    }

    // Create scaled voxel above base voxel (replaces createVoxelAbove)
    Voxel3D createScaledVoxelAbove(const Voxel3D& baseVoxel, VoxelType type, int level, float height) {
        Voxel3D newVoxel;
        newVoxel.type = type;
        newVoxel.parentFootprintId = baseVoxel.parentFootprintId;

        // Position: keep X and Y from base, set Z to height above ground
        newVoxel.position = Point3D(baseVoxel.position.x, baseVoxel.position.y, height);

        // Use same horizontal dimensions as base but with program-specific height
        const ProgramRequirements& req = programRequirements[type];
        newVoxel.dimensions = Point3D(
            baseVoxel.dimensions.x,  // Same width as base
            baseVoxel.dimensions.y,  // Same depth as base
            req.minFloorHeight       // Program-specific height
        );
        newVoxel.level = level;

        // Copy orientation from base voxel
        newVoxel.orientation = baseVoxel.orientation;
        newVoxel.rotationAngle = baseVoxel.rotationAngle;

        // Add program-specific features
        switch (type) {
        case VoxelType::WORK:
            newVoxel.features = { "office_spaces", "meeting_rooms", "open_plan" };
            break;
        case VoxelType::LIVE:
            newVoxel.features = { "apartments", "balconies", "residential_lobby" };
            break;
        case VoxelType::VISIT:
            newVoxel.features = { "retail_space", "public_access", "storefront" };
            break;
        default:
            newVoxel.features = { "service_space" };
            break;
        }

        return newVoxel;
    }

    int calculateWorkFloors(const Plot& plot, const BuildingFootprint& footprint) {
        const DensityLimits& limits = densityLimits[plot.originalCategory];


        // Base floors calculation with height gradient
        int baseFloors = 0;
        switch (plot.originalCategory) {
        case LARGE_PARCEL:
            baseFloors = 8; // Base 8 floors of office
            break;
        case MEDIUM_PARCEL:
            baseFloors = 5; // Base 5 floors of office
            break;
        case SMALL_PARCEL:
            baseFloors = 3; // Base 3 floors of office
            break;
        }

        // Apply height gradient multiplier
        if (enableHeightGradient) {
            float heightMultiplier = calculateHeightGradientMultiplier(plot, footprint);
            baseFloors = (int)(baseFloors * heightMultiplier);
        }

        return min(baseFloors, limits.maxFloors / 2);
    }

    int calculateLiveFloors(const Plot& plot, const BuildingFootprint& footprint, float remainingHeight) {
        int maxPossibleFloors = (int)(remainingHeight / programRequirements[VoxelType::LIVE].minFloorHeight);

        // Base floors calculation with height gradient
        int baseFloors = 0;
        switch (plot.originalCategory) {
        case LARGE_PARCEL:
            baseFloors = 12; // Base 12 floors of residential
            break;
        case MEDIUM_PARCEL:
            baseFloors = 8; // Base 8 floors of residential
            break;
        case SMALL_PARCEL:
            baseFloors = 4; // Base 4 floors of residential
            break;
        }

        // Apply height gradient multiplier
        if (enableHeightGradient) {
            float heightMultiplier = calculateHeightGradientMultiplier(plot, footprint);
            baseFloors = (int)(baseFloors * heightMultiplier);
        }

        return min(baseFloors, maxPossibleFloors);
    }

    int calculateFloorsForZone(const Plot& plot, VoxelType zoneType) {
        const DensityLimits& limits = densityLimits[plot.originalCategory];

        int baseFloors = 0;
        switch (zoneType) {
        case VoxelType::VISIT:
            baseFloors = min(4, limits.maxFloors / 4); // Up to 3 floors, or 1/4 of total
            break;
        case VoxelType::WORK:
            baseFloors = min(16, limits.maxFloors / 2); // Up to 8 floors, or 1/2 of total
            break;
        case VoxelType::LIVE:
            baseFloors = min(24, limits.maxFloors); // Up to 12 floors, or all remaining
            break;
        default:
            baseFloors = 1;
            break;
        }

        // Apply height gradient multiplier for WORK and LIVE zones
        if (enableHeightGradient && (zoneType == VoxelType::WORK || zoneType == VoxelType::LIVE)) {
            // Create a dummy footprint for the calculation
            BuildingFootprint dummyFootprint;
            dummyFootprint.area = plot.area;
            dummyFootprint.center = Point2D(plot.center.x, plot.center.y);
            dummyFootprint.primaryEdgeType = SiteEdgeType::INTERNAL_EDGE; // Default

            float heightMultiplier = calculateHeightGradientMultiplier(plot, dummyFootprint);
            baseFloors = (int)(baseFloors * heightMultiplier);
        }

        return baseFloors;
    }

    // Method to calculate height gradient multiplier
    float calculateHeightGradientMultiplier(const Plot& plot, const BuildingFootprint& footprint) {
        float multiplier = 1.0f;

        // Base multiplier based on parcel category
        switch (plot.originalCategory) {
        case LARGE_PARCEL:
            multiplier = 1.0f + (0.5f * gradientIntensity); // Up to 50% more floors
            break;
        case MEDIUM_PARCEL:
            multiplier = 1.0f + (0.2f * gradientIntensity); // Up to 20% more floors
            break;
        case SMALL_PARCEL:
            multiplier = 1.0f; // No change for small parcels
            break;
        }

        // Add variation based on plot area (within same category)
        float areaFactor = 0.0f;
        switch (plot.originalCategory) {
        case LARGE_PARCEL:
            // For large parcels, bigger ones get taller (100+ unit² gets bonus)
            if (plot.area >= 100.0f) {
                areaFactor = min(0.3f, (plot.area - 100.0f) / 100.0f * 0.3f);
            }
            break;
        case MEDIUM_PARCEL:
            // For medium parcels, bigger ones get taller (80+ unit² gets bonus)
            if (plot.area >= 80.0f) {
                areaFactor = min(0.2f, (plot.area - 80.0f) / 40.0f * 0.2f);
            }
            break;
        case SMALL_PARCEL:
            // Small parcels have minimal variation to keep them consistent
            areaFactor = 0.0f;
            break;
        }

        multiplier += areaFactor * gradientIntensity;

        // Add slight random variation for more natural look
        std::uniform_real_distribution<float> randomDist(0.85f, 1.15f);
        float randomFactor = randomDist(rng);
        multiplier *= randomFactor;

        // Add bonus for special edge conditions (better locations get taller)
        if (footprint.hasMajorRoadFrontage) {
            multiplier += 0.1f * gradientIntensity;
        }
        if (footprint.hasWaterFrontage || footprint.hasParkFrontage) {
            multiplier += 0.35f * gradientIntensity; // Premium locations
        }
        if (footprint.cornerType != SiteCornerType::NO_CORNER) {
            multiplier += 0.05f * gradientIntensity; // Corner locations
        }

        // Ensure multiplier stays within reasonable bounds
        multiplier = max(0.7f, min(2.0f, multiplier));

        if (verboseLogging) {
            cout << "Height gradient multiplier for " << getParcelCategoryName(plot.originalCategory)
                << " plot (area " << plot.area << "): " << multiplier << endl;
        }

        return multiplier;
    }

    // ================== VISUALIZATION METHODS (UPDATED FOR ORIENTATION) ==================

    void drawAll3DBuildings() {
        for (const auto& voxel : building3DVoxels) {
            draw3DVoxel(voxel);
        }

        // NEW: Draw orientation debug info if enabled
        if (showOrientationDebug) {
            drawOrientationDebugInfo();
        }
    }

    void draw3DVoxel(const Voxel3D& voxel) {
        // Color based on voxel type
        switch (voxel.type) {
        case VoxelType::VISIT:
            glColor4f(0.8f, 0.4f, 0.8f, 0.7f); // Purple for retail/commercial
            break;
        case VoxelType::WORK:
            glColor4f(0.2f, 0.6f, 1.0f, 0.7f); // Blue for office
            break;
        case VoxelType::LIVE:
            glColor4f(1.0f, 0.8f, 0.2f, 0.7f); // Yellow for residential
            break;
        case VoxelType::TRANSITION:
            glColor4f(0.6f, 0.6f, 0.6f, 0.7f); // Gray for services
            break;
        default:
            glColor4f(0.5f, 0.5f, 0.5f, 0.7f); // Default gray
            break;
        }

        // Draw filled voxel (box) with proper orientation
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Apply rotation transformation if orientation is enabled
        glPushMatrix();

        if (enableVoxelOrientation && (voxel.orientation.x != 1.0f || voxel.orientation.y != 0.0f)) {
            // Calculate voxel center for rotation
            Point3D voxelCenter = voxel.getCenter();

            // Translate to center, rotate, then translate back
            glTranslatef(voxelCenter.x, voxelCenter.y, voxelCenter.z);
            glRotatef(voxel.rotationAngle * 180.0f / M_PI, 0.0f, 0.0f, 1.0f); // Rotate around Z-axis
            glTranslatef(-voxelCenter.x, -voxelCenter.y, -voxelCenter.z);
        }

        // Draw voxel as a 3D box
        drawBox(voxel.position, voxel.dimensions);

        glPopMatrix();

        glDisable(GL_BLEND);

        // Draw wireframe outline with orientation
        glColor3f(0.3f, 0.3f, 0.3f); // Dark gray outline
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        glPushMatrix();
        if (enableVoxelOrientation && (voxel.orientation.x != 1.0f || voxel.orientation.y != 0.0f)) {
            Point3D voxelCenter = voxel.getCenter();
            glTranslatef(voxelCenter.x, voxelCenter.y, voxelCenter.z);
            glRotatef(voxel.rotationAngle * 180.0f / M_PI, 0.0f, 0.0f, 1.0f);
            glTranslatef(-voxelCenter.x, -voxelCenter.y, -voxelCenter.z);
        }

        drawBox(voxel.position, voxel.dimensions);
        glPopMatrix();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    void drawBox(const Point3D& position, const Point3D& dimensions) {
        float x = position.x;
        float y = position.y;
        float z = position.z;
        float w = dimensions.x;  // Width (X direction)
        float d = dimensions.y;  // Depth (Y direction)
        float h = dimensions.z;  // Height (Z direction - vertical)

        glBegin(GL_QUADS);

        // Bottom face (at ground level Z)
        glVertex3f(x, y, z);
        glVertex3f(x + w, y, z);
        glVertex3f(x + w, y + d, z);
        glVertex3f(x, y + d, z);

        // Top face (at Z + height)
        glVertex3f(x, y, z + h);
        glVertex3f(x, y + d, z + h);
        glVertex3f(x + w, y + d, z + h);
        glVertex3f(x + w, y, z + h);

        // Front face (Y = y)
        glVertex3f(x, y, z);
        glVertex3f(x, y, z + h);
        glVertex3f(x + w, y, z + h);
        glVertex3f(x + w, y, z);

        // Back face (Y = y + depth)
        glVertex3f(x, y + d, z);
        glVertex3f(x + w, y + d, z);
        glVertex3f(x + w, y + d, z + h);
        glVertex3f(x, y + d, z + h);

        // Left face (X = x)
        glVertex3f(x, y, z);
        glVertex3f(x, y + d, z);
        glVertex3f(x, y + d, z + h);
        glVertex3f(x, y, z + h);

        // Right face (X = x + width)
        glVertex3f(x + w, y, z);
        glVertex3f(x + w, y, z + h);
        glVertex3f(x + w, y + d, z + h);
        glVertex3f(x + w, y + d, z);

        glEnd();
    }

    void draw3DBuildingsByProgram() {
        // Draw buildings grouped by program type with orientation

        // First pass: VISIT (ground floor)
        for (const auto& voxel : building3DVoxels) {
            if (voxel.type == VoxelType::VISIT) {
                glColor4f(0.8f, 0.4f, 0.8f, 0.8f); // Purple
                draw3DVoxel(voxel);
            }
        }

        // Second pass: WORK (middle floors)
        for (const auto& voxel : building3DVoxels) {
            if (voxel.type == VoxelType::WORK) {
                glColor4f(0.2f, 0.6f, 1.0f, 0.8f); // Blue
                draw3DVoxel(voxel);
            }
        }

        // Third pass: LIVE (upper floors)
        for (const auto& voxel : building3DVoxels) {
            if (voxel.type == VoxelType::LIVE) {
                glColor4f(1.0f, 0.8f, 0.2f, 0.8f); // Yellow
                draw3DVoxel(voxel);
            }
        }

        // Fourth pass: TRANSITION (service floors)
        for (const auto& voxel : building3DVoxels) {
            if (voxel.type == VoxelType::TRANSITION) {
                glColor4f(0.6f, 0.6f, 0.6f, 0.6f); // Gray
                draw3DVoxel(voxel);
            }
        }

        // NEW: Draw orientation debug info if enabled
        if (showOrientationDebug) {
            drawOrientationDebugInfo();
        }
    }

    // NEW: Draw orientation debug information
    void drawOrientationDebugInfo() {
        // Draw orientation vectors for each building's base voxel
        map<int, Voxel3D> baseVoxels; // footprintId -> base voxel

        // Find base voxel (lowest Z) for each building
        for (const auto& voxel : building3DVoxels) {
            int footprintId = voxel.parentFootprintId;

            if (baseVoxels.find(footprintId) == baseVoxels.end() ||
                voxel.position.z < baseVoxels[footprintId].position.z) {
                baseVoxels[footprintId] = voxel;
            }
        }

        // Draw orientation vectors for base voxels
        glColor3f(1.0f, 0.0f, 0.0f); // Red for orientation vectors
        glLineWidth(3.0f);

        for (const auto& pair : baseVoxels) {
            const Voxel3D& voxel = pair.second;
            Point3D center = voxel.getCenter();

            // Draw orientation vector at building center
            Point3D vectorEnd = Point3D(
                center.x + voxel.orientation.x * 3.0f, // 30m scaled vector
                center.y + voxel.orientation.y * 3.0f,
                center.z + 1.0f // Slightly above building
            );

            glBegin(GL_LINES);
            glVertex3f(center.x, center.y, center.z + 1.0f);
            glVertex3f(vectorEnd.x, vectorEnd.y, vectorEnd.z);
            glEnd();

            // Draw arrow head
            glBegin(GL_TRIANGLES);
            Point3D arrowBase = Point3D(
                vectorEnd.x - voxel.orientation.x * 0.5f,
                vectorEnd.y - voxel.orientation.y * 0.5f,
                vectorEnd.z
            );
            Point3D perpendicular = Point3D(-voxel.orientation.y * 0.3f, voxel.orientation.x * 0.3f, 0);

            glVertex3f(vectorEnd.x, vectorEnd.y, vectorEnd.z);
            glVertex3f(arrowBase.x + perpendicular.x, arrowBase.y + perpendicular.y, arrowBase.z);
            glVertex3f(arrowBase.x - perpendicular.x, arrowBase.y - perpendicular.y, arrowBase.z);
            glEnd();
        }

        glLineWidth(1.0f);
    }

    // ================== ACCESSOR METHODS ==================

    const vector<Voxel3D>& getAll3DVoxels() const {
        return building3DVoxels;
    }

    vector<Voxel3D> get3DVoxelsByFootprint(int footprintId) const {
        vector<Voxel3D> result;
        for (const auto& voxel : building3DVoxels) {
            if (voxel.parentFootprintId == footprintId) {
                result.push_back(voxel);
            }
        }
        return result;
    }

    vector<Voxel3D> get3DVoxelsByType(VoxelType type) const {
        vector<Voxel3D> result;
        for (const auto& voxel : building3DVoxels) {
            if (voxel.type == type) {
                result.push_back(voxel);
            }
        }
        return result;
    }

    // ================== STATISTICS & VALIDATION ==================

    bool validateDependencies() const {
        if (!footprintGenerator) {
            cout << "Error: BuildingFootprintGenerator not initialized" << endl;
            return false;
        }

        if (!parcelSubdivider) {
            cout << "Error: ParcelSubdivider not initialized" << endl;
            return false;
        }

        return true;
    }

    void print3DStatistics() {
        cout << "\n=== ORIENTED 3D BUILDING STATISTICS ===" << endl;

        if (building3DVoxels.empty()) {
            cout << "No 3D buildings generated." << endl;
            return;
        }

        // Count by voxel type
        map<VoxelType, int> typeCounts;
        map<VoxelType, float> typeVolumes;
        float totalVolume = 0;

        for (const auto& voxel : building3DVoxels) {
            typeCounts[voxel.type]++;
            float volume = voxel.getVolume();
            typeVolumes[voxel.type] += volume;
            totalVolume += volume;
        }

        cout << "Total 3D Voxels: " << building3DVoxels.size() << endl;
        cout << "Total Building Volume: " << totalVolume << " unit³ (" << totalVolume * 1000 << "m³)" << endl;
        cout << "Building Scale Factor: " << buildingScaleFactor << " (buildings are " << (buildingScaleFactor * 100) << "% of footprint size)" << endl;
        cout << "Building Setback: " << buildingSetback << " unit (" << buildingSetback * 10 << "m)" << endl;
        cout << "Voxel Orientation: " << (enableVoxelOrientation ? "ENABLED" : "DISABLED") << endl;

        cout << "\nVoxels by Program Type:" << endl;
        cout << "  VISIT (Retail/Commercial): " << typeCounts[VoxelType::VISIT]
            << " voxels, " << typeVolumes[VoxelType::VISIT] << " unit³" << endl;
        cout << "  WORK (Office): " << typeCounts[VoxelType::WORK]
            << " voxels, " << typeVolumes[VoxelType::WORK] << " unit³" << endl;
        cout << "  LIVE (Residential): " << typeCounts[VoxelType::LIVE]
            << " voxels, " << typeVolumes[VoxelType::LIVE] << " unit³" << endl;
        cout << "  TRANSITION (Services): " << typeCounts[VoxelType::TRANSITION]
            << " voxels, " << typeVolumes[VoxelType::TRANSITION] << " unit³" << endl;

        // Calculate program ratios
        if (totalVolume > 0) {
            cout << "\nProgram Volume Ratios:" << endl;
            cout << "  VISIT: " << (typeVolumes[VoxelType::VISIT] / totalVolume) * 100 << "%" << endl;
            cout << "  WORK: " << (typeVolumes[VoxelType::WORK] / totalVolume) * 100 << "%" << endl;
            cout << "  LIVE: " << (typeVolumes[VoxelType::LIVE] / totalVolume) * 100 << "%" << endl;
            cout << "  TRANSITION: " << (typeVolumes[VoxelType::TRANSITION] / totalVolume) * 100 << "%" << endl;
        }

        // Height statistics
        float maxHeight = 0;
        float avgHeight = 0;
        map<int, int> buildingCounts; // footprintId -> voxel count

        for (const auto& voxel : building3DVoxels) {
            float voxelTop = voxel.position.z + voxel.dimensions.z;
            maxHeight = max(maxHeight, voxelTop);
            avgHeight += voxelTop;

            buildingCounts[voxel.parentFootprintId]++;
        }

        if (!building3DVoxels.empty()) {
            avgHeight /= building3DVoxels.size();
        }

        cout << "\nHeight Statistics:" << endl;
        cout << "  Maximum Building Height: " << maxHeight << " unit (" << maxHeight * 10 << "m)" << endl;
        cout << "  Average Voxel Height: " << avgHeight << " unit (" << avgHeight * 10 << "m)" << endl;
        cout << "  Number of Buildings: " << buildingCounts.size() << endl;

        // NEW: Orientation statistics
        cout << "\nOrientation Statistics:" << endl;
        map<int, int> orientationAngles; // Angle bins -> count
        for (const auto& voxel : building3DVoxels) {
            if (voxel.level == 0) { // Only count base voxels
                int angleDegrees = (int)(voxel.rotationAngle * 180.0f / M_PI);
                int angleBin = ((angleDegrees + 15) / 30) * 30; // Round to nearest 30 degrees
                orientationAngles[angleBin]++;
            }
        }

        cout << "  Buildings by Orientation:" << endl;
        for (const auto& pair : orientationAngles) {
            cout << "    " << pair.first << "° ± 15°: " << pair.second << " buildings" << endl;
        }

        cout << "\nORIENTATION STATUS: BUILDINGS ARE ALIGNED TO FOOTPRINT ORIENTATIONS" << endl;
        cout << "=================================" << endl;
    }

    // ================== PARAMETER ADJUSTMENT ==================

    void setDefaultFloorHeight(float height) {
        defaultFloorHeight = height;
        cout << "Default floor height set to: " << height << " unit (" << height * 10 << "m)" << endl;
    }

    void setMaxBuildingHeight(float height) {
        maxBuildingHeight = height;
        cout << "Maximum building height set to: " << height << " unit (" << height * 10 << "m)" << endl;
    }

    void setRespectHeightLimits(bool respect) {
        respectHeightLimits = respect;
        cout << "Height limit enforcement: " << (respect ? "ON" : "OFF") << endl;
    }

    void setHeightGradient(bool enable, float intensity = 1.0f) {
        enableHeightGradient = enable;
        gradientIntensity = max(0.0f, min(2.0f, intensity));
        cout << "Height gradient: " << (enable ? "ON" : "OFF");
        if (enable) {
            cout << " (intensity: " << gradientIntensity << ")";
        }
        cout << endl;
    }

    void setVerboseLogging(bool verbose) {
        verboseLogging = verbose;
        cout << "3D generation verbose logging: " << (verbose ? "ON" : "OFF") << endl;
    }

    // Building scaling parameter controls
    void setBuildingScaleFactor(float scaleFactor) {
        buildingScaleFactor = max(0.5f, min(1.0f, scaleFactor)); // 50% to 100% of footprint size
        cout << "Building scale factor set to: " << buildingScaleFactor
            << " (buildings will be " << (buildingScaleFactor * 100) << "% of footprint size)" << endl;
    }

    void setBuildingSetback(float setback) {
        buildingSetback = max(0.1f, min(2.0f, setback)); // 1m to 20m scaled
        cout << "Building setback set to: " << buildingSetback << " unit (" << buildingSetback * 10 << "m)" << endl;
    }

    void setCenterBuildingsInFootprint(bool center) {
        centerBuildingsInFootprint = center;
        cout << "Center buildings in footprint: " << (center ? "ON" : "OFF") << endl;
    }

    // NEW: Orientation parameter controls
    void setVoxelOrientation(bool enable) {
        enableVoxelOrientation = enable;
        cout << "Voxel orientation to footprint: " << (enable ? "ENABLED" : "DISABLED") << endl;
    }

    void setOrientationDebug(bool show) {
        showOrientationDebug = show;
        cout << "Orientation debug visualization: " << (show ? "ON" : "OFF") << endl;
    }

    void updateDensityLimits(ParcelCategory category, float maxHeight, float maxFAR, int maxFloors) {
        densityLimits[category].maxBuildingHeight = maxHeight;
        densityLimits[category].maxFloorAreaRatio = maxFAR;
        densityLimits[category].maxFloors = maxFloors;

        cout << "Updated density limits for " << getParcelCategoryName(category) << ":" << endl;
        cout << "  Max Height: " << maxHeight << " unit, FAR: " << maxFAR << ", Max Floors: " << maxFloors << endl;
    }

    // ================== UTILITY METHODS ==================

    string getParcelCategoryName(ParcelCategory category) {
        switch (category) {
        case LARGE_PARCEL: return "Large";
        case MEDIUM_PARCEL: return "Medium";
        case SMALL_PARCEL: return "Small";
        default: return "Unknown";
        }
    }

    string getVoxelTypeName(VoxelType type) {
        switch (type) {
        case VoxelType::VISIT: return "VISIT";
        case VoxelType::WORK: return "WORK";
        case VoxelType::LIVE: return "LIVE";
        case VoxelType::TRANSITION: return "TRANSITION";
        default: return "UNKNOWN";
        }
    }

    string getBuildingStrategyName(BuildingStrategy strategy) {
        switch (strategy) {
        case BuildingStrategy::PODIUM_TOWER: return "Podium Tower";
        case BuildingStrategy::UNIFORM_MIXED: return "Uniform Mixed";
        case BuildingStrategy::HORIZONTAL_ZONES: return "Horizontal Zones";
        case BuildingStrategy::VERTICAL_ZONES: return "Vertical Zones";
        case BuildingStrategy::COURTYARD_BLOCK: return "Courtyard Block";
        case BuildingStrategy::STEPPED_MASSING: return "Stepped Massing";
        default: return "Unknown";
        }
    }

    // Clear all 3D building data
    void clear3DBuildings() {
        building3DVoxels.clear();
        nextVoxelId = 0;
        cout << "3D building data cleared" << endl;
    }
};

#endif // BUILDING_3D_GENERATOR_H