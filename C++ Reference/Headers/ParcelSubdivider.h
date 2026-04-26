#ifndef PARCEL_SUBDIVIDER_H
#define PARCEL_SUBDIVIDER_H

// Include dependencies
#include "SiteGeneration.h"
#include "ParcelAnalyzer.h"
#include "RoadAnalyzer.h"

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

// ===================== SUBDIVISION DATA STRUCTURES =====================

// Subdivision levels based on parcel area (scaled for 1 unit = 10m)
enum class SubdivisionLevel {
    NO_SUBDIVISION = 0,    // Small parcels (<250 unit²) - No subdivision
    SINGLE_DIVISION = 1,   // Small parcels (≥250 unit²) - 1 division
    DOUBLE_DIVISION = 2,   // Medium parcels (≥1000 unit²) - 2 divisions  
    QUADRUPLE_DIVISION = 4 // Large parcels (≥1500 unit²) - 4 divisions
};

// Open space types based on size and function
enum class OpenSpaceType {
    NONE,               // No open space
    CENTRAL_PARK,       // Large central open space in large parcels
    PLAZA,              // Hard-surfaced open area
    COURTYARD,          // Interior open space surrounded by buildings
    LINEAR_PARK,        // Linear green space, often along edges
    POCKET_PARK,        // Small park in corner or edge
    GREEN_BUFFER,       // Vegetated buffer space between functions
    GREEN_ROOF          // Roof garden (for future 3D implementation)
};

// Open space structure to hold open space data
struct OpenSpace {
    int id;
    int parentPlotId;
    vector<Point2D> boundary;
    Point2D center;
    float area;
    OpenSpaceType type;
    vector<string> features; // Vegetation, water, seating, etc.

    OpenSpace() : id(-1), parentPlotId(-1), area(0), type(OpenSpaceType::NONE) {}

    void calculateBasicProperties() {
        // Calculate area using shoelace formula
        area = 0;
        if (boundary.size() < 3) return;

        for (size_t i = 0; i < boundary.size(); ++i) {
            size_t j = (i + 1) % boundary.size();
            area += boundary[i].x * boundary[j].y;
            area -= boundary[j].x * boundary[i].y;
        }
        area = abs(area) * 0.5f;

        // Calculate centroid
        float cx = 0, cy = 0;
        for (const auto& pt : boundary) {
            cx += pt.x;
            cy += pt.y;
        }
        center = Point2D(cx / boundary.size(), cy / boundary.size());
    }
};

// Plot structure - enhanced version of Parcel for subdivided plots
struct Plot {
    int id;
    int parentParcelId;
    vector<Point2D> boundary;
    Point2D center;
    float area;
    ParcelCategory originalCategory;  // Category of parent parcel

    // Subdivision information
    int subdivisionLevel;
    int plotIndex;               // Index within subdivision (0 to n-1)
    Point2D subdivisionAxis;     // Direction of subdivision
    Point2D perpendicularDirection; // Perpendicular to subdivision axis

    // Road relationship
    vector<int> adjacentRoadIds;    // IDs from RoadAnalyzer
    Point2D primaryRoadDirection;   // Direction of primary adjacent road
    float distanceToMajorRoad;
    bool hasMajorRoadFrontage;

    // Building constraints
    float buildableArea;
    vector<Point2D> buildablePerimeter;
    float requiredSetback;

    // Open space information
    bool isOpenSpace;            // Is this plot designated as open space
    OpenSpaceType openSpaceType; // Type of open space if this is one
    float openSpaceRatio;        // Percentage of plot area that is open space
    vector<string> openSpaceFeatures; // Features of the open space

    Plot() : id(-1), parentParcelId(-1), area(0), originalCategory(SMALL_PARCEL),
        subdivisionLevel(0), plotIndex(0), distanceToMajorRoad(1000.0f),
        hasMajorRoadFrontage(false), buildableArea(0), requiredSetback(0.5f),
        isOpenSpace(false), openSpaceType(OpenSpaceType::NONE), openSpaceRatio(0.0f) {
    }

    void calculateBasicProperties() {
        // Calculate area using shoelace formula
        area = 0;
        if (boundary.size() < 3) return;

        for (size_t i = 0; i < boundary.size(); ++i) {
            size_t j = (i + 1) % boundary.size();
            area += boundary[i].x * boundary[j].y;
            area -= boundary[j].x * boundary[i].y;
        }
        area = abs(area) * 0.5f;

        // Calculate centroid
        float cx = 0, cy = 0;
        for (const auto& pt : boundary) {
            cx += pt.x;
            cy += pt.y;
        }
        center = Point2D(cx / boundary.size(), cy / boundary.size());
    }

    // Check if plot meets minimum area requirements
    bool isViablePlot() const {
        const float MIN_PLOT_AREA = 25.0f; // 250m² scaled down by 10
        return area >= MIN_PLOT_AREA;
    }

    // Get the primary road frontage edge
    vector<Point2D> getPrimaryFrontageEdge() const {
        if (boundary.size() < 2) return vector<Point2D>();

        // Find edge closest to primary road direction
        // For now, return the first edge - can be enhanced with road analysis
        return { boundary[0], boundary[1] };
    }
};

// Subdivision parameters for different parcel categories
struct SubdivisionParameters {
    SubdivisionLevel level;
    float minPlotArea;           // Minimum area for resulting plots
    float preferredAspectRatio;  // Preferred width/height ratio
    float roadAlignmentWeight;   // How much to weight road direction
    bool allowIrregularSplits;   // Allow irregular subdivisions

    // Open space parameters
    float openSpaceRatio;        // Percentage of area to be open space
    OpenSpaceType preferredOpenSpaceType; // Preferred type of open space

    SubdivisionParameters(SubdivisionLevel lvl = SubdivisionLevel::NO_SUBDIVISION)
        : level(lvl), minPlotArea(25.0f), preferredAspectRatio(1.5f),
        roadAlignmentWeight(0.8f), allowIrregularSplits(false),
        openSpaceRatio(0.0f), preferredOpenSpaceType(OpenSpaceType::NONE) {
    }
};

// Subdivision result structure
struct SubdivisionResult {
    vector<Plot> plots;
    bool successful;
    string failureReason;

    SubdivisionResult() : successful(true) {}
};

// ===================== PARCEL SUBDIVIDER CLASS =====================

class ParcelSubdivider {
private:
    // Dependencies
    SiteGeneration* siteGen;
    ParcelAnalyzer* parcelAnalyzer;
    RoadAnalyzer* roadAnalyzer;

    // Generated plots
    vector<Plot> allPlots;
    vector<OpenSpace> allOpenSpaces;
    map<int, SubdivisionResult> subdivisionResults; // parcelId -> result

    // Parameters
    map<ParcelCategory, SubdivisionParameters> categoryParameters;
    float globalMinPlotArea;
    int nextPlotId;
    int nextOpenSpaceId;

    // Random number generator for variation
    std::random_device rd;
    std::mt19937 rng;

public:
    // ================== CONSTRUCTOR & DESTRUCTOR ==================

    ParcelSubdivider(SiteGeneration* siteGeneration,
        ParcelAnalyzer* parcelAnalyzer,
        RoadAnalyzer* roadAnalyzer)
        : siteGen(siteGeneration), parcelAnalyzer(parcelAnalyzer),
        roadAnalyzer(roadAnalyzer), globalMinPlotArea(25.0f), nextPlotId(0),
        nextOpenSpaceId(0), rng(rd()) {

        cout << "=== ParcelSubdivider Initialized ===" << endl;
        initializeSubdivisionParameters();
    }

    ~ParcelSubdivider() {
        cout << "=== ParcelSubdivider Destroyed ===" << endl;
    }

    // ================== INITIALIZATION METHODS ==================

    void initializeSubdivisionParameters() {
        // Large parcels: 4x subdivision, lower density, more open space
        categoryParameters[LARGE_PARCEL] = SubdivisionParameters(SubdivisionLevel::QUADRUPLE_DIVISION);
        categoryParameters[LARGE_PARCEL].minPlotArea = 37.5f;  // 375m² scaled
        categoryParameters[LARGE_PARCEL].preferredAspectRatio = 1.5f;
        categoryParameters[LARGE_PARCEL].roadAlignmentWeight = 0.9f;
        categoryParameters[LARGE_PARCEL].allowIrregularSplits = true;
        categoryParameters[LARGE_PARCEL].openSpaceRatio = 0.25f; // 25% open space
        categoryParameters[LARGE_PARCEL].preferredOpenSpaceType = OpenSpaceType::CENTRAL_PARK;

        // Medium parcels: 2x subdivision, medium density, courtyards
        categoryParameters[MEDIUM_PARCEL] = SubdivisionParameters(SubdivisionLevel::DOUBLE_DIVISION);
        categoryParameters[MEDIUM_PARCEL].minPlotArea = 50.0f;  // 500m² scaled
        categoryParameters[MEDIUM_PARCEL].preferredAspectRatio = 1.2f;
        categoryParameters[MEDIUM_PARCEL].roadAlignmentWeight = 0.8f;
        categoryParameters[MEDIUM_PARCEL].allowIrregularSplits = true;
        categoryParameters[MEDIUM_PARCEL].openSpaceRatio = 0.15f; // 15% open space
        categoryParameters[MEDIUM_PARCEL].preferredOpenSpaceType = OpenSpaceType::COURTYARD;

        // Small parcels: 1x or no subdivision, high density, minimal open space
        categoryParameters[SMALL_PARCEL] = SubdivisionParameters(SubdivisionLevel::SINGLE_DIVISION);
        categoryParameters[SMALL_PARCEL].minPlotArea = 25.0f;  // 250m² scaled
        categoryParameters[SMALL_PARCEL].preferredAspectRatio = 1.0f;
        categoryParameters[SMALL_PARCEL].roadAlignmentWeight = 0.7f;
        categoryParameters[SMALL_PARCEL].allowIrregularSplits = false;
        categoryParameters[SMALL_PARCEL].openSpaceRatio = 0.05f; // 5% open space
        categoryParameters[SMALL_PARCEL].preferredOpenSpaceType = OpenSpaceType::POCKET_PARK;

        cout << "Subdivision parameters initialized for all parcel categories" << endl;
    }

    // ================== MAIN SUBDIVISION METHODS ==================

    // Subdivide all parcels in the site
    void subdivideAllParcels() {
        cout << "\n=== SUBDIVIDING ALL PARCELS ===" << endl;

        if (!validateDependencies()) {
            cout << "Error: Required dependencies not available" << endl;
            return;
        }

        const auto& parcels = siteGen->getParcels();
        allPlots.clear();
        allOpenSpaces.clear();
        subdivisionResults.clear();
        nextPlotId = 0;
        nextOpenSpaceId = 0;

        for (size_t i = 0; i < parcels.size(); ++i) {
            const auto& parcel = parcels[i];
            SubdivisionResult result = subdivideParcel(parcel);

            subdivisionResults[parcel.id] = result;

            if (result.successful) {
                for (auto& plot : result.plots) {
                    plot.id = nextPlotId++;

                    // Generate open spaces if needed
                    if (!plot.isOpenSpace && shouldGenerateOpenSpace(plot)) {
                        generateOpenSpaceInPlot(plot);
                    }

                    allPlots.push_back(plot);
                }
            }
        }

        cout << "Subdivision complete. Generated " << allPlots.size() << " plots from "
            << parcels.size() << " parcels with " << countOpenSpacePlots() << " open spaces" << endl;
        printSubdivisionStatistics();
    }

    // Subdivide a single parcel
    SubdivisionResult subdivideParcel(const Parcel& parcel) {
        SubdivisionResult result;

        // Get parcel category and parameters
        ParcelCategory category = parcelAnalyzer->classifyParcelBySize(parcel);
        SubdivisionParameters params = categoryParameters[category];

        cout << "Subdividing parcel " << parcel.id << " (Category: "
            << getParcelCategoryName(category) << ", Area: " << parcel.area << ")" << endl;

        // Check if subdivision is needed/feasible
        if (params.level == SubdivisionLevel::NO_SUBDIVISION ||
            parcel.area < params.minPlotArea * (int)params.level) {

            // No subdivision - create single plot from parcel
            Plot singlePlot = createPlotFromParcel(parcel, 0, 0);
            result.plots.push_back(singlePlot);
            cout << "  No subdivision applied (single plot)" << endl;
            return result;
        }

        // Determine subdivision axis based on road alignment
        Point2D subdivisionAxis = calculateOptimalSubdivisionAxis(parcel, params);

        // Perform subdivision based on level
        switch (params.level) {
        case SubdivisionLevel::SINGLE_DIVISION:
            result = performSingleDivision(parcel, subdivisionAxis, params);
            break;
        case SubdivisionLevel::DOUBLE_DIVISION:
            result = performDoubleDivisionWithOpenSpace(parcel, subdivisionAxis, params);
            break;
        case SubdivisionLevel::QUADRUPLE_DIVISION:
            result = performQuadrupleDivisionWithOpenSpace(parcel, subdivisionAxis, params);
            break;
        default:
            result.successful = false;
            result.failureReason = "Invalid subdivision level";
            break;
        }

        // Validate and enhance resulting plots
        if (result.successful) {
            for (auto& plot : result.plots) {
                enhancePlotProperties(plot, parcel);

                if (!plot.isViablePlot() && !plot.isOpenSpace) {
                    cout << "  Warning: Plot " << plot.plotIndex << " below minimum area" << endl;
                }
            }
        }

        return result;
    }

    // ================== SUBDIVISION IMPLEMENTATIONS ==================

    SubdivisionResult performSingleDivision(const Parcel& parcel,
        const Point2D& axis,
        const SubdivisionParameters& params) {
        SubdivisionResult result;

        // Find the best split line through parcel center
        Point2D splitStart, splitEnd;
        if (!findOptimalSplitLine(parcel, axis, splitStart, splitEnd)) {
            result.successful = false;
            result.failureReason = "Could not find valid split line";
            return result;
        }

        // Split the parcel
        vector<vector<Point2D>> splitBoundaries = splitPolygonByLine(parcel.boundary, splitStart, splitEnd);

        if (splitBoundaries.size() != 2) {
            result.successful = false;
            result.failureReason = "Split did not produce exactly 2 polygons";
            return result;
        }

        // Create plots from split boundaries
        for (size_t i = 0; i < splitBoundaries.size(); ++i) {
            Plot plot = createPlotFromBoundary(parcel, splitBoundaries[i], 1, i);
            plot.subdivisionAxis = axis;

            // Determine if this plot should be open space
            if (i == 0 && parcel.area >= 40.0f) {  // Only consider if parcel is large enough
                // For single division, we might designate smaller plot as pocket park
                float openSpaceThreshold = 0.05f; // 5% chance for small parcels
                std::uniform_real_distribution<float> dist(0.0f, 1.0f);
                if (dist(rng) < openSpaceThreshold) {
                    designateAsOpenSpace(plot, OpenSpaceType::POCKET_PARK);
                }
            }

            result.plots.push_back(plot);
        }

        cout << "  Single division successful (2 plots)" << endl;
        return result;
    }

    SubdivisionResult performDoubleDivisionWithOpenSpace(const Parcel& parcel,
        const Point2D& axis,
        const SubdivisionParameters& params) {
        SubdivisionResult result;

        // First split along primary axis
        SubdivisionResult firstSplit = performSingleDivision(parcel, axis, params);
        if (!firstSplit.successful) {
            result.successful = false;
            result.failureReason = "First split failed: " + firstSplit.failureReason;
            return result;
        }

        // Randomly select one plot for potential courtyard space
        int courtyardPlotIndex = -1;
        bool createCourtyard = false;

        // Only create courtyards for medium or large parcels
        if (parcel.category == MEDIUM_PARCEL || parcel.category == LARGE_PARCEL) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng) < params.openSpaceRatio * 1.5f) { // Increased chance
                createCourtyard = true;
                courtyardPlotIndex = (firstSplit.plots.size() > 1) ?
                    (dist(rng) < 0.5f ? 0 : 1) : 0;
            }
        }

        // Second split along perpendicular axis
        Point2D perpAxis(-axis.y, axis.x); // Rotate 90 degrees

        for (size_t i = 0; i < firstSplit.plots.size(); ++i) {
            const auto& plot = firstSplit.plots[i];

            // Convert plot back to parcel-like structure for splitting
            Parcel tempParcel;
            tempParcel.boundary = plot.boundary;
            tempParcel.center = plot.center;
            tempParcel.area = plot.area;

            // Split this intermediate plot
            SubdivisionResult secondSplit = performSingleDivision(tempParcel, perpAxis, params);

            if (secondSplit.successful) {
                for (size_t j = 0; j < secondSplit.plots.size(); ++j) {
                    auto& finalPlot = secondSplit.plots[j];
                    finalPlot.parentParcelId = parcel.id;
                    finalPlot.subdivisionLevel = 2;
                    finalPlot.subdivisionAxis = axis; // Keep original axis
                    finalPlot.plotIndex = result.plots.size();

                    // Check if this is the plot we want to turn into a courtyard
                    if (createCourtyard && i == courtyardPlotIndex && j == 0) {
                        designateAsOpenSpace(finalPlot, OpenSpaceType::COURTYARD);
                    }

                    result.plots.push_back(finalPlot);
                }
            }
            else {
                // If second split fails, keep the intermediate plot
                Plot finalPlot = plot;
                finalPlot.subdivisionLevel = 2;
                finalPlot.plotIndex = result.plots.size();
                result.plots.push_back(finalPlot);
            }
        }

        cout << "  Double division successful (" << result.plots.size() << " plots)" << endl;
        return result;
    }

    SubdivisionResult performQuadrupleDivisionWithOpenSpace(const Parcel& parcel,
        const Point2D& axis,
        const SubdivisionParameters& params) {
        SubdivisionResult result;

        // For large parcels, we'll designate the central area as a park
        bool createCentralPark = false;

        // Decide whether to create a central park based on parcel size
        if (parcel.category == LARGE_PARCEL && parcel.area >= 100.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            // High probability for large parcels
            createCentralPark = (dist(rng) < 0.7f);
        }

        // Perform double division first
        SubdivisionResult doubleSplit = performDoubleDivisionWithOpenSpace(parcel, axis, params);
        if (!doubleSplit.successful) {
            result.successful = false;
            result.failureReason = "Double split failed: " + doubleSplit.failureReason;
            return result;
        }

        // Further subdivide larger plots if possible
        float averagePlotArea = parcel.area / 4.0f;
        float maxAllowableArea = averagePlotArea * 1.5f;

        // Track central location for potential central park
        Point2D centralLocation(0, 0);
        int centralAreaCount = 0;

        for (const auto& plot : doubleSplit.plots) {
            if (plot.area > maxAllowableArea) {
                // Try to subdivide this larger plot
                Parcel tempParcel;
                tempParcel.boundary = plot.boundary;
                tempParcel.center = plot.center;
                tempParcel.area = plot.area;

                // Choose subdivision direction based on plot shape
                Point2D localAxis = chooseLocalSubdivisionAxis(plot);
                SubdivisionResult localSplit = performSingleDivision(tempParcel, localAxis, params);

                if (localSplit.successful) {
                    for (auto& finalPlot : localSplit.plots) {
                        finalPlot.parentParcelId = parcel.id;
                        finalPlot.subdivisionLevel = 4;
                        finalPlot.subdivisionAxis = axis; // Keep original axis
                        finalPlot.plotIndex = result.plots.size();

                        // Accumulate central location for central park
                        centralLocation.x += finalPlot.center.x;
                        centralLocation.y += finalPlot.center.y;
                        centralAreaCount++;

                        result.plots.push_back(finalPlot);
                    }
                }
                else {
                    // Keep original plot if subdivision fails
                    Plot finalPlot = plot;
                    finalPlot.subdivisionLevel = 4;
                    finalPlot.plotIndex = result.plots.size();

                    // Accumulate central location for central park
                    centralLocation.x += finalPlot.center.x;
                    centralLocation.y += finalPlot.center.y;
                    centralAreaCount++;

                    result.plots.push_back(finalPlot);
                }
            }
            else {
                // Plot is appropriately sized, keep as is
                Plot finalPlot = plot;
                finalPlot.subdivisionLevel = 4;
                finalPlot.plotIndex = result.plots.size();

                // Accumulate central location for central park
                centralLocation.x += finalPlot.center.x;
                centralLocation.y += finalPlot.center.y;
                centralAreaCount++;

                result.plots.push_back(finalPlot);
            }
        }

        // Create central park if needed
        if (createCentralPark && centralAreaCount > 0) {
            // Calculate central location
            centralLocation.x /= centralAreaCount;
            centralLocation.y /= centralAreaCount;

            // Find plot closest to central location
            int centralPlotIndex = -1;
            float minDistance = numeric_limits<float>::max();

            for (size_t i = 0; i < result.plots.size(); ++i) {
                float dist = centralLocation.distance(result.plots[i].center);
                if (dist < minDistance) {
                    minDistance = dist;
                    centralPlotIndex = i;
                }
            }

            // Designate central plot as park if it's not already open space
            if (centralPlotIndex >= 0 && !result.plots[centralPlotIndex].isOpenSpace) {
                designateAsOpenSpace(result.plots[centralPlotIndex], OpenSpaceType::CENTRAL_PARK);
            }
        }

        cout << "  Quadruple division successful (" << result.plots.size() << " plots)" << endl;
        return result;
    }

    // ================== OPEN SPACE METHODS ==================

    bool shouldGenerateOpenSpace(const Plot& plot) {
        // Get the parameters for this plot's original category
        const auto& params = categoryParameters[plot.originalCategory];

        // Basic probability check based on open space ratio
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng) < params.openSpaceRatio;
    }

    void designateAsOpenSpace(Plot& plot, OpenSpaceType type) {
        // Flag the plot as open space
        plot.isOpenSpace = true;
        plot.openSpaceType = type;

        // Set open space ratio to 100% since entire plot is open space
        plot.openSpaceRatio = 1.0f;

        // Add appropriate features based on type
        plot.openSpaceFeatures.clear();

        switch (type) {
        case OpenSpaceType::CENTRAL_PARK:
            plot.openSpaceFeatures = { "trees", "paths", "lawn", "playground", "seating" };
            break;

        case OpenSpaceType::PLAZA:
            plot.openSpaceFeatures = { "paving", "fountain", "seating", "public_art" };
            break;

        case OpenSpaceType::COURTYARD:
            plot.openSpaceFeatures = { "gardens", "seating", "water_feature" };
            break;

        case OpenSpaceType::LINEAR_PARK:
            plot.openSpaceFeatures = { "trees", "path", "benches" };
            break;

        case OpenSpaceType::POCKET_PARK:
            plot.openSpaceFeatures = { "garden", "bench", "small_feature" };
            break;

        case OpenSpaceType::GREEN_BUFFER:
            plot.openSpaceFeatures = { "trees", "shrubs", "grass" };
            break;

        default:
            plot.openSpaceFeatures = { "open_space" };
            break;
        }
    }

    void generateOpenSpaceInPlot(Plot& plot) {
        const auto& params = categoryParameters[plot.originalCategory];

        // Determine open space type based on plot size and category
        OpenSpaceType openSpaceType = params.preferredOpenSpaceType;

        // Create open space
        switch (plot.originalCategory) {
        case LARGE_PARCEL:
            if (plot.area >= 80.0f) {
                // Large plots get central park or linear park
                openSpaceType = (plot.hasMajorRoadFrontage) ?
                    OpenSpaceType::LINEAR_PARK : OpenSpaceType::CENTRAL_PARK;
            }
            else {
                openSpaceType = OpenSpaceType::PLAZA;
            }
            break;

        case MEDIUM_PARCEL:
            // Medium plots get courtyards or plazas
            openSpaceType = (plot.hasMajorRoadFrontage) ?
                OpenSpaceType::PLAZA : OpenSpaceType::COURTYARD;
            break;

        case SMALL_PARCEL:
            // Small plots get pocket parks or green buffers
            openSpaceType = (plot.hasMajorRoadFrontage) ?
                OpenSpaceType::POCKET_PARK : OpenSpaceType::GREEN_BUFFER;
            break;

        default:
            openSpaceType = OpenSpaceType::POCKET_PARK;
            break;
        }

        // Create a separate open space within the plot
        OpenSpace openSpace;
        openSpace.id = nextOpenSpaceId++;
        openSpace.parentPlotId = plot.id;
        openSpace.type = openSpaceType;

        // Determine size of open space (as percent of plot)
        float openSpacePercent = params.openSpaceRatio;

        // Create boundary by scaling down the plot boundary
        vector<Point2D> scaledBoundary;
        for (const auto& pt : plot.boundary) {
            Point2D scaledPt = plot.center + (pt - plot.center) * sqrt(openSpacePercent);
            scaledBoundary.push_back(scaledPt);
        }

        openSpace.boundary = scaledBoundary;
        openSpace.calculateBasicProperties();

        // Add features based on type
        switch (openSpaceType) {
        case OpenSpaceType::CENTRAL_PARK:
            openSpace.features = { "trees", "paths", "lawn", "playground", "seating" };
            break;

        case OpenSpaceType::PLAZA:
            openSpace.features = { "paving", "fountain", "seating", "public_art" };
            break;

        case OpenSpaceType::COURTYARD:
            openSpace.features = { "gardens", "seating", "water_feature" };
            break;

        case OpenSpaceType::LINEAR_PARK:
            openSpace.features = { "trees", "path", "benches" };
            break;

        case OpenSpaceType::POCKET_PARK:
            openSpace.features = { "garden", "bench", "small_feature" };
            break;

        case OpenSpaceType::GREEN_BUFFER:
            openSpace.features = { "trees", "shrubs", "grass" };
            break;

        default:
            openSpace.features = { "open_space" };
            break;
        }

        // Update plot's open space ratio
        plot.openSpaceRatio = openSpacePercent;

        // Add to collection
        allOpenSpaces.push_back(openSpace);
    }

    int countOpenSpacePlots() const {
        int count = 0;
        for (const auto& plot : allPlots) {
            if (plot.isOpenSpace) {
                count++;
            }
        }
        return count;
    }

    // ================== SUBDIVISION AXIS CALCULATION ==================

    Point2D calculateOptimalSubdivisionAxis(const Parcel& parcel, const SubdivisionParameters& params) {
        // Get road directions near this parcel
        vector<Point2D> candidateDirections;
        vector<float> directionWeights;

        // Get adjacent roads and their directions
        auto adjacentRoads = parcelAnalyzer->getAdjacentRoads(parcel);

        for (const auto& roadInfo : adjacentRoads) {
            Point2D roadDirection = roadInfo.segment.direction;
            float weight = calculateRoadInfluence(parcel, roadInfo, params);

            candidateDirections.push_back(roadDirection);
            directionWeights.push_back(weight);

            // Also consider perpendicular direction
            Point2D perpDirection(-roadDirection.y, roadDirection.x);
            candidateDirections.push_back(perpDirection);
            directionWeights.push_back(weight * 0.5f); // Less weight for perpendicular
        }

        // If no adjacent roads, use network primary direction
        if (candidateDirections.empty() && roadAnalyzer) {
            const auto& networkAnalysis = roadAnalyzer->getNetworkAnalysis();
            candidateDirections.push_back(networkAnalysis.primaryDirection);
            directionWeights.push_back(1.0f);
            candidateDirections.push_back(networkAnalysis.secondaryDirection);
            directionWeights.push_back(0.8f);
        }

        // Default to cardinal directions if nothing else available
        if (candidateDirections.empty()) {
            return Point2D(1, 0); // East-west
        }

        // Calculate weighted average direction
        Point2D optimalAxis = calculateWeightedAverageDirection(candidateDirections, directionWeights);

        // Validate and adjust for parcel geometry
        optimalAxis = adjustAxisForParcelGeometry(parcel, optimalAxis);

        return optimalAxis.normalize();
    }

    float calculateRoadInfluence(const Parcel& parcel, const RoadInfo& roadInfo, const SubdivisionParameters& params) {
        float influence = params.roadAlignmentWeight;

        // Weight by road hierarchy (assuming width indicates importance)
        influence *= min(2.0f, roadInfo.segment.width / 6.0f);

        // Weight by frontage length
        influence *= min(2.0f, roadInfo.frontageLength / 10.0f);

        // Weight by proximity
        float maxDistance = 5.0f; // 50m scaled
        float proximityWeight = max(0.1f, 1.0f - roadInfo.distanceToParcel / maxDistance);
        influence *= proximityWeight;

        return influence;
    }

    Point2D calculateWeightedAverageDirection(const vector<Point2D>& directions, const vector<float>& weights) {
        Point2D avgDirection(0, 0);
        float totalWeight = 0;

        for (size_t i = 0; i < directions.size(); ++i) {
            avgDirection.x += directions[i].x * weights[i];
            avgDirection.y += directions[i].y * weights[i];
            totalWeight += weights[i];
        }

        if (totalWeight > 0) {
            avgDirection.x /= totalWeight;
            avgDirection.y /= totalWeight;
        }

        return avgDirection.normalize();
    }

    Point2D adjustAxisForParcelGeometry(const Parcel& parcel, const Point2D& proposedAxis) {
        // Calculate parcel's major axis
        Point2D parcelMajorAxis = calculateParcelMajorAxis(parcel);

        // If proposed axis is too different from parcel major axis, blend them
        float dotProduct = abs(proposedAxis.x * parcelMajorAxis.x + proposedAxis.y * parcelMajorAxis.y);

        if (dotProduct < 0.5f) { // Very different directions
            // Blend proposed axis with parcel major axis
            Point2D blendedAxis;
            blendedAxis.x = 0.3f * parcelMajorAxis.x + 0.7f * proposedAxis.x;
            blendedAxis.y = 0.3f * parcelMajorAxis.y + 0.7f * proposedAxis.y;
            return blendedAxis.normalize();
        }

        return proposedAxis;
    }

    Point2D calculateParcelMajorAxis(const Parcel& parcel) {
        if (parcel.boundary.size() < 3) return Point2D(1, 0);

        // Find bounding box
        float minX = parcel.boundary[0].x, maxX = parcel.boundary[0].x;
        float minY = parcel.boundary[0].y, maxY = parcel.boundary[0].y;

        for (const auto& pt : parcel.boundary) {
            minX = min(minX, pt.x);
            maxX = max(maxX, pt.x);
            minY = min(minY, pt.y);
            maxY = max(maxY, pt.y);
        }

        float width = maxX - minX;
        float height = maxY - minY;

        // Return direction of major axis
        if (width > height) {
            return Point2D(1, 0); // Horizontal
        }
        else {
            return Point2D(0, 1); // Vertical
        }
    }

    Point2D chooseLocalSubdivisionAxis(const Plot& plot) {
        // For local subdivision, prioritize the direction that creates more regular plots
        Point2D majorAxis = calculatePlotMajorAxis(plot);

        // Add some variation to avoid monotonous subdivisions
        std::uniform_real_distribution<float> angleDist(-15.0f, 15.0f);
        float randomAngle = angleDist(rng) * M_PI / 180.0f;

        float cosA = cos(randomAngle);
        float sinA = sin(randomAngle);

        Point2D variedAxis;
        variedAxis.x = majorAxis.x * cosA - majorAxis.y * sinA;
        variedAxis.y = majorAxis.x * sinA + majorAxis.y * cosA;

        return variedAxis.normalize();
    }

    // Helper method for calculating Plot's major axis
    Point2D calculatePlotMajorAxis(const Plot& plot) {
        if (plot.boundary.size() < 3) return Point2D(1, 0);

        // Find bounding box
        float minX = plot.boundary[0].x, maxX = plot.boundary[0].x;
        float minY = plot.boundary[0].y, maxY = plot.boundary[0].y;

        for (const auto& pt : plot.boundary) {
            minX = min(minX, pt.x);
            maxX = max(maxX, pt.x);
            minY = min(minY, pt.y);
            maxY = max(maxY, pt.y);
        }

        float width = maxX - minX;
        float height = maxY - minY;

        // Return direction of major axis
        if (width > height) {
            return Point2D(1, 0); // Horizontal
        }
        else {
            return Point2D(0, 1); // Vertical
        }
    }

    // ================== GEOMETRY OPERATIONS ==================

    bool findOptimalSplitLine(const Parcel& parcel, const Point2D& axis, Point2D& splitStart, Point2D& splitEnd) {
        // Create a line through parcel center with given direction
        Point2D center = parcel.center;

        // Extend line in both directions to ensure it crosses parcel boundary
        float extend = 10.0f; // Large enough to cross any parcel
        Point2D lineStart = center + axis * (-extend);
        Point2D lineEnd = center + axis * extend;

        // Find intersections with parcel boundary
        vector<Point2D> intersections = findLinePolygonIntersections(lineStart, lineEnd, parcel.boundary);

        if (intersections.size() >= 2) {
            splitStart = intersections[0];
            splitEnd = intersections[1];
            return true;
        }

        return false;
    }

    vector<Point2D> findLinePolygonIntersections(const Point2D& lineStart, const Point2D& lineEnd,
        const vector<Point2D>& polygon) {
        vector<Point2D> intersections;

        for (size_t i = 0; i < polygon.size(); ++i) {
            size_t j = (i + 1) % polygon.size();

            Point2D intersection;
            if (findLineSegmentIntersection(lineStart, lineEnd, polygon[i], polygon[j], intersection)) {
                // Check if this intersection is not too close to existing ones
                bool tooClose = false;
                for (const auto& existing : intersections) {
                    if (intersection.distance(existing) < 0.1f) {
                        tooClose = true;
                        break;
                    }
                }

                if (!tooClose) {
                    intersections.push_back(intersection);
                }
            }
        }

        return intersections;
    }

    bool findLineSegmentIntersection(const Point2D& line1Start, const Point2D& line1End,
        const Point2D& line2Start, const Point2D& line2End,
        Point2D& intersection) {
        float x1 = line1Start.x, y1 = line1Start.y;
        float x2 = line1End.x, y2 = line1End.y;
        float x3 = line2Start.x, y3 = line2Start.y;
        float x4 = line2End.x, y4 = line2End.y;

        float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

        if (abs(denom) < 0.001f) {
            return false; // Lines are parallel
        }

        float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
        float u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

        if (u >= 0.0f && u <= 1.0f) { // Intersection is on the polygon edge
            intersection.x = x1 + t * (x2 - x1);
            intersection.y = y1 + t * (y2 - y1);
            return true;
        }

        return false;
    }

    vector<vector<Point2D>> splitPolygonByLine(const vector<Point2D>& polygon,
        const Point2D& splitStart,
        const Point2D& splitEnd) {
        vector<vector<Point2D>> result;

        if (polygon.size() < 3) return result;

        // Find intersection points and their positions on polygon boundary
        vector<pair<Point2D, size_t>> intersectionData; // intersection point and edge index

        for (size_t i = 0; i < polygon.size(); ++i) {
            size_t j = (i + 1) % polygon.size();

            Point2D intersection;
            if (findLineSegmentIntersection(splitStart, splitEnd, polygon[i], polygon[j], intersection)) {
                intersectionData.push_back({ intersection, i });
            }
        }

        if (intersectionData.size() < 2) {
            return result; // Cannot split
        }

        // Sort intersections by distance along the split line
        sort(intersectionData.begin(), intersectionData.end(),
            [&](const pair<Point2D, size_t>& a, const pair<Point2D, size_t>& b) {
                Point2D dirToA = a.first - splitStart;
                Point2D dirToB = b.first - splitStart;
                Point2D splitDir = splitEnd - splitStart;
                float projA = dirToA.x * splitDir.x + dirToA.y * splitDir.y;
                float projB = dirToB.x * splitDir.x + dirToB.y * splitDir.y;
                return projA < projB;
            });

        // Use first two intersections to split polygon
        Point2D int1 = intersectionData[0].first;
        Point2D int2 = intersectionData[1].first;
        size_t edge1 = intersectionData[0].second;
        size_t edge2 = intersectionData[1].second;

        // Create two polygons
        vector<Point2D> poly1, poly2;

        // First polygon: from edge1 intersection to edge2 intersection
        poly1.push_back(int1);

        size_t current = (edge1 + 1) % polygon.size();
        while (current != (edge2 + 1) % polygon.size()) {
            poly1.push_back(polygon[current]);
            current = (current + 1) % polygon.size();
        }

        poly1.push_back(int2);

        // Second polygon: from edge2 intersection back to edge1 intersection
        poly2.push_back(int2);

        current = (edge2 + 1) % polygon.size();
        while (current != (edge1 + 1) % polygon.size()) {
            poly2.push_back(polygon[current]);
            current = (current + 1) % polygon.size();
        }

        poly2.push_back(int1);

        result.push_back(poly1);
        result.push_back(poly2);

        return result;
    }

    // ================== PLOT CREATION & ENHANCEMENT ==================

    Plot createPlotFromParcel(const Parcel& parcel, int subdivisionLevel, int plotIndex) {
        Plot plot;
        plot.parentParcelId = parcel.id;
        plot.boundary = parcel.boundary;
        plot.center = parcel.center;
        plot.area = parcel.area;
        plot.originalCategory = parcel.category;
        plot.subdivisionLevel = subdivisionLevel;
        plot.plotIndex = plotIndex;

        return plot;
    }

    Plot createPlotFromBoundary(const Parcel& parentParcel, const vector<Point2D>& boundary,
        int subdivisionLevel, int plotIndex) {
        Plot plot;
        plot.parentParcelId = parentParcel.id;
        plot.boundary = boundary;
        plot.originalCategory = parentParcel.category;
        plot.subdivisionLevel = subdivisionLevel;
        plot.plotIndex = plotIndex;

        plot.calculateBasicProperties();

        return plot;
    }

    void enhancePlotProperties(Plot& plot, const Parcel& originalParcel) {
        // Analyze road relationships
        if (parcelAnalyzer && roadAnalyzer) {
            analyzePlotRoadRelationship(plot);
        }

        // Calculate buildable area (considering setbacks)
        calculateBuildableArea(plot);

        // Determine setback requirements
        determinePlotSetbacks(plot);
    }

    void analyzePlotRoadRelationship(Plot& plot) {
        // Convert plot to temporary parcel for analysis
        Parcel tempParcel;
        tempParcel.boundary = plot.boundary;
        tempParcel.center = plot.center;
        tempParcel.area = plot.area;

        // Get adjacent roads
        auto adjacentRoads = parcelAnalyzer->getAdjacentRoads(tempParcel);

        if (!adjacentRoads.empty()) {
            // Store road relationship data
            for (const auto& roadInfo : adjacentRoads) {
                plot.adjacentRoadIds.push_back(-1); // Would need road IDs from road analyzer

                if (roadInfo.isPrimaryFrontage) {
                    plot.primaryRoadDirection = roadInfo.segment.direction;
                    plot.hasMajorRoadFrontage = (roadInfo.segment.width >= 1.0f); // 10m scaled
                    plot.distanceToMajorRoad = roadInfo.distanceToParcel;
                }
            }
        }
    }

    void calculateBuildableArea(Plot& plot) {
        // If plot is designated as open space, buildable area is 0
        if (plot.isOpenSpace) {
            plot.buildableArea = 0;
            return;
        }

        // Simple setback calculation - can be enhanced
        float setback = plot.requiredSetback;

        if (plot.boundary.size() < 3) {
            plot.buildableArea = 0;
            return;
        }

        // Calculate area after setbacks (simplified)
        float perimeterReduction = setback * 2; // Rough approximation
        float sideLength = sqrt(plot.area); // Assume roughly square
        float reducedSideLength = max(0.0f, sideLength - perimeterReduction);

        plot.buildableArea = reducedSideLength * reducedSideLength;

        // Adjust for open space ratio
        plot.buildableArea *= (1.0f - plot.openSpaceRatio);

        // Cap at 90% of total area
        plot.buildableArea = max(0.0f, min(plot.buildableArea, plot.area * 0.9f));
    }

    void determinePlotSetbacks(Plot& plot) {
        // Skip setback calculation for open spaces
        if (plot.isOpenSpace) {
            plot.requiredSetback = 0.0f;
            return;
        }

        // Base setback
        plot.requiredSetback = 0.5f; // 5m scaled

        // Adjust based on original parcel category
        switch (plot.originalCategory) {
        case LARGE_PARCEL:
            plot.requiredSetback = 1.0f; // 10m scaled
            break;
        case MEDIUM_PARCEL:
            plot.requiredSetback = 0.7f; // 7m scaled
            break;
        case SMALL_PARCEL:
            plot.requiredSetback = 0.5f; // 5m scaled
            break;
        }

        // Reduce setback for smaller plots
        if (plot.area < 50.0f) { // 500m² scaled
            plot.requiredSetback *= 0.5f;
        }
    }

    // ================== ACCESSOR METHODS ==================

    const vector<Plot>& getAllPlots() const { return allPlots; }
    const vector<OpenSpace>& getAllOpenSpaces() const { return allOpenSpaces; }

    const SubdivisionResult* getSubdivisionResult(int parcelId) const {
        auto it = subdivisionResults.find(parcelId);
        return (it != subdivisionResults.end()) ? &it->second : nullptr;
    }

    vector<Plot> getPlotsByCategory(ParcelCategory category) const {
        vector<Plot> result;
        for (const auto& plot : allPlots) {
            if (plot.originalCategory == category) {
                result.push_back(plot);
            }
        }
        return result;
    }

    vector<Plot> getPlotsBySubdivisionLevel(int level) const {
        vector<Plot> result;
        for (const auto& plot : allPlots) {
            if (plot.subdivisionLevel == level) {
                result.push_back(plot);
            }
        }
        return result;
    }

    vector<Plot> getOpenSpacePlots() const {
        vector<Plot> result;
        for (const auto& plot : allPlots) {
            if (plot.isOpenSpace) {
                result.push_back(plot);
            }
        }
        return result;
    }

    // ================== VALIDATION & STATISTICS ==================

    bool validateDependencies() const {
        if (!siteGen) {
            cout << "Error: SiteGeneration not initialized" << endl;
            return false;
        }

        if (!parcelAnalyzer) {
            cout << "Error: ParcelAnalyzer not initialized" << endl;
            return false;
        }

        if (!roadAnalyzer) {
            cout << "Warning: RoadAnalyzer not available - using default directions" << endl;
        }

        return true;
    }

    void printSubdivisionStatistics() {
        cout << "\n=== SUBDIVISION STATISTICS ===" << endl;

        // Count by subdivision level
        map<int, int> levelCounts;
        map<ParcelCategory, int> categoryCounts;
        map<OpenSpaceType, int> openSpaceCounts;

        for (const auto& plot : allPlots) {
            levelCounts[plot.subdivisionLevel]++;
            categoryCounts[plot.originalCategory]++;

            if (plot.isOpenSpace) {
                openSpaceCounts[plot.openSpaceType]++;
            }
        }

        cout << "Plots by Subdivision Level:" << endl;
        for (const auto& pair : levelCounts) {
            cout << "  Level " << pair.first << ": " << pair.second << " plots" << endl;
        }

        cout << "Plots by Original Parcel Category:" << endl;
        for (const auto& pair : categoryCounts) {
            cout << "  " << getParcelCategoryName(pair.first) << ": " << pair.second << " plots" << endl;
        }

        cout << "Open Spaces by Type:" << endl;
        int totalOpenSpaces = 0;
        for (const auto& pair : openSpaceCounts) {
            cout << "  " << getOpenSpaceTypeName(pair.first) << ": " << pair.second << endl;
            totalOpenSpaces += pair.second;
        }
        cout << "  Total Open Spaces: " << totalOpenSpaces << endl;

        // Calculate total areas
        float totalPlotArea = 0;
        float totalBuildableArea = 0;
        float totalOpenSpaceArea = 0;

        for (const auto& plot : allPlots) {
            totalPlotArea += plot.area;

            if (plot.isOpenSpace) {
                totalOpenSpaceArea += plot.area;
            }
            else {
                totalBuildableArea += plot.buildableArea;
            }
        }

        cout << "Area Statistics:" << endl;
        cout << "  Total plot area: " << totalPlotArea << " unit²" << endl;
        cout << "  Total buildable area: " << totalBuildableArea << " unit²" << endl;
        cout << "  Total open space area: " << totalOpenSpaceArea << " unit²" << endl;

        if (totalPlotArea > 0) {
            cout << "  Buildable ratio: " << (totalBuildableArea / totalPlotArea) * 100 << "%" << endl;
            cout << "  Open space ratio: " << (totalOpenSpaceArea / totalPlotArea) * 100 << "%" << endl;
        }

        cout << "=================================" << endl;
    }

    // ================== VISUALIZATION METHODS ==================

    void drawSubdividedPlots() {
        drawPlotsBySubdivisionLevel();
        drawOpenSpaces();
        void drawOpenSpacesSimplified();
        drawSubdivisionAxes();
        drawRoadConnections();
    }

    void drawPlotsBySubdivisionLevel() {
        for (const auto& plot : allPlots) {
            // Skip open spaces - they'll be drawn separately
            if (plot.isOpenSpace) continue;

            // Color by subdivision level
            switch (plot.subdivisionLevel) {
            case 0: // No subdivision
                glColor4f(0.8f, 0.8f, 0.8f, 0.4f); // Light gray
                break;
            case 1: // Single division
                glColor4f(0.0f, 0.8f, 0.8f, 0.4f); // Cyan
                break;
            case 2: // Double division
                glColor4f(0.0f, 0.8f, 0.0f, 0.4f); // Green
                break;
            case 4: // Quadruple division
                glColor4f(0.8f, 0.0f, 0.8f, 0.4f); // Magenta
                break;
            default:
                glColor4f(1.0f, 1.0f, 0.0f, 0.4f); // Yellow
                break;
            }

            // Draw filled plot
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            if (plot.boundary.size() >= 3) {
                glBegin(GL_TRIANGLE_FAN);
                glVertex3f(plot.center.x, plot.center.y, 0.01f);
                for (const auto& pt : plot.boundary) {
                    glVertex3f(pt.x, pt.y, 0.01f);
                }
                glVertex3f(plot.boundary[0].x, plot.boundary[0].y, 0.01f);
                glEnd();
            }

            glDisable(GL_BLEND);

            // Draw plot boundary
            glColor3f(0.0f, 0.0f, 0.0f);
            glLineWidth(1.0f);
            glBegin(GL_LINE_LOOP);
            for (const auto& pt : plot.boundary) {
                glVertex3f(pt.x, pt.y, 0.02f);
            }
            glEnd();

            // Draw plot ID
            glRasterPos3f(plot.center.x - 2, plot.center.y - 1, 0.1f);
            string idStr = to_string(plot.id);
            for (char c : idStr) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, c);
            }
        }
    }

    void drawOpenSpaces() {
        for (const auto& plot : allPlots) {
            // Only draw open spaces
            if (!plot.isOpenSpace) continue;

            // Color by open space type
            switch (plot.openSpaceType) {
            case OpenSpaceType::CENTRAL_PARK:
                glColor4f(0.0f, 0.8f, 0.0f, 0.7f); // Dark green
                break;
            case OpenSpaceType::PLAZA:
                glColor4f(0.9f, 0.9f, 0.6f, 0.7f); // Light tan
                break;
            case OpenSpaceType::COURTYARD:
                glColor4f(0.6f, 0.8f, 1.0f, 0.7f); // Light blue
                break;
            case OpenSpaceType::LINEAR_PARK:
                glColor4f(0.4f, 0.8f, 0.4f, 0.7f); // Medium green
                break;
            case OpenSpaceType::POCKET_PARK:
                glColor4f(0.6f, 0.9f, 0.6f, 0.7f); // Light green
                break;
            case OpenSpaceType::GREEN_BUFFER:
                glColor4f(0.7f, 0.8f, 0.7f, 0.7f); // Gray-green
                break;
            default:
                glColor4f(0.8f, 0.8f, 0.8f, 0.7f); // Light gray
                break;
            }

            // Draw filled plot with higher opacity
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            if (plot.boundary.size() >= 3) {
                glBegin(GL_TRIANGLE_FAN);
                glVertex3f(plot.center.x, plot.center.y, 0.03f); // Higher z to be on top
                for (const auto& pt : plot.boundary) {
                    glVertex3f(pt.x, pt.y, 0.03f);
                }
                glVertex3f(plot.boundary[0].x, plot.boundary[0].y, 0.03f);
                glEnd();
            }

            glDisable(GL_BLEND);

            // Draw thicker boundary for open spaces
            glColor3f(0.0f, 0.5f, 0.0f); // Dark green outline
            glLineWidth(2.0f);
            glBegin(GL_LINE_LOOP);
            for (const auto& pt : plot.boundary) {
                glVertex3f(pt.x, pt.y, 0.04f);
            }
            glEnd();
            glLineWidth(1.0f);

            // Draw open space symbols based on type
            drawOpenSpaceSymbols(plot);
        }
    }

    void drawOpenSpaceSymbols(const Plot& plot) {
        switch (plot.openSpaceType) {
        case OpenSpaceType::CENTRAL_PARK:
        case OpenSpaceType::LINEAR_PARK:
        case OpenSpaceType::POCKET_PARK:
            // Draw tree symbols
            drawTreeSymbols(plot, 3 + plot.area / 20.0f); // Number of trees based on area
            break;

        case OpenSpaceType::PLAZA:
            // Draw plaza pattern
            drawPlazaPattern(plot);
            break;

        case OpenSpaceType::COURTYARD:
            // Draw courtyard with water feature
            drawCourtyardPattern(plot);
            break;

        case OpenSpaceType::GREEN_BUFFER:
            // Draw simple vegetation
            drawTreeSymbols(plot, 2); // Just a few trees
            break;

        default:
            break;
        }
    }

    void drawTreeSymbols(const Plot& plot, int numTrees) {
        // Generate pseudo-random tree positions within the plot
        std::mt19937 treeRng(plot.id); // Seed with plot ID for consistency
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (int i = 0; i < numTrees; i++) {
            // Generate random point inside the plot using rejection sampling
            Point2D treePos;
            bool validPos = false;
            int attempts = 0;

            while (!validPos && attempts < 10) {
                // Random position relative to plot center
                float radius = sqrt(dist(treeRng)) * sqrt(plot.area / M_PI) * 0.8f;
                float angle = dist(treeRng) * 2.0f * M_PI;

                treePos.x = plot.center.x + radius * cos(angle);
                treePos.y = plot.center.y + radius * sin(angle);

                // Check if inside plot (simplified)
                validPos = true; // Assume valid for simplicity
                attempts++;
            }

            // Draw tree
            float treeSize = 0.3f + dist(treeRng) * 0.3f; // Random size

            // Tree crown (circle)
            glColor3f(0.0f, 0.4f + dist(treeRng) * 0.2f, 0.0f); // Varying green
            drawCircle(treePos, treeSize, 8);

            // Tree trunk
            glColor3f(0.4f, 0.2f, 0.0f); // Brown
            glBegin(GL_LINES);
            glVertex3f(treePos.x, treePos.y, 0.04f);
            glVertex3f(treePos.x, treePos.y - treeSize, 0.04f);
            glEnd();
        }
    }

    void drawPlazaPattern(const Plot& plot) {
        // Draw grid pattern for plaza
        glColor3f(0.7f, 0.7f, 0.7f); // Gray for paving
        glLineWidth(0.5f);

        // Calculate grid spacing based on plot size
        float gridSize = sqrt(plot.area) / 5.0f;

        // Find bounding box
        float minX = plot.boundary[0].x, maxX = plot.boundary[0].x;
        float minY = plot.boundary[0].y, maxY = plot.boundary[0].y;

        for (const auto& pt : plot.boundary) {
            minX = min(minX, pt.x);
            maxX = max(maxX, pt.x);
            minY = min(minY, pt.y);
            maxY = max(maxY, pt.y);
        }

        // Draw horizontal grid lines
        for (float y = minY; y <= maxY; y += gridSize) {
            glBegin(GL_LINES);
            glVertex3f(minX, y, 0.035f);
            glVertex3f(maxX, y, 0.035f);
            glEnd();
        }

        // Draw vertical grid lines
        for (float x = minX; x <= maxX; x += gridSize) {
            glBegin(GL_LINES);
            glVertex3f(x, minY, 0.035f);
            glVertex3f(x, maxY, 0.035f);
            glEnd();
        }

        // Draw fountain in center
        glColor3f(0.6f, 0.8f, 1.0f); // Light blue for water
        drawCircle(plot.center, sqrt(plot.area) / 8.0f, 12);

        glLineWidth(1.0f);
    }

    void drawCourtyardPattern(const Plot& plot) {
        // Draw central water feature
        glColor3f(0.5f, 0.7f, 1.0f); // Blue for water
        drawCircle(plot.center, sqrt(plot.area) / 6.0f, 12);

        // Draw some seating around
        glColor3f(0.6f, 0.4f, 0.2f); // Brown for benches
        glLineWidth(1.5f);

        float radius = sqrt(plot.area) / 4.0f;

        // Draw 3-4 benches around the water feature
        int numBenches = 3 + (plot.area > 50.0f ? 1 : 0);

        for (int i = 0; i < numBenches; i++) {
            float angle = i * (2.0f * M_PI / numBenches);

            // Bench position
            Point2D benchPos;
            benchPos.x = plot.center.x + radius * cos(angle);
            benchPos.y = plot.center.y + radius * sin(angle);

            // Bench direction (perpendicular to radius)
            Point2D benchDir;
            benchDir.x = -sin(angle);
            benchDir.y = cos(angle);

            // Draw bench (simple line)
            float benchLength = sqrt(plot.area) / 8.0f;

            glBegin(GL_LINES);
            glVertex3f(benchPos.x - benchDir.x * benchLength,
                benchPos.y - benchDir.y * benchLength, 0.04f);
            glVertex3f(benchPos.x + benchDir.x * benchLength,
                benchPos.y + benchDir.y * benchLength, 0.04f);
            glEnd();
        }

        glLineWidth(1.0f);
    }

    void drawCircle(const Point2D& center, float radius, int segments) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(center.x, center.y, 0.04f); // Center

        for (int i = 0; i <= segments; i++) {
            float angle = 2.0f * M_PI * i / segments;
            glVertex3f(center.x + radius * cos(angle),
                center.y + radius * sin(angle), 0.04f);
        }

        glEnd();
    }

    void drawSubdivisionAxes() {
        for (const auto& plot : allPlots) {
            // Skip open spaces for clarity
            if (plot.isOpenSpace) continue;

            if (plot.subdivisionLevel > 0) {
                // Draw subdivision axis
                glColor3f(1.0f, 0.0f, 0.0f); // Red for subdivision axis
                glLineWidth(2.0f);

                Point2D axisStart = plot.center + plot.subdivisionAxis * (-5.0f);
                Point2D axisEnd = plot.center + plot.subdivisionAxis * 5.0f;

                glBegin(GL_LINES);
                glVertex3f(axisStart.x, axisStart.y, 0.05f);
                glVertex3f(axisEnd.x, axisEnd.y, 0.05f);
                glEnd();

                // Draw perpendicular direction if available
                if (plot.perpendicularDirection.magnitude() > 0.1f) {
                    glColor3f(0.0f, 1.0f, 0.0f); // Green for perpendicular

                    Point2D perpStart = plot.center + plot.perpendicularDirection * (-3.0f);
                    Point2D perpEnd = plot.center + plot.perpendicularDirection * 3.0f;

                    glBegin(GL_LINES);
                    glVertex3f(perpStart.x, perpStart.y, 0.05f);
                    glVertex3f(perpEnd.x, perpEnd.y, 0.05f);
                    glEnd();
                }
            }
        }
        glLineWidth(1.0f);
    }

    void drawRoadConnections() {
        for (const auto& plot : allPlots) {
            // Skip open spaces for clarity
            if (plot.isOpenSpace) continue;

            if (plot.hasMajorRoadFrontage) {
                // Draw connection to major road
                glColor3f(0.0f, 0.0f, 1.0f); // Blue for road connections
                glLineWidth(1.5f);

                Point2D roadEnd = plot.center + plot.primaryRoadDirection * 5.0f;

                glBegin(GL_LINES);
                glVertex3f(plot.center.x, plot.center.y, 0.1f);
                glVertex3f(roadEnd.x, roadEnd.y, 0.1f);
                glEnd();

                // Draw connection point
                glColor3f(1.0f, 0.0f, 0.0f);
                glPointSize(4.0f);
                glBegin(GL_POINTS);
                glVertex3f(roadEnd.x, roadEnd.y, 0.1f);
                glEnd();
                glPointSize(1.0f);
            }
        }
        glLineWidth(1.0f);
    }

    void drawBuildableAreas() {
        for (const auto& plot : allPlots) {
            // Skip open spaces
            if (plot.isOpenSpace) continue;

            if (plot.buildableArea > 0) {
                // Simplified visualization - draw smaller boundary representing buildable area
                float reductionFactor = sqrt(plot.buildableArea / plot.area);

                glColor4f(1.0f, 1.0f, 0.0f, 0.3f); // Yellow for buildable area
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                // Draw scaled-down polygon representing buildable area
                if (plot.boundary.size() >= 3) {
                    glBegin(GL_TRIANGLE_FAN);
                    glVertex3f(plot.center.x, plot.center.y, 0.03f);

                    for (const auto& pt : plot.boundary) {
                        Point2D scaledPt = plot.center + (pt - plot.center) * reductionFactor;
                        glVertex3f(scaledPt.x, scaledPt.y, 0.03f);
                    }

                    Point2D scaledFirstPt = plot.center + (plot.boundary[0] - plot.center) * reductionFactor;
                    glVertex3f(scaledFirstPt.x, scaledFirstPt.y, 0.03f);
                    glEnd();
                }

                glDisable(GL_BLEND);
            }
        }
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

    string getSubdivisionLevelName(SubdivisionLevel level) {
        switch (level) {
        case SubdivisionLevel::NO_SUBDIVISION: return "No Subdivision";
        case SubdivisionLevel::SINGLE_DIVISION: return "Single Division";
        case SubdivisionLevel::DOUBLE_DIVISION: return "Double Division";
        case SubdivisionLevel::QUADRUPLE_DIVISION: return "Quadruple Division";
        default: return "Unknown";
        }
    }

    string getOpenSpaceTypeName(OpenSpaceType type) {
        switch (type) {
        case OpenSpaceType::CENTRAL_PARK: return "Central Park";
        case OpenSpaceType::PLAZA: return "Plaza";
        case OpenSpaceType::COURTYARD: return "Courtyard";
        case OpenSpaceType::LINEAR_PARK: return "Linear Park";
        case OpenSpaceType::POCKET_PARK: return "Pocket Park";
        case OpenSpaceType::GREEN_BUFFER: return "Green Buffer";
        case OpenSpaceType::GREEN_ROOF: return "Green Roof";
        case OpenSpaceType::NONE: return "None";
        default: return "Unknown";
        }
    }

    // Clear all subdivision data
    void clearSubdivisions() {
        allPlots.clear();
        allOpenSpaces.clear();
        subdivisionResults.clear();
        nextPlotId = 0;
        nextOpenSpaceId = 0;
        cout << "Subdivision data cleared" << endl;
    }

    // ================== PARAMETER ADJUSTMENT ==================

    void setMinPlotArea(float area) {
        globalMinPlotArea = area;
        cout << "Minimum plot area set to: " << area << " unit²" << endl;
    }

    void updateCategoryParameters(ParcelCategory category, const SubdivisionParameters& params) {
        categoryParameters[category] = params;
        cout << "Updated subdivision parameters for " << getParcelCategoryName(category) << " parcels" << endl;
    }

    SubdivisionParameters getCategoryParameters(ParcelCategory category) const {
        auto it = categoryParameters.find(category);
        return (it != categoryParameters.end()) ? it->second : SubdivisionParameters();
    }

    // Set open space ratios for all categories
    void setOpenSpaceRatios(float largeRatio, float mediumRatio, float smallRatio) {
        categoryParameters[LARGE_PARCEL].openSpaceRatio = largeRatio;
        categoryParameters[MEDIUM_PARCEL].openSpaceRatio = mediumRatio;
        categoryParameters[SMALL_PARCEL].openSpaceRatio = smallRatio;

        cout << "Updated open space ratios:" << endl;
        cout << "  Large parcels: " << largeRatio * 100 << "%" << endl;
        cout << "  Medium parcels: " << mediumRatio * 100 << "%" << endl;
        cout << "  Small parcels: " << smallRatio * 100 << "%" << endl;
    }
    // Function to draw open spaces with simple green coloring
    void drawOpenSpacesSimplified() {
        // Draw fills for all open spaces
        for (size_t i = 0; i < allPlots.size(); ++i) {
            const Plot& plot = allPlots[i];

            // Only process open spaces
            if (!plot.isOpenSpace) continue;

            // Set different shades of green based on open space type
            float r = 0.0f, g = 0.0f, b = 0.0f;
            float alpha = 0.7f; // Consistent transparency

            switch (plot.openSpaceType) {
            case OpenSpaceType::CENTRAL_PARK:
                r = 0.0f; g = 0.7f; b = 0.0f; // Dark green
                break;
            case OpenSpaceType::PLAZA:
                r = 0.2f; g = 0.8f; b = 0.2f; // Light green
                break;
            case OpenSpaceType::COURTYARD:
                r = 0.3f; g = 0.6f; b = 0.3f; // Medium green
                break;
            case OpenSpaceType::LINEAR_PARK:
                r = 0.1f; g = 0.6f; b = 0.1f; // Forest green
                break;
            case OpenSpaceType::POCKET_PARK:
                r = 0.4f; g = 0.8f; b = 0.4f; // Mint green
                break;
            case OpenSpaceType::GREEN_BUFFER:
                r = 0.4f; g = 0.6f; b = 0.4f; // Gray-green
                break;
            case OpenSpaceType::GREEN_ROOF:
                r = 0.3f; g = 0.7f; b = 0.3f; // Standard green
                break;
            default:
                r = 0.2f; g = 0.7f; b = 0.2f; // Default green
                break;
            }

            // Draw filled plot with transparency
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glColor4f(r, g, b, alpha);

            if (plot.boundary.size() >= 3) {
                glBegin(GL_TRIANGLE_FAN);
                glVertex3f(plot.center.x, plot.center.y, 0.04f); // Higher z to be on top
                for (size_t j = 0; j < plot.boundary.size(); ++j) {
                    const Point2D& pt = plot.boundary[j];
                    glVertex3f(pt.x, pt.y, 0.04f);
                }
                glVertex3f(plot.boundary[0].x, plot.boundary[0].y, 0.04f);
                glEnd();
            }

            glDisable(GL_BLEND);

            // Draw boundary outline
            glColor3f(0.0f, 0.5f, 0.0f); // Dark green outline for all open spaces
            glLineWidth(1.5f);
            glBegin(GL_LINE_LOOP);
            for (size_t j = 0; j < plot.boundary.size(); ++j) {
                const Point2D& pt = plot.boundary[j];
                glVertex3f(pt.x, pt.y, 0.045f);
            }
            glEnd();
            glLineWidth(1.0f);
        }
    }
};

#endif // PARCEL_SUBDIVIDER_H