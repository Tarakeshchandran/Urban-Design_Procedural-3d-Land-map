import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { TransformControls } from 'three/examples/jsm/controls/TransformControls.js';
import { OBJExporter } from 'three/examples/jsm/exporters/OBJExporter.js';
import { VoxelType } from '../core/Building3DGenerator.js';

export class ThreeRenderer {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        if (!this.container) return;

        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0xf0f0f0);
        this.technicalViewEnabled = false;
        this.renderedViewEnabled = false;
        this.hasAutoFramed = false;

        this.camera = new THREE.PerspectiveCamera(45, this.container.clientWidth / this.container.clientHeight, 0.01, 1000);
        this.camera.position.set(0, 100, 150);
        this.camera.lookAt(0, 0, 0);

        this.renderer = new THREE.WebGLRenderer({ antialias: true });
        this.renderer.setSize(this.container.clientWidth, this.container.clientHeight);
        this.renderer.outputColorSpace = THREE.SRGBColorSpace;
        this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
        this.renderer.toneMappingExposure = 1.0;
        this.renderer.shadowMap.enabled = true;
        this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
        this.container.appendChild(this.renderer.domElement);

        this.controls = new OrbitControls(this.camera, this.renderer.domElement);
        this.worldUnitMeters = 10.0;
        this.firstPersonEnabled = false;
        this.eyeHeight = this.metersToWorldUnits(1.5);
        this.moveSpeedMeters = 9.0;
        this.fpYaw = 0;
        this.fpPitch = 0;
        this.mouseLookActive = false;
        this.lastMouseX = 0;
        this.lastMouseY = 0;
        this.mouseLookSensitivity = 0.003;
        this.navigatorPose = null;
        this.movementState = {
            up: false,
            down: false,
            left: false,
            right: false
        };
        this.clock = new THREE.Clock();
        this.raycaster = new THREE.Raycaster();
        this.pointerNdc = new THREE.Vector2();
        this.selectableBuildingMeshes = [];
        this.selectablePlotMeshes = [];
        this.selectableMeshes = [];
        this.footprintMeshMap = new Map();
        this.plotMeshMap = new Map();
        this.onSelectionChange = null;
        this.materialCache = new Map();
        this.currentSelectedFootprintId = null;
        this.currentSelectedPlotId = null;
        this.selectionBuildingMaterial = new THREE.MeshStandardMaterial({
            color: 0xff8f2a,
            roughness: 0.48,
            metalness: 0.02
        });
        
        // Lighting
        this.ambientLight = new THREE.AmbientLight(0xffffff, 0.55);
        this.scene.add(this.ambientLight);

        this.dirLight = new THREE.DirectionalLight(0xffffff, 1.0);
        this.dirLight.position.set(140, 220, 120);
        this.dirLight.castShadow = true;
        this.dirLight.shadow.mapSize.set(4096, 4096);
        this.dirLight.shadow.radius = 3;
        this.dirLight.shadow.bias = -0.00012;
        this.dirLight.shadow.normalBias = 0.02;
        this.dirLight.shadow.camera.near = 5;
        this.dirLight.shadow.camera.far = 1200;
        this.dirLight.shadow.camera.left = -450;
        this.dirLight.shadow.camera.right = 450;
        this.dirLight.shadow.camera.top = 450;
        this.dirLight.shadow.camera.bottom = -450;
        this.dirLight.shadow.camera.updateProjectionMatrix();
        this.scene.add(this.dirLight);

        this.skyLight = new THREE.HemisphereLight(0xbfcfff, 0x2a2a2a, 0.65);
        this.scene.add(this.skyLight);

        this.rimLight = new THREE.DirectionalLight(0xdde4ff, 0.35);
        this.rimLight.position.set(-120, 180, -160);
        this.scene.add(this.rimLight);

        this.groundPlane = new THREE.Mesh(
            new THREE.PlaneGeometry(6000, 6000),
            new THREE.MeshStandardMaterial({
                color: 0x5a5a5a,
                roughness: 0.96,
                metalness: 0.0
            })
        );
        this.groundPlane.rotation.x = -Math.PI / 2;
        this.groundPlane.position.y = -0.03;
        this.groundPlane.receiveShadow = true;
        this.scene.add(this.groundPlane);

        // Grid
        this.gridHelper = new THREE.GridHelper(400, 40, 0x000000, 0x000000);
        this.gridHelper.material.opacity = 0.1;
        this.gridHelper.material.transparent = true;
        this.scene.add(this.gridHelper);

        this.buildingGroup = new THREE.Group();
        this.scene.add(this.buildingGroup);

        this.debugGroup = new THREE.Group();
        this.scene.add(this.debugGroup);

        this.siteLayersGroup = new THREE.Group();
        this.scene.add(this.siteLayersGroup);

        this.attractorsGroup = new THREE.Group();
        this.scene.add(this.attractorsGroup);
        
        this.transformControls = [];
        this.attractorMeta = [];

        this.materials = {
            [VoxelType.VISIT]: new THREE.MeshLambertMaterial({ color: 0x800080 }), // Purple (Retail/Podium)
            [VoxelType.WORK]: new THREE.MeshLambertMaterial({ color: 0x4169e1 }),  // Blue (Office)
            [VoxelType.LIVE]: new THREE.MeshLambertMaterial({ color: 0xffd700 }),  // Yellow (Residential)
            [VoxelType.TRANSITION]: new THREE.MeshLambertMaterial({ color: 0x888888 }), // Grey (Transition/Amenity)
            DEFAULT: new THREE.MeshLambertMaterial({ color: 0xaaaaaa })
        };

        this.edgeMaterials = {
            'STREET_EDGE': new THREE.MeshLambertMaterial({ color: 0x808080 }),
            'WATER_EDGE': new THREE.MeshLambertMaterial({ color: 0x00ffff }),
            'PARK_EDGE': new THREE.MeshLambertMaterial({ color: 0x00ff00 }),
            'INTERNAL_EDGE': new THREE.MeshLambertMaterial({ color: 0xcccccc }),
            'MIXED_EDGE': new THREE.MeshLambertMaterial({ color: 0xff00ff })
        };
        this.technicalViewBuildingMaterial = new THREE.MeshStandardMaterial({
            color: 0xc9ced6,
            roughness: 0.72,
            metalness: 0.04
        });
        this.technicalViewEdgeMaterial = new THREE.LineBasicMaterial({
            color: 0xffffff,
            transparent: true,
            opacity: 0.75
        });
        this.renderedViewBuildingMaterial = new THREE.MeshStandardMaterial({
            color: 0xf7f7f7,
            roughness: 0.9,
            metalness: 0.0
        });

        window.addEventListener('resize', this.onWindowResize.bind(this));
        window.addEventListener('keydown', this.onKeyDown.bind(this));
        window.addEventListener('keyup', this.onKeyUp.bind(this));
        this.renderer.domElement.addEventListener('mousedown', this.onMouseDown.bind(this));
        this.renderer.domElement.addEventListener('mousemove', this.onMouseMove.bind(this));
        this.renderer.domElement.addEventListener('mouseup', this.onMouseUp.bind(this));
        this.renderer.domElement.addEventListener('mouseleave', this.onMouseUp.bind(this));
        this.renderer.domElement.addEventListener('click', this.onSceneClick.bind(this));
        
        this.applyVisualMode();
        this.animate();
    }

    onWindowResize() {
        if (!this.container) return;
        this.camera.aspect = this.container.clientWidth / this.container.clientHeight;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(this.container.clientWidth, this.container.clientHeight);
    }

    metersToWorldUnits(meters) {
        return meters / Math.max(this.worldUnitMeters, 0.001);
    }

    animate() {
        requestAnimationFrame(this.animate.bind(this));
        const delta = this.clock.getDelta();
        if (this.firstPersonEnabled) {
            this.updateFirstPersonMovement(delta);
        } else {
            this.controls.update();
        }
        this.renderer.render(this.scene, this.camera);
    }

    onKeyDown(event) {
        if (!this.firstPersonEnabled) return;
        if (event.key === 'ArrowUp') this.movementState.up = true;
        if (event.key === 'ArrowDown') this.movementState.down = true;
        if (event.key === 'ArrowLeft') this.movementState.left = true;
        if (event.key === 'ArrowRight') this.movementState.right = true;
        if (event.key.startsWith('Arrow')) event.preventDefault();
    }

    onKeyUp(event) {
        if (event.key === 'ArrowUp') this.movementState.up = false;
        if (event.key === 'ArrowDown') this.movementState.down = false;
        if (event.key === 'ArrowLeft') this.movementState.left = false;
        if (event.key === 'ArrowRight') this.movementState.right = false;
    }

    onMouseDown(event) {
        if (!this.firstPersonEnabled) return;
        if (event.button !== 0) return;
        this.mouseLookActive = true;
        this.lastMouseX = event.clientX;
        this.lastMouseY = event.clientY;
        event.preventDefault();
    }

    onMouseMove(event) {
        if (!this.firstPersonEnabled || !this.mouseLookActive) return;
        const dx = event.clientX - this.lastMouseX;
        const dy = event.clientY - this.lastMouseY;
        this.lastMouseX = event.clientX;
        this.lastMouseY = event.clientY;

        this.fpYaw -= dx * this.mouseLookSensitivity;
        this.fpPitch -= dy * this.mouseLookSensitivity;
        this.fpPitch = Math.max(-1.2, Math.min(1.2, this.fpPitch));
    }

    onMouseUp() {
        this.mouseLookActive = false;
    }

    updateFirstPersonMovement(delta) {
        const forwardFlat = new THREE.Vector3(Math.sin(this.fpYaw), 0, Math.cos(this.fpYaw)).normalize();
        const rightFlat = new THREE.Vector3(forwardFlat.z, 0, -forwardFlat.x).normalize();
        const walkDistance = this.metersToWorldUnits(this.moveSpeedMeters) * delta;

        if (this.movementState.up) {
            this.camera.position.addScaledVector(forwardFlat, walkDistance);
        }
        if (this.movementState.down) {
            this.camera.position.addScaledVector(forwardFlat, -walkDistance);
        }
        if (this.movementState.left) {
            this.camera.position.addScaledVector(rightFlat, -walkDistance);
        }
        if (this.movementState.right) {
            this.camera.position.addScaledVector(rightFlat, walkDistance);
        }

        this.camera.position.y = this.eyeHeight;
        const lookDir = new THREE.Vector3(
            Math.sin(this.fpYaw) * Math.cos(this.fpPitch),
            Math.sin(this.fpPitch),
            Math.cos(this.fpYaw) * Math.cos(this.fpPitch)
        ).normalize();
        const lookTarget = this.camera.position.clone().add(
            lookDir.multiplyScalar(this.metersToWorldUnits(4.0))
        );
        this.camera.lookAt(lookTarget);
        this.navigatorPose = {
            x: this.camera.position.x,
            y: this.camera.position.z,
            yaw: this.fpYaw,
            active: true
        };
    }

    setFirstPersonMode(enabled) {
        this.firstPersonEnabled = !!enabled;
        this.controls.enabled = !this.firstPersonEnabled;
        this.movementState.up = false;
        this.movementState.down = false;
        this.movementState.left = false;
        this.movementState.right = false;

        if (this.firstPersonEnabled) {
            const spawn = this.controls?.target ? this.controls.target.clone() : new THREE.Vector3(0, 0, 0);
            this.camera.position.set(spawn.x, this.eyeHeight, spawn.z + this.metersToWorldUnits(3.0));
            this.fpYaw = Math.PI;
            this.fpPitch = 0;
            const lookDir = new THREE.Vector3(
                Math.sin(this.fpYaw),
                0,
                Math.cos(this.fpYaw)
            ).normalize();
            this.camera.lookAt(this.camera.position.clone().addScaledVector(lookDir, this.metersToWorldUnits(4.0)));
            this.navigatorPose = { x: this.camera.position.x, y: this.camera.position.z, yaw: this.fpYaw, active: true };
        } else {
            this.mouseLookActive = false;
            this.navigatorPose = null;
        }
    }

    applyVisualMode() {
        if (this.technicalViewEnabled) {
            this.scene.background = new THREE.Color(0x000000);
            this.ambientLight.intensity = 0.24;
            this.dirLight.intensity = 1.15;
            this.skyLight.intensity = 0.38;
            this.rimLight.intensity = 0.15;
            this.groundPlane.material.color.setHex(0x4a4a4a);
            this.groundPlane.visible = true;
            this.gridHelper.visible = true;
            this.renderer.shadowMap.enabled = true;
            this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
            this.renderer.toneMappingExposure = 1.05;
            return;
        }

        if (this.renderedViewEnabled) {
            this.scene.background = new THREE.Color(0x000000);
            this.ambientLight.intensity = 0.2;
            this.dirLight.intensity = 1.35;
            this.skyLight.intensity = 0.78;
            this.rimLight.intensity = 0.28;
            this.groundPlane.material.color.setHex(0x606060);
            this.groundPlane.visible = true;
            this.gridHelper.visible = false;
            this.renderer.shadowMap.enabled = true;
            this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
            this.renderer.toneMappingExposure = 1.0;
            return;
        }

        this.scene.background = new THREE.Color(0xf0f0f0);
        this.ambientLight.intensity = 0.55;
        this.dirLight.intensity = 1.0;
        this.skyLight.intensity = 0.52;
        this.rimLight.intensity = 0.22;
        this.groundPlane.material.color.setHex(0xe8e8e8);
        this.groundPlane.visible = true;
        this.gridHelper.visible = true;
        this.renderer.toneMappingExposure = 1.0;
    }

    setTechnicalView(enabled) {
        this.technicalViewEnabled = !!enabled;
        if (this.technicalViewEnabled) this.renderedViewEnabled = false;
        this.applyVisualMode();
    }

    setRenderedView(enabled) {
        this.renderedViewEnabled = !!enabled;
        if (this.renderedViewEnabled) this.technicalViewEnabled = false;
        this.applyVisualMode();
    }

    setRenderMode(enabled) {
        // Backward compatibility with existing caller paths.
        this.setTechnicalView(enabled);
    }

    requestAutoFrame() {
        this.hasAutoFramed = false;
    }

    getViewModes() {
        return {
            technical: this.technicalViewEnabled,
            rendered: this.renderedViewEnabled
        };
    }

    setViewModes({ technical = false, rendered = false } = {}) {
        this.technicalViewEnabled = !!technical;
        this.renderedViewEnabled = !!rendered;
        if (this.technicalViewEnabled && this.renderedViewEnabled) {
            this.renderedViewEnabled = false;
        }
        this.applyVisualMode();
    }

    jumpFirstPersonTo(x, y) {
        if (!Number.isFinite(x) || !Number.isFinite(y)) return;
        if (!this.firstPersonEnabled) {
            // Store as orbit target so entering FP starts near this location.
            this.controls.target.set(x, 0, y);
            return;
        }
        this.camera.position.set(x, this.eyeHeight, y);
        const lookDir = new THREE.Vector3(
            Math.sin(this.fpYaw) * Math.cos(this.fpPitch),
            Math.sin(this.fpPitch),
            Math.cos(this.fpYaw) * Math.cos(this.fpPitch)
        ).normalize();
        this.camera.lookAt(this.camera.position.clone().addScaledVector(lookDir, this.metersToWorldUnits(4.0)));
        this.navigatorPose = { x: this.camera.position.x, y: this.camera.position.z, yaw: this.fpYaw, active: true };
    }

    getNavigatorPose() {
        return this.navigatorPose ? { ...this.navigatorPose } : null;
    }

    setSelectionChangeCallback(callback) {
        this.onSelectionChange = callback;
    }

    getCachedMaterial(key, createFn) {
        if (!this.materialCache.has(key)) {
            this.materialCache.set(key, createFn());
        }
        return this.materialCache.get(key);
    }

    refreshSelectableTargets() {
        this.selectableMeshes = [
            ...(this.selectableBuildingMeshes || []),
            ...(this.selectablePlotMeshes || [])
        ];
    }

    onSceneClick(event) {
        if (this.firstPersonEnabled) return;
        if (!this.selectableMeshes || this.selectableMeshes.length === 0) return;

        const rect = this.renderer.domElement.getBoundingClientRect();
        if (rect.width <= 0 || rect.height <= 0) return;
        this.pointerNdc.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
        this.pointerNdc.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;

        this.raycaster.setFromCamera(this.pointerNdc, this.camera);
        const hits = this.raycaster.intersectObjects(this.selectableMeshes, false);
        if (!hits || hits.length === 0) {
            if (this.onSelectionChange) this.onSelectionChange(null);
            return;
        }

        const hit = hits[0].object;
        const footprintId = Number.isFinite(hit.userData?.footprintId) ? hit.userData.footprintId : null;
        const plotId = Number.isFinite(hit.userData?.plotId) ? hit.userData.plotId : null;
        if (this.onSelectionChange) {
            this.onSelectionChange({ footprintId, plotId });
        }
    }

    focusOnSelection({ footprintId = null, plotId = null } = {}) {
        if (this.firstPersonEnabled) return false;
        let meshes = [];

        if (Number.isFinite(footprintId) && this.footprintMeshMap.has(footprintId)) {
            meshes = this.footprintMeshMap.get(footprintId);
        } else if (Number.isFinite(plotId) && this.plotMeshMap.has(plotId)) {
            meshes = this.plotMeshMap.get(plotId);
        }
        if (!meshes || meshes.length === 0) return false;

        const box = new THREE.Box3();
        for (const mesh of meshes) {
            box.expandByObject(mesh);
        }
        if (!Number.isFinite(box.min.x) || !Number.isFinite(box.max.x)) return false;

        const center = new THREE.Vector3();
        const size = new THREE.Vector3();
        box.getCenter(center);
        box.getSize(size);
        const radius = Math.max(size.x, size.y, size.z, 2.0);

        const viewDir = this.camera.position.clone().sub(this.controls.target);
        if (viewDir.lengthSq() < 1e-8) {
            viewDir.set(1, 0.7, 1);
        }
        viewDir.normalize();
        const targetDistance = Math.max(radius * 2.8, this.metersToWorldUnits(30.0));

        this.controls.target.copy(center);
        this.camera.position.copy(center.clone().addScaledVector(viewDir, targetDistance));
        this.controls.update();
        return true;
    }

    setSelectionHighlight({ footprintId = null, plotId = null } = {}) {
        this.currentSelectedFootprintId = Number.isFinite(footprintId) ? footprintId : null;
        this.currentSelectedPlotId = Number.isFinite(plotId) ? plotId : null;

        for (const mesh of this.selectableBuildingMeshes || []) {
            const isSelected = this.currentSelectedFootprintId !== null
                && mesh.userData?.footprintId === this.currentSelectedFootprintId;
            if (isSelected) {
                mesh.material = this.selectionBuildingMaterial;
            } else if (mesh.userData?.defaultMaterial) {
                mesh.material = mesh.userData.defaultMaterial;
            }
        }

        for (const mesh of this.selectablePlotMeshes || []) {
            const baseColor = mesh.userData?.baseColor ?? 0xcccccc;
            const baseOpacity = mesh.userData?.baseOpacity ?? 0.5;
            const isSelected = this.currentSelectedPlotId !== null
                && mesh.userData?.plotId === this.currentSelectedPlotId;
            mesh.material.color.setHex(isSelected ? 0xff8f2a : baseColor);
            mesh.material.opacity = isSelected ? 0.92 : baseOpacity;
            mesh.material.transparent = mesh.material.opacity < 0.999;
        }
    }

    setupAttractors(initialPoints, onChangeCallback) {
        // Clear existing
        while (this.attractorsGroup.children.length > 0) {
            const mesh = this.attractorsGroup.children[0];
            this.attractorsGroup.remove(mesh);
            mesh.geometry.dispose();
            mesh.material.dispose();
        }
        for (const tc of this.transformControls) {
            tc.detach();
            tc.dispose();
            this.scene.remove(tc.getHelper());
        }
        this.transformControls = [];
        this.attractorMeshes = [];
        this.attractorMeta = [];

        for (let i = 0; i < initialPoints.length; i++) {
            const pt = initialPoints[i];
            const isPrimary = !!pt.isPrimary;
            const weight = typeof pt.weight === 'number' ? pt.weight : (isPrimary ? 1.0 : 0.6);
            const geo = new THREE.SphereGeometry(3, 16, 16);
            const mat = new THREE.MeshLambertMaterial({ color: isPrimary ? 0xff0000 : 0x1f6dff });
            const mesh = new THREE.Mesh(geo, mat);
            mesh.position.set(pt.x, 0, pt.y);
            this.attractorsGroup.add(mesh);
            this.attractorMeshes.push(mesh);
            this.attractorMeta.push({
                isPrimary,
                weight
            });

            const tc = new TransformControls(this.camera, this.renderer.domElement);
            tc.attach(mesh);
            tc.setMode('translate');
            tc.showY = false; // Disable Y-axis translation
            this.scene.add(tc.getHelper());
            this.transformControls.push(tc);

            // Disable orbit controls while dragging
            tc.addEventListener('dragging-changed', (event) => {
                this.controls.enabled = !event.value;
                if (!event.value) { // Finished dragging
                    if (onChangeCallback) {
                        const newPoints = this.attractorMeshes.map((m, idx) => ({
                            x: m.position.x,
                            y: m.position.z,
                            isPrimary: this.attractorMeta[idx]?.isPrimary || false,
                            weight: this.attractorMeta[idx]?.weight ?? 0.6
                        }));
                        onChangeCallback(newPoints);
                    }
                }
            });
            console.log('TransformControls created and attached to mesh', mesh.position, tc);
        }
    }

    toggleAttractorsVisibility(visible) {
        this.attractorsGroup.visible = visible;
        for (const tc of this.transformControls) {
            tc.getHelper().visible = visible;
            tc.enabled = visible;
        }
    }

    clear() {
        while (this.buildingGroup.children.length > 0) {
            const mesh = this.buildingGroup.children[0];
            this.buildingGroup.remove(mesh);
            if (mesh.geometry) mesh.geometry.dispose();
        }
        while (this.debugGroup.children.length > 0) {
            const mesh = this.debugGroup.children[0];
            this.debugGroup.remove(mesh);
            if (mesh.geometry) mesh.geometry.dispose();
        }
        this.selectableBuildingMeshes = [];
        this.footprintMeshMap = new Map();
        this.currentSelectedFootprintId = null;
        this.currentSelectedPlotId = null;
        this.refreshSelectableTargets();
    }

    renderBuildings(generator, options = {}) {
        this.clear();
        if (!generator || !generator.building3DVoxels) return;
        const thematicBuildingColors = options.thematicBuildingColorsByFootprintId || null;
        const thematicOpacity = Number.isFinite(options.thematicOpacity)
            ? Math.max(0.08, Math.min(1.0, options.thematicOpacity))
            : 0.94;
        const programOpacity = Number.isFinite(options.programOpacity)
            ? Math.max(0.08, Math.min(1.0, options.programOpacity))
            : 0.95;
        const selectedFootprintId = Number.isFinite(options.selectedFootprintId) ? options.selectedFootprintId : null;
        const footprints = generator?.footprintGenerator?.buildingFootprints || [];
        const footprintById = new Map(footprints.map((f) => [f.id, f]));

        let centerSum = new THREE.Vector3(0, 0, 0);
        let minBound = new THREE.Vector3(Infinity, Infinity, Infinity);
        let maxBound = new THREE.Vector3(-Infinity, -Infinity, -Infinity);
        let count = 0;

        for (const voxel of generator.building3DVoxels) {
            let geometry;
            let isExtrusion = false;

            if (voxel.shapePolygon && voxel.shapePolygon.length >= 3) {
                const shape = new THREE.Shape();
                shape.moveTo(voxel.shapePolygon[0].x, -voxel.shapePolygon[0].y);
                for (let i = 1; i < voxel.shapePolygon.length; i++) {
                    shape.lineTo(voxel.shapePolygon[i].x, -voxel.shapePolygon[i].y);
                }
                shape.lineTo(voxel.shapePolygon[0].x, -voxel.shapePolygon[0].y);

                geometry = new THREE.ExtrudeGeometry(shape, {
                    depth: voxel.dimensions.z,
                    bevelEnabled: false
                });
                geometry.rotateX(-Math.PI / 2);
                isExtrusion = true;
            } else {
                geometry = new THREE.BoxGeometry(voxel.dimensions.x, voxel.dimensions.z, voxel.dimensions.y);
            }
            
            let material = this.materials[voxel.type] || this.materials.DEFAULT;
            const parentFootprint = footprintById.get(voxel.parentFootprintId) || null;

            const technical = this.technicalViewEnabled || options.technicalView;
            const rendered = this.renderedViewEnabled || options.renderedView;
            if (selectedFootprintId !== null && voxel.parentFootprintId === selectedFootprintId) {
                material = this.selectionBuildingMaterial;
            } else if (options.programColoring || options.showBuildingsByProgram) {
                const baseMat = this.materials[voxel.type] || this.materials.DEFAULT;
                const colorHex = baseMat.color?.getHex?.() ?? 0xaaaaaa;
                const key = `program-${colorHex}-${programOpacity.toFixed(2)}`;
                material = this.getCachedMaterial(key, () => new THREE.MeshStandardMaterial({
                    color: colorHex,
                    roughness: 0.48,
                    metalness: 0.03,
                    transparent: programOpacity < 0.999,
                    opacity: programOpacity
                }));
            } else if (thematicBuildingColors && Object.prototype.hasOwnProperty.call(thematicBuildingColors, voxel.parentFootprintId)) {
                const colorHex = thematicBuildingColors[voxel.parentFootprintId];
                const key = `theme-${colorHex}`;
                material = this.getCachedMaterial(key, () => new THREE.MeshStandardMaterial({
                    color: colorHex,
                    roughness: 0.58,
                    metalness: 0.04,
                    transparent: true,
                    opacity: thematicOpacity
                }));
            } else if (technical) {
                material = this.technicalViewBuildingMaterial;
            } else if (rendered) {
                material = this.renderedViewBuildingMaterial;
            } else if (options.showBuildingsByProgram) {
                material = this.materials[voxel.type] || this.materials.DEFAULT;
            } else if (options.showFootprintsByEdge) {
                if (parentFootprint) {
                    material = this.edgeMaterials[parentFootprint.primaryEdgeType] || this.edgeMaterials['INTERNAL_EDGE'];
                }
            }

            const mesh = new THREE.Mesh(geometry, material);
            mesh.castShadow = true;
            mesh.receiveShadow = true;
            mesh.userData = {
                selectable: true,
                footprintId: voxel.parentFootprintId,
                plotId: parentFootprint?.parentPlotId ?? null,
                defaultMaterial: material
            };
            
            if (isExtrusion) {
                mesh.position.set(0, voxel.position.z, 0);
                // The geometry is already in world coordinates, so we don't rotate the mesh
                // Calculate centroid for bounding box updates
                let cx = 0, cy = 0;
                for (const pt of voxel.shapePolygon) { cx += pt.x; cy += pt.y; }
                cx /= voxel.shapePolygon.length; cy /= voxel.shapePolygon.length;
                
                const centerPt = new THREE.Vector3(cx, voxel.position.z + voxel.dimensions.z / 2, cy);
                centerSum.add(centerPt);
                minBound.min(centerPt);
                maxBound.max(centerPt);
            } else {
                mesh.position.set(
                    voxel.position.x + voxel.dimensions.x / 2,
                    voxel.position.z + voxel.dimensions.z / 2,
                    voxel.position.y + voxel.dimensions.y / 2
                );
                mesh.rotation.y = -voxel.rotationAngle;
                
                centerSum.add(mesh.position);
                minBound.min(mesh.position);
                maxBound.max(mesh.position);
            }
            
            count++;

            this.buildingGroup.add(mesh);
            this.selectableBuildingMeshes.push(mesh);

            if (!this.footprintMeshMap.has(voxel.parentFootprintId)) {
                this.footprintMeshMap.set(voxel.parentFootprintId, []);
            }
            this.footprintMeshMap.get(voxel.parentFootprintId).push(mesh);

            if (parentFootprint?.parentPlotId !== undefined && parentFootprint?.parentPlotId !== null) {
                if (!this.plotMeshMap.has(parentFootprint.parentPlotId)) {
                    this.plotMeshMap.set(parentFootprint.parentPlotId, []);
                }
                this.plotMeshMap.get(parentFootprint.parentPlotId).push(mesh);
            }

            if (technical) {
                const edgesGeometry = new THREE.EdgesGeometry(geometry, 35);
                const edges = new THREE.LineSegments(edgesGeometry, this.technicalViewEdgeMaterial);
                edges.position.copy(mesh.position);
                edges.rotation.copy(mesh.rotation);
                this.buildingGroup.add(edges);
            }

            if (options.showOrientationDebug && voxel.level === 0 && !isExtrusion) {
                const dir = new THREE.Vector3(voxel.orientation.x, 0, voxel.orientation.y).normalize();
                const origin = new THREE.Vector3(mesh.position.x, mesh.position.y + voxel.dimensions.z / 2 + 0.5, mesh.position.z);
                const arrowHelper = new THREE.ArrowHelper(dir, origin, 3, 0xff0000);
                this.debugGroup.add(arrowHelper);
            }
        }

        this.refreshSelectableTargets();
        this.setSelectionHighlight({
            footprintId: selectedFootprintId,
            plotId: Number.isFinite(options.selectedPlotId) ? options.selectedPlotId : this.currentSelectedPlotId
        });

        if (count > 0 && !this.firstPersonEnabled && !this.hasAutoFramed) {
            centerSum.divideScalar(count);
            this.controls.target.copy(centerSum);
            
            const size = maxBound.distanceTo(minBound);
            this.camera.position.set(centerSum.x + size * 0.5, centerSum.y + size * 0.5, centerSum.z + size * 0.5);
            this.controls.update();
            this.hasAutoFramed = true;
        }
    }

    exportToOBJ() {
        if (!this.buildingGroup) return null;
        const exporter = new OBJExporter();
        const result = exporter.parse(this.buildingGroup);
        return result;
    }

    exportHighResPNG(scaleFactor = 3) {
        if (!this.renderer || !this.camera || !this.container) return null;
        const sf = Math.max(1, Math.floor(scaleFactor));

        const originalWidth = this.container.clientWidth;
        const originalHeight = this.container.clientHeight;
        const targetWidth = Math.max(1, originalWidth * sf);
        const targetHeight = Math.max(1, originalHeight * sf);

        this.renderer.setSize(targetWidth, targetHeight, false);
        this.camera.aspect = targetWidth / targetHeight;
        this.camera.updateProjectionMatrix();
        this.renderer.render(this.scene, this.camera);
        const dataUrl = this.renderer.domElement.toDataURL('image/png');

        this.renderer.setSize(originalWidth, originalHeight, false);
        this.camera.aspect = originalWidth / originalHeight;
        this.camera.updateProjectionMatrix();

        return dataUrl;
    }

    renderSiteLayers(siteGen, parcelSubdivider, options = {}) {
        // Clear old 2d layers
        while (this.siteLayersGroup.children.length > 0) {
            const child = this.siteLayersGroup.children[0];
            this.siteLayersGroup.remove(child);
            if (child.geometry) child.geometry.dispose();
            if (child.material) child.material.dispose();
        }
        this.selectablePlotMeshes = [];
        this.plotMeshMap = new Map();

        const drawCurve = (curve, color, lineWidth, yOffset, opacity = 1.0) => {
            if (!curve || curve.points.length < 2) return;
            const points = [];
            for (const pt of curve.points) {
                points.push(new THREE.Vector3(pt.x, yOffset, pt.y));
            }
            if (curve.isClosed && points.length > 0) {
                points.push(new THREE.Vector3(curve.points[0].x, yOffset, curve.points[0].y));
            }
            const geometry = new THREE.BufferGeometry().setFromPoints(points);
            const material = new THREE.LineBasicMaterial({
                color: color,
                linewidth: lineWidth,
                transparent: opacity < 0.999,
                opacity
            });
            const line = new THREE.Line(geometry, material);
            this.siteLayersGroup.add(line);
        };

        const drawPolygon = (points2d, colorHex, opacity, yOffset, metadata = null) => {
            if (!points2d || points2d.length < 3) return;
            const shape = new THREE.Shape();
            shape.moveTo(points2d[0].x, points2d[0].y);
            for (let i = 1; i < points2d.length; i++) {
                shape.lineTo(points2d[i].x, points2d[i].y);
            }
            shape.lineTo(points2d[0].x, points2d[0].y);

            const geometry = new THREE.ShapeGeometry(shape);
            const material = new THREE.MeshBasicMaterial({ 
                color: colorHex, 
                transparent: true, 
                opacity: opacity,
                side: THREE.DoubleSide
            });
            const mesh = new THREE.Mesh(geometry, material);
            mesh.rotation.x = Math.PI / 2;
            mesh.position.y = yOffset;
            if (metadata && Number.isFinite(metadata.plotId)) {
                mesh.userData = {
                    selectable: true,
                    plotId: metadata.plotId,
                    footprintId: null,
                    baseColor: colorHex,
                    baseOpacity: opacity
                };
                this.selectablePlotMeshes.push(mesh);
                if (!this.plotMeshMap.has(metadata.plotId)) {
                    this.plotMeshMap.set(metadata.plotId, []);
                }
                this.plotMeshMap.get(metadata.plotId).push(mesh);
            }
            this.siteLayersGroup.add(mesh);

            // Also draw outline
            const edges = new THREE.EdgesGeometry(geometry);
            const lineMat = new THREE.LineBasicMaterial({ color: metadata?.outlineColor ?? 0x333333 });
            const outline = new THREE.LineSegments(edges, lineMat);
            outline.rotation.x = Math.PI / 2;
            outline.position.y = yOffset + 0.01;
            this.siteLayersGroup.add(outline);
        };

        const clientBlueViz = !!options.clientBlueViz;
        const lineOpacity = clientBlueViz ? 0.88 : 1.0;
        const palette = clientBlueViz
            ? {
                major: 0x2fdddd,
                local: 0x1fa8a8,
                water: 0x27d6d6,
                park: 0x25bcb8,
                site: 0x1b8f95,
                tram: 0x30e8e8,
                rail: 0x2bcfd0,
                forest: 0x1da39f
            }
            : {
                major: 0xFF0000,
                local: 0x808080,
                water: 0x00B2FF,
                park: 0x00CC00,
                site: 0x000000,
                tram: 0xFF00FF,
                rail: 0x000000,
                forest: 0x228B22
            };

        if (options.showSite && siteGen) {
            for (const road of siteGen.getMajorRoads()) drawCurve(road, palette.major, 2, 0.05, lineOpacity);
            for (const road of siteGen.getLocalRoads()) drawCurve(road, palette.local, 1, 0.05, lineOpacity * 0.9);
            for (const water of siteGen.getWaterBoundaries()) drawCurve(water, palette.water, 2, 0.05, lineOpacity);
            for (const park of siteGen.getParkBoundaries()) drawCurve(park, palette.park, 2, 0.05, lineOpacity);
            for (const site of siteGen.getSiteBoundaries()) drawCurve(site, palette.site, 2, 0.05, lineOpacity * 0.95);
            for (const tram of siteGen.getTramLines()) drawCurve(tram, palette.tram, 2, 0.05, lineOpacity);
            for (const rail of siteGen.getRailways()) drawCurve(rail, palette.rail, 2, 0.05, lineOpacity * 0.95);
            for (const forest of siteGen.getForestBoundaries()) drawCurve(forest, palette.forest, 2, 0.05, lineOpacity);
        }

        if (options.showSubdividedPlots && parcelSubdivider) {
            const thematicPlotColors = options.thematicPlotColorsByPlotId || null;
            const selectedPlotId = Number.isFinite(options.selectedPlotId) ? options.selectedPlotId : null;
            const plotOpacity = Number.isFinite(options.plotOpacity)
                ? Math.max(0.08, Math.min(1.0, options.plotOpacity))
                : (clientBlueViz ? 0.34 : 0.58);
            for (const plot of parcelSubdivider.allPlots) {
                if (plot.isOpenSpace) continue;
                const isSelected = selectedPlotId !== null && plot.id === selectedPlotId;
                const themedColor = thematicPlotColors && Object.prototype.hasOwnProperty.call(thematicPlotColors, plot.id)
                    ? thematicPlotColors[plot.id]
                    : (clientBlueViz ? 0x0f8f95 : 0xcccccc);
                const fillColor = isSelected ? 0xff8f2a : themedColor;
                const fillOpacity = isSelected ? 0.92 : plotOpacity;
                const outlineColor = isSelected ? 0xffe1b5 : (clientBlueViz ? 0x2adede : 0x222222);
                drawPolygon(plot.boundary, fillColor, fillOpacity, 0.01, {
                    plotId: plot.id,
                    outlineColor
                });
            }
            for (const space of parcelSubdivider.allOpenSpaces) {
                drawPolygon(space.boundary, clientBlueViz ? 0x14777d : 0x00cc00, clientBlueViz ? 0.26 : 0.5, 0.02);
            }
        }
        this.refreshSelectableTargets();
    }
}
