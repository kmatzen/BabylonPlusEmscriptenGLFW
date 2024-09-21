/// <reference path="../node_modules/babylonjs/babylon.module.d.ts" />
/// <reference path="../node_modules/babylonjs-loaders/babylonjs.loaders.module.d.ts" />
/// <reference path="../node_modules/babylonjs-materials/babylonjs.materials.module.d.ts" />
/// <reference path="../node_modules/babylonjs-gui/babylon.gui.module.d.ts" />

const offscreenCanvas = document.createElement("canvas");
offscreenCanvas.width = 640;
offscreenCanvas.height = 480;

Module.offscreenCanvas = offscreenCanvas;
const gl = offscreenCanvas.getContext("webgl2", { preserveDrawingBuffer: true }); // Preserve buffer to allow reading
const engine = new BABYLON.Engine(gl, true, {
    premultipliedAlpha: false, // Disable premultiplied alpha to allow correct compositing
});

function transferCanvasToCPP(bufferPointer, bufferSize) {
    let width = offscreenCanvas.width;
    let height = offscreenCanvas.height;

    // Read pixels from the WebGL context (RGBA format)
    let pixelData = new Uint8Array(width * height * 4);
    gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixelData);

    // Copy the pixel data to the shared memory buffer (passed by C++)
    Module.HEAPU8.set(pixelData, bufferPointer);
}
Module.transferCanvasToCPP = transferCanvasToCPP;

var scene = new BABYLON.Scene(engine);

engine.runRenderLoop(function () {
    scene.render();
});

//-------------------- YOUR CODE GOES HERE ------------------------------
scene.createDefaultCamera(true, true, true);
var camera = scene.activeCamera;
camera.setTarget(BABYLON.Vector3.Zero());
camera.position = new BABYLON.Vector3(0, 5, -10);

// This creates a light, aiming 0,1,0 - to the sky (non-mesh)
var light = new BABYLON.HemisphericLight("light", new BABYLON.Vector3(0, 1, 0), scene);

// Default intensity is 1. Let's dim the light a small amount
light.intensity = 0.7;

// Our built-in 'sphere' shape.
var sphere = BABYLON.MeshBuilder.CreateSphere("sphere", { diameter: 2, segments: 32 }, scene);
sphere.material = new BABYLON.StandardMaterial("myMaterial", scene);

// Move the sphere upward 1/2 its height
sphere.position.y = 1;

// Our built-in 'ground' shape.
var ground = BABYLON.MeshBuilder.CreateGround("ground", { width: 6, height: 6 }, scene);

function MoveUp()
{
    sphere.position.y += 0.05;
}

function ChangeBallSize(size)
{
    sphere.scaling = new BABYLON.Vector3(size, size, size);
}

function ChangeBallColor(r, g, b, a)
{
    sphere.material.diffuseColor = new BABYLON.Color3(r, g, b);
}

function SetBallVisible(visible)
{
    sphere.isVisible = visible;
}

function SetFloorVisible(visible)
{
    ground.isVisible = visible;
}

