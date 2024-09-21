#include <GLES3/gl3.h>
#include <GLFW/glfw3.h>
#include <emscripten.h>
#include <iostream>

namespace {

struct InputEventRecord {
  std::string eventType;
  int keyCode;
  double clientX, clientY;
  double movementX, movementY;
  int button, buttons;
  bool ctrlKey, shiftKey, altKey, metaKey;
  double wheelDelta;
};

std::queue<InputEventRecord> inputEventQueue;

struct InputState {
  double mouseX, mouseY;
  bool leftButtonPressed, middleButtonPressed, rightButtonPressed;
  bool ctrlPressed, shiftPressed, altPressed, metaPressed;
  double wheelDelta;
  bool insideWindow;
  bool keys[GLFW_KEY_LAST] = {false};
};

InputState inputState;

void MouseEnterCallback(GLFWwindow *window, int entered) {
  inputState.insideWindow = entered;
  if (!entered) {
    InputEventRecord mouseEvent = {
        "mouseout",
        -1,
        inputState.mouseX,
        inputState.mouseY,
        0,
        0,
        -1,
        (inputState.leftButtonPressed ? 1 : 0) |
            (inputState.middleButtonPressed ? 2 : 0) |
            (inputState.rightButtonPressed ? 4 : 0),
        inputState.ctrlPressed,
        inputState.shiftPressed,
        inputState.altPressed,
        inputState.metaPressed,
        0};
    inputEventQueue.push(mouseEvent);

    InputEventRecord pointerEvent = {
        "pointerout",
        -1,
        inputState.mouseX,
        inputState.mouseY,
        0,
        0,
        -1,
        (inputState.leftButtonPressed ? 1 : 0) |
            (inputState.middleButtonPressed ? 2 : 0) |
            (inputState.rightButtonPressed ? 4 : 0),
        inputState.ctrlPressed,
        inputState.shiftPressed,
        inputState.altPressed,
        inputState.metaPressed,
        0};
    inputEventQueue.push(pointerEvent);
  }
}

void MouseCallback(GLFWwindow *window, double xpos, double ypos) {
  double movementX = xpos - inputState.mouseX;
  double movementY = ypos - inputState.mouseY;
  inputState.mouseX = xpos;
  inputState.mouseY = ypos;

  InputEventRecord mouseEvent = {"mousemove",
                                 -1,
                                 xpos,
                                 ypos,
                                 movementX,
                                 movementY,
                                 -1,
                                 (inputState.leftButtonPressed ? 1 : 0) |
                                     (inputState.middleButtonPressed ? 2 : 0) |
                                     (inputState.rightButtonPressed ? 4 : 0),
                                 inputState.ctrlPressed,
                                 inputState.shiftPressed,
                                 inputState.altPressed,
                                 inputState.metaPressed,
                                 0};
  inputEventQueue.push(mouseEvent);

  InputEventRecord pointerEvent = {
      "pointermove",
      -1,
      xpos,
      ypos,
      movementX,
      movementY,
      -1,
      (inputState.leftButtonPressed ? 1 : 0) |
          (inputState.middleButtonPressed ? 2 : 0) |
          (inputState.rightButtonPressed ? 4 : 0),
      inputState.ctrlPressed,
      inputState.shiftPressed,
      inputState.altPressed,
      inputState.metaPressed,
      0};
  inputEventQueue.push(pointerEvent);
}

void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    inputState.leftButtonPressed = (action == GLFW_PRESS);
  } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
    inputState.middleButtonPressed = (action == GLFW_PRESS);
  } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    inputState.rightButtonPressed = (action == GLFW_PRESS);
  }

  std::string eventType = (action == GLFW_PRESS) ? "down" : "up";

  InputEventRecord mouseEvent = {"mouse" + eventType,
                                 -1,
                                 inputState.mouseX,
                                 inputState.mouseY,
                                 0,
                                 0,
                                 button,
                                 (inputState.leftButtonPressed ? 1 : 0) |
                                     (inputState.middleButtonPressed ? 2 : 0) |
                                     (inputState.rightButtonPressed ? 4 : 0),
                                 inputState.ctrlPressed,
                                 inputState.shiftPressed,
                                 inputState.altPressed,
                                 inputState.metaPressed,
                                 0};
  inputEventQueue.push(mouseEvent);

  InputEventRecord pointerEvent = {
      "pointer" + eventType,
      -1,
      inputState.mouseX,
      inputState.mouseY,
      0,
      0,
      button,
      (inputState.leftButtonPressed ? 1 : 0) |
          (inputState.middleButtonPressed ? 2 : 0) |
          (inputState.rightButtonPressed ? 4 : 0),
      inputState.ctrlPressed,
      inputState.shiftPressed,
      inputState.altPressed,
      inputState.metaPressed,
      0};
  inputEventQueue.push(pointerEvent);
}

void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
  inputState.wheelDelta = yoffset;

  InputEventRecord event = {"wheel",
                            -1,
                            inputState.mouseX,
                            inputState.mouseY,
                            0,
                            0,
                            -1,
                            (inputState.leftButtonPressed ? 1 : 0) |
                                (inputState.middleButtonPressed ? 2 : 0) |
                                (inputState.rightButtonPressed ? 4 : 0),
                            inputState.ctrlPressed,
                            inputState.shiftPressed,
                            inputState.altPressed,
                            inputState.metaPressed,
                            yoffset};
  inputEventQueue.push(event);
}

void KeyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mods) {
  if (key >= 0 && key < GLFW_KEY_LAST) {
    inputState.keys[key] = (action == GLFW_PRESS || action == GLFW_REPEAT);
  }

  std::string eventType =
      (action == GLFW_PRESS || action == GLFW_REPEAT) ? "keydown" : "keyup";

  InputEventRecord event = {eventType,
                            key,
                            inputState.mouseX,
                            inputState.mouseY,
                            0,
                            0,
                            -1,
                            0,
                            inputState.ctrlPressed,
                            inputState.shiftPressed,
                            inputState.altPressed,
                            inputState.metaPressed,
                            0};
  inputEventQueue.push(event);
}

void ProcessInputEvents() {
  while (!inputEventQueue.empty()) {
    InputEventRecord event = inputEventQueue.front();
    inputEventQueue.pop();

    EM_ASM(
        {
          if (!Module.offscreenCanvas)
            return;

          let eventType = UTF8ToString($0);
          let t = (eventType === "wheel")
                      ? WheelEvent
                      : (eventType.startsWith("mouse") ? MouseEvent
                                                       : KeyboardEvent);
          let inputEvent = new t(eventType, {
            clientX : $1,
            clientY : $2,
            movementX : $3,
            movementY : $4,
            button : $5,
            buttons : $6,
            ctrlKey : $7,
            shiftKey : $8,
            altKey : $9,
            metaKey : $10,
            deltaY : -50 * $11,
            keyCode : $12,
            bubbles : true,
            cancelable : true,
            view : window
          });

          Module.offscreenCanvas.dispatchEvent(inputEvent);
        },
        event.eventType.c_str(), event.clientX, event.clientY, event.movementX,
        event.movementY, event.button, event.buttons, event.ctrlKey,
        event.shiftKey, event.altKey, event.metaKey, event.wheelDelta,
        event.keyCode);
  }
}

class QuadRenderer {
public:
  QuadRenderer() {
    InitShaderProgram();
    SetupQuad();
  }

  ~QuadRenderer() {
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteProgram(shaderProgram);
  }

  void Render(GLuint textureID) {
    glUseProgram(shaderProgram);

    // Bind the texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set the texture uniform
    glUniform1i(glGetUniformLocation(shaderProgram, "uTexture"), 0);

    // Bind the VAO and draw the quad
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Cleanup bindings
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

private:
  GLuint quadVAO = 0;
  GLuint quadVBO;
  GLuint shaderProgram;

  void SetupQuad() {
    if (quadVAO == 0) {
      float quadVertices[] = {-1.0f, 1.0f, -1.0f, -1.0f,
                              1.0f,  1.0f, 1.0f,  -1.0f};

      glGenVertexArrays(1, &quadVAO);
      glGenBuffers(1, &quadVBO);

      glBindVertexArray(quadVAO);
      glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
      glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices,
                   GL_STATIC_DRAW);

      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                            (void *)0);

      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindVertexArray(0);
    }
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

  GLuint CompileShaders(const char *vertexSrc, const char *fragmentSrc) {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSrc, NULL);
    glCompileShader(vertexShader);

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

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
      std::cerr << "Fragment Shader compilation error: " << infoLog
                << std::endl;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetProgramInfoLog(program, 512, NULL, infoLog);
      std::cerr << "Shader Program linking error: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
  }
};

class TextureHandler {
public:
  TextureHandler(int width, int height)
      : canvasWidth(width), canvasHeight(height) {
    InitGLTexture();
  }

  ~TextureHandler() { glDeleteTextures(1, &textureID); }

  void UpdateTexture(const std::vector<unsigned char> &pixelData) {
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, canvasWidth, canvasHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixelData.data());
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  GLuint GetTextureID() const { return textureID; }

private:
  GLuint textureID;
  int canvasWidth, canvasHeight;

  void InitGLTexture() {
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
  }
};

std::unique_ptr<TextureHandler> textureHandler;
std::unique_ptr<QuadRenderer> quadRenderer;

int canvasWidth = 640;
int canvasHeight = 480;

static const char *s_applicationName = "BabylonNative Playground";

void WindowResizeCallback(GLFWwindow *window, int width, int height) {
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

  textureHandler = std::make_unique<TextureHandler>(canvasWidth, canvasHeight);
}

void RegisterCallbacks(GLFWwindow *window) {
  glfwSetCursorPosCallback(window, MouseCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetScrollCallback(window, ScrollCallback);
  glfwSetKeyCallback(window, KeyCallback);
  glfwSetCursorEnterCallback(window, MouseEnterCallback);
  glfwSetWindowSizeCallback(window, WindowResizeCallback);
}

GLFWwindow *InitGLFW(int width, int height) {
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

void InitOffscreenCanvas() {
  EM_ASM(
      {
        Module.offscreenCanvas = document.createElement("canvas");
        Module.gl = Module.offscreenCanvas.getContext(
            "webgl2",
            {preserveDrawingBuffer : true}); // Preserve buffer to allow reading
        Module.offscreenCanvas.width = $0;
        Module.offscreenCanvas.height = $1;
      },
      canvasWidth, canvasHeight);
}

void InitBabylon() {
  InitOffscreenCanvas();
  init_babylon_js();
}

void MainLoop(void *window) {
  glfwPollEvents();

  ProcessInputEvents();

  // Call the Babylon.js function to transfer pixel data to C++ memory buffer
  int bufferSize = canvasWidth * canvasHeight * 4;
  std::vector<unsigned char> pixelDataBuffer(bufferSize);
  EM_ASM(
      {
        if (!Module.offscreenCanvas)
          return;
        let width = Module.offscreenCanvas.width;
        let height = Module.offscreenCanvas.height;

        // Read pixels from the WebGL context (RGBA format)
        let pixelData = new Uint8Array(width * height * 4);
        Module.gl.readPixels(0, 0, width, height, Module.gl.RGBA,
                             Module.gl.UNSIGNED_BYTE, pixelData);

        // Copy the pixel data to the shared memory buffer (passed by C++)
        Module.HEAPU8.set(pixelData, $0);
      },
      pixelDataBuffer.data());

  // Update texture with pixel data
  textureHandler->UpdateTexture(pixelDataBuffer);

  glClear(GL_COLOR_BUFFER_BIT);

  quadRenderer->Render(textureHandler->GetTextureID());

  glfwSwapBuffers(glfwGetCurrentContext());
}

} // namespace

int main(int argc, const char *const *argv) {
  GLFWwindow *window = InitGLFW(canvasWidth, canvasHeight);
  glfwGetWindowSize(window, &canvasWidth, &canvasHeight);
  textureHandler = std::make_unique<TextureHandler>(canvasWidth, canvasHeight);
  quadRenderer = std::make_unique<QuadRenderer>();
  InitBabylon();
  emscripten_set_main_loop_arg(MainLoop, window, 0, 1);

  return 0;
}
