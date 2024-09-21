#include <GL/glew.h> // Make sure GLEW is included for OpenGL extensions
#include <GLFW/glfw3.h>
#include <iostream>

#include <optional>

#include <GLES3/gl3.h> // Include OpenGL headers for Emscripten
#include <emscripten.h>
#include <queue>
#include <string>

namespace {

// Structure to hold mouse event data
struct MouseEventRecord {
  std::string eventType;
  double clientX, clientY;
  double movementX, movementY;
  int button, buttons;
  bool ctrlKey, shiftKey, altKey, metaKey;
  double wheelDelta;
};

// Queue to hold the mouse events
std::queue<MouseEventRecord> mouseEventQueue;

GLuint textureID;
std::vector<unsigned char> pixelDataBuffer;
int canvasWidth = 640;
int canvasHeight = 480;

static const char *s_applicationName = "BabylonNative Playground";
GLuint quadVAO = 0;
GLuint quadVBO;

GLuint shaderProgram;
// Global state for mouse and keyboard events
double mouseX = 0, mouseY = 0;
bool leftButtonPressed = false;
bool middleButtonPressed = false;
bool rightButtonPressed = false;
bool ctrlPressed = false;
bool shiftPressed = false;
bool altPressed = false;
bool metaPressed = false;
double wheelDelta = 0;
bool inside = true;
void MouseEnterCallback(GLFWwindow *window, int entered) {
  if (!entered) {
    leftButtonPressed = false;
    middleButtonPressed = false;
    rightButtonPressed = false;
    inside = false;
  } else {
    inside = true;
  }
}

void MouseCallback(GLFWwindow *window, double xpos, double ypos) {
  double movementX = xpos - mouseX;
  double movementY = ypos - mouseY;
  mouseX = xpos;
  mouseY = ypos;

  if (!inside || !leftButtonPressed) {
    // return;
  }

  MouseEventRecord event = {
      "pointermove",
      xpos,
      ypos,
      movementX,
      movementY,
      -1, // No specific button for mousemove
      (leftButtonPressed ? 1 : 0) | (middleButtonPressed ? 2 : 0) |
          (rightButtonPressed ? 4 : 0),
      ctrlPressed,
      shiftPressed,
      altPressed,
      metaPressed,
      0 // No wheel delta for mousemove
  };
  mouseEventQueue.push(event);
}

void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
  // Update button press states based on the button and action
  if (!inside) {
    // return;
  }
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    leftButtonPressed = (action == GLFW_PRESS);
  } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
    middleButtonPressed = (action == GLFW_PRESS);
  } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    rightButtonPressed = (action == GLFW_PRESS);
  }

  // Determine the event type
  std::string eventType = (action == GLFW_PRESS) ? "pointerdown" : "pointerup";

  // Create the event record and push it to the queue
  MouseEventRecord event = {
      eventType,
      mouseX,
      mouseY,
      0,
      0,
      button,
      (leftButtonPressed ? 1 : 0) | (middleButtonPressed ? 2 : 0) |
          (rightButtonPressed ? 4 : 0),
      ctrlPressed,
      shiftPressed,
      altPressed,
      metaPressed,
      0 // No wheel delta for mouse button events
  };
  mouseEventQueue.push(event);
}

void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
  MouseEventRecord event = {
      "wheel",
      mouseX,
      mouseY,
      0,
      0,
      -1, // No button associated with scroll
      (leftButtonPressed ? 1 : 0) | (middleButtonPressed ? 2 : 0) |
          (rightButtonPressed ? 4 : 0),
      ctrlPressed,
      shiftPressed,
      altPressed,
      metaPressed,
      yoffset // Wheel delta
  };
  mouseEventQueue.push(event);
}

void InitGLTexture() {
  // Generate an OpenGL texture
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);

  // Set texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_2D, 0);

  // Allocate memory for pixel data (RGBA)
  pixelDataBuffer.resize(canvasWidth * canvasHeight * 4);
}

void UpdateTexture() {
  // Bind the texture and update it with new pixel data
  glBindTexture(GL_TEXTURE_2D, textureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, canvasWidth, canvasHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixelDataBuffer.data());
  glBindTexture(GL_TEXTURE_2D, 0);
}

static void window_resize_callback(GLFWwindow *window, int width, int height) {
  canvasWidth = width;
  canvasHeight = height;

  EM_ASM(
      {
        if (!Module.offscreenCanvas) {
          return;
        }
        Module.offscreenCanvas.width = $0;
        Module.offscreenCanvas.height = $1;
      },
      width, height);
}

// Function to register GLFW callbacks
void RegisterCallbacks(GLFWwindow *window) {
  glfwSetCursorPosCallback(window, MouseCallback); // Track mouse movement
  glfwSetMouseButtonCallback(window,
                             MouseButtonCallback); // Track mouse buttons
  glfwSetScrollCallback(window, ScrollCallback);   // Track mouse scroll (wheel)
  glfwSetWindowSizeCallback(window, window_resize_callback);
  glfwSetCursorEnterCallback(window, MouseEnterCallback);
}

GLFWwindow *init_glfw(int width, int height) {
  if (!glfwInit()) {
    std::cerr << "Failed to init GLFW" << std::endl;
    return 0;
  }

  GLFWwindow *window = glfwCreateWindow(width, height, "BabylonNative test app",
                                        nullptr, nullptr);

  if (!window) {
    std::cerr << "Failed to create window" << std::endl;
    return 0;
  }

  glfwMakeContextCurrent(window);

  int w, h;
  glfwGetFramebufferSize(window, &w, &h);
  glViewport(0, 0, w, h);

  RegisterCallbacks(window);

  return window;
}

EM_ASYNC_JS(void, init_babylon_js, (), {
  const moduleRootUrl = "./Scripts/";
  const scripts = [
    moduleRootUrl + "ammo.js", moduleRootUrl + "recast.js",
    moduleRootUrl + "babylon.max.js", moduleRootUrl + "babylonjs.loaders.js",
    moduleRootUrl + "babylonjs.materials.js", moduleRootUrl + "babylon.gui.js",
    moduleRootUrl + "game.js"
  ];
  async function loadScript(src) {
    return new Promise((resolve, reject) => {
      const script = document.createElement('script');
      script.src = src;
      script.onload = () => resolve();
      script.onerror = () => reject(new Error(`Failed to load script
                                               : $ { src }`));
      document.head.appendChild(script);
    });
  }
  (async() => {
    for (let script of scripts) {
      try {
        await loadScript(script);
      } catch (error) {
        console.error(error);
        break;
      }
    }
    console.log("All scripts loaded successfully");
  })();
});

// Compile the shaders and link the shader program
GLuint CompileShaders(const char *vertexSrc, const char *fragmentSrc) {
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexSrc, NULL);
  glCompileShader(vertexShader);

  // Check for vertex shader compilation errors
  int success;
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
    std::cerr << "Vertex Shader compilation error: " << infoLog << std::endl;
  }

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentSrc, NULL);
  glCompileShader(fragmentShader);

  // Check for fragment shader compilation errors
  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
    std::cerr << "Fragment Shader compilation error: " << infoLog << std::endl;
  }

  // Link the shaders into a program
  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);

  // Check for linking errors
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(program, 512, NULL, infoLog);
    std::cerr << "Shader Program linking error: " << infoLog << std::endl;
  }

  // Cleanup shaders as they are now linked into the program
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}

void InitShaderProgram() {
  const char *vertexShaderSrc = R"(#version 300 es
        layout (location = 0) in vec2 aPos;
        out vec2 vTexCoord;
        void main() {
            vTexCoord = aPos * 0.5 + 0.5; // Transform [-1, 1] to [0, 1]
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

  const char *fragmentShaderSrc = R"(#version 300 es
        precision highp float;
        in vec2 vTexCoord;
        out vec4 FragColor;
        uniform sampler2D uTexture;
        void main() {
            FragColor = texture(uTexture, vTexCoord);
        }
    )";

  shaderProgram = CompileShaders(vertexShaderSrc, fragmentShaderSrc);
}

void InitBabylon(int width, int height) {
  InitShaderProgram();
  InitGLTexture();
  init_babylon_js();
}

void SetupQuad() {
  if (quadVAO == 0) {
    // Define fullscreen quad vertices (two triangles)
    float quadVertices[] = {// positions
                            -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f};

    // Generate VAO and VBO
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices,
                 GL_STATIC_DRAW);

    // Position attribute (location = 0 in shader)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          (void *)0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
  }
}

void RenderFullscreenQuadWithTexture(GLuint textureID) {
  if (quadVAO == 0) {
    SetupQuad();
  }

  // Use the shader program
  glUseProgram(shaderProgram);

  // Bind the texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textureID);

  // Set the texture uniform (uTexture in the fragment shader)
  glUniform1i(glGetUniformLocation(shaderProgram, "uTexture"), 0);

  // Bind the VAO and draw the quad
  glBindVertexArray(quadVAO);
  glDrawArrays(GL_TRIANGLE_STRIP, 0,
               4); // Drawing two triangles as a triangle strip

  // Cleanup bindings
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void ProcessMouseEvents() {
  while (!mouseEventQueue.empty()) {
    MouseEventRecord event = mouseEventQueue.front();
    mouseEventQueue.pop();

    // Forward the event to the offscreen canvas using EM_ASM
    EM_ASM(
        {
          if (!Module.offscreenCanvas) {
            return;
          }

          let eventType = UTF8ToString($0);
          let t = eventType === "wheel" ? WheelEvent : PointerEvent;
          let mouseEvent = new t(eventType, {
            clientX : $1,
            clientY : $2,
            screenX : $1,
            screenY : $2,
            movementX : $10,
            movementY : $11,
            button : $3,
            buttons : $4,
            ctrlKey : $5,
            shiftKey : $6,
            altKey : $7,
            metaKey : $8,
            deltaY : -50 * $9, // Only for wheel events
            bubbles : true,
            cancelable : true,
            view : window,
          });

          Module.offscreenCanvas.dispatchEvent(mouseEvent);
        },
        event.eventType.c_str(), event.clientX, event.clientY, event.button,
        event.buttons, event.ctrlKey, event.shiftKey, event.altKey,
        event.metaKey, event.wheelDelta, event.movementX, event.movementY);
  }
}

void MainLoop(void *window) {
  glfwPollEvents();

  ProcessMouseEvents();

  // Call the Babylon.js function to transfer pixel data to C++ memory buffer
  int bufferSize = canvasWidth * canvasHeight * 4;
  EM_ASM(
      {
        if (Module.transferCanvasToCPP)
          Module.transferCanvasToCPP($0, $1);
      },
      pixelDataBuffer.data(), bufferSize);

  // Update the OpenGL texture with the new data
  UpdateTexture();

  // Clear the screen
  glClear(GL_COLOR_BUFFER_BIT);

  // Render the texture as a fullscreen quad (you should have a shader setup for
  // this)
  RenderFullscreenQuadWithTexture(textureID);

  glfwSwapBuffers(glfwGetCurrentContext());
}
} // namespace

int main(int argc, const char *const *argv) {
  GLFWwindow *window = init_glfw(canvasWidth, canvasHeight);
  glfwGetWindowSize(window, &canvasWidth, &canvasHeight);
  InitBabylon(canvasWidth, canvasHeight);
  emscripten_set_main_loop_arg(MainLoop, window, 0, 1);

  return 0;
}
