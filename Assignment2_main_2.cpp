#include <algorithm>
#include <iostream>
#include <vector>

#define GLEW_STATIC 1  // This allows linking with Static Library on Windows, without DLL
#include <GL/glew.h>   // Include GLEW - OpenGL Extension Wrangler
#include <GLFW/glfw3.h>  // GLFW provides a cross-platform interface for creating a graphical context,
// initializing OpenGL and binding inputs

#include <glm/glm.hpp>  // GLM is an optimized math library with syntax to similar to OpenGL Shading Language
#include <glm/gtc/matrix_transform.hpp>  // include this to create transformation matrices
#include <glm/gtc/type_ptr.hpp>

#include "OBJloader.h"    //For loading .obj files
#include "OBJloaderV3.h"  //For loading .obj files using a polygon list format
#include "shaderloader.h"
#include "Airplane.h"

using namespace glm;
using namespace std;

// window dimensions
const GLuint WIDTH = 1024, HEIGHT = 768;

vec3 computeCameraLookAt(double &lastMousePosX, double &lastMousePosY, float dt);

GLuint setupModelVBO(string path, int& vertexCount);

// Sets up a model using an Element Buffer Object to refer to vertex data
GLuint setupModelEBO(string path, int& vertexCount);

// shader variable setters
void SetUniformMat4(GLuint shader_id, const char* uniform_name,
                    mat4 uniform_value) {
  glUseProgram(shader_id);
  glUniformMatrix4fv(glGetUniformLocation(shader_id, uniform_name), 1, GL_FALSE,
                     &uniform_value[0][0]);
}

void SetUniformVec3(GLuint shader_id, const char* uniform_name,
                    vec3 uniform_value) {
  glUseProgram(shader_id);
  glUniform3fv(glGetUniformLocation(shader_id, uniform_name), 1,
               value_ptr(uniform_value));
}

template <class T>
void SetUniform1Value(GLuint shader_id, const char* uniform_name,
                      T uniform_value) {
  glUseProgram(shader_id);
  glUniform1i(glGetUniformLocation(shader_id, uniform_name), uniform_value);
  glUseProgram(0);
}

GLFWwindow* window = nullptr;
bool InitContext();



struct PlaneMeshes {
  GLuint planeVAO;  int planeVertices;
  GLuint propVAO;   int propVertices;
};

// shared base model for plane + prop using plane state.
// T(pos) * Y(yaw) * Br(roll) * Fix(model axis adjut) * scale
static inline glm::mat4 BuildPlaneBaseModel(const Airplane& p) {
  const mat4 T   = translate(mat4(1.f), p.position());
  const mat4 Y   = p.velocityYawMatrix();
  const mat4 Br  = rotate(mat4(1.f), radians(-p.bankRollDeg()), vec3(0,0,1));
  const mat4 Fix = rotate(mat4(1.f), radians(-90.f), vec3(1,0,0));
  const mat4 S   = scale(mat4(1.f), vec3(0.2f));
  return T * Y * Br * Fix * S;
}

static inline void DrawPlaneShadowOnly(const Airplane& p,
                                       const PlaneMeshes& mesh,
                                       GLuint shaderShadow,
                                       const glm::mat4& lightProjView,
                                       float propSpinDeg)
{
  const mat4 base = BuildPlaneBaseModel(p);

  const mat4 planeModel = base;
  const mat4 propModel  = base *
      translate(mat4(1.f), vec3(0.f, -15.0f, 0.8f)) *
      rotate(mat4(1.f), radians(propSpinDeg * 50.f), vec3(0,1,0)) *
      scale(mat4(1.f), vec3(1.3f));

  // plane
  SetUniformMat4(shaderShadow, "transform_in_light_space", lightProjView * planeModel);
  glBindVertexArray(mesh.planeVAO);
  glDrawArrays(GL_TRIANGLES, 0, mesh.planeVertices);
  glBindVertexArray(0);

  // prop
  SetUniformMat4(shaderShadow, "transform_in_light_space", lightProjView * propModel);
  glBindVertexArray(mesh.propVAO);
  glDrawArrays(GL_TRIANGLES, 0, mesh.propVertices);
  glBindVertexArray(0);
}

static inline void DrawPlaneSceneOnly(const Airplane& p,
                                      const PlaneMeshes& mesh,
                                      GLuint shaderScene,
                                      float propSpinDeg)
{
  const mat4 base = BuildPlaneBaseModel(p);

  const mat4 planeModel = base;
  const mat4 propModel  = base *
      translate(mat4(1.f), vec3(0.f, -15.0f, 0.8f)) *
      rotate(mat4(1.f), radians(propSpinDeg * 50.f), vec3(0,1,0)) *
      scale(mat4(1.f), vec3(1.3f));

  // plane
  SetUniformMat4(shaderScene, "model_matrix", planeModel);
  glBindVertexArray(mesh.planeVAO);
  glDrawArrays(GL_TRIANGLES, 0, mesh.planeVertices);
  glBindVertexArray(0);

  // prop
  SetUniformMat4(shaderScene, "model_matrix", propModel);
  glBindVertexArray(mesh.propVAO);
  glDrawArrays(GL_TRIANGLES, 0, mesh.propVertices);
  glBindVertexArray(0);
}

int main(int argc, char* argv[]) {
  if (!InitContext()) return -1;

    // sky color blue
  glClearColor(0.2f, 0.35f, 0.7f, 1.0f);

  std::string shaderPathPrefix = "Shaders/";

  GLuint shaderScene = loadSHADER(shaderPathPrefix + "scene_vertex.glsl",
                                  shaderPathPrefix + "scene_fragment.glsl");

  GLuint shaderShadow = loadSHADER(shaderPathPrefix + "shadow_vertex.glsl",
                                   shaderPathPrefix + "shadow_fragment.glsl");

  string planePath = "Models/airplane2.obj";
  string propPath = "Models/propeller3.obj";

  int planeVertices;
  int propVertices;
  GLuint planeVAO = setupModelVBO(planePath, planeVertices);
  GLuint propVAO  = setupModelVBO(propPath, propVertices);

  PlaneMeshes meshes{ planeVAO, planeVertices, propVAO, propVertices };


  std::vector<Airplane> planes;
  planes.emplace_back(glm::vec3( 0.f, 20.f, -30.f));
  planes.emplace_back(glm::vec3(8.f, 25.f, -30.f));
  planes.emplace_back(glm::vec3(-8.f, 22.f, -30.f));

  // texture and framebuffer for shadow map

  const unsigned int DEPTH_MAP_TEXTURE_SIZE = 1024;

  // index to texture used for shadow mapping
  GLuint depth_map_texture;
  glGenTextures(1, &depth_map_texture);
  glBindTexture(GL_TEXTURE_2D, depth_map_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, DEPTH_MAP_TEXTURE_SIZE,
               DEPTH_MAP_TEXTURE_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  // index to framebuffer used for shadow mapping
  GLuint depth_map_fbo;
  glGenFramebuffers(1, &depth_map_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, depth_map_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         depth_map_texture, 0);
  glDrawBuffer(GL_NONE);  // disable rendering colors, only write depth values


  vec3 cameraPosition(0.6f, 1.0f, 2.0f);
  vec3 cameraLookAt(0.0f, 0.0f, -1.0f);
  vec3 cameraUp(0.0f, 1.0f, 0.0f);


  float cameraSpeed = 1.0f;
  float cameraFastSpeed = 3 * cameraSpeed;


  float spinningAngle = 0.0f;


  mat4 projectionMatrix =
      glm::perspective(radians(70.0f),
                       WIDTH * 1.0f / HEIGHT,
                       0.01f, 800.0f);


  mat4 viewMatrix = lookAt(cameraPosition,                 // eye
                           cameraPosition + cameraLookAt,  // center
                           cameraUp);                      // up


  SetUniformMat4(shaderScene, "projection_matrix", projectionMatrix);


  SetUniformMat4(shaderScene, "view_matrix", viewMatrix);

  float lightAngleOuter = radians(30.0f);
  float lightAngleInner = radians(20.0f);
  
  SetUniform1Value(shaderScene, "light_cutoff_inner", cos(lightAngleInner));
  SetUniform1Value(shaderScene, "light_cutoff_outer", cos(lightAngleOuter));

  
  SetUniformVec3(shaderScene, "light_color", vec3(1));

  
  SetUniformVec3(shaderScene, "object_color", vec3(1));

  
  float lastFrameTime = glfwGetTime();
  int lastMouseLeftState = GLFW_RELEASE;
  double lastMousePosX, lastMousePosY;
  glfwGetCursorPos(window, &lastMousePosX, &lastMousePosY);

  
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);


  while (!glfwWindowShouldClose(window)) {

    float dt = glfwGetTime() - lastFrameTime;
    lastFrameTime = glfwGetTime();

    // light parameters
    float phi = glfwGetTime() * 0.5f * 3.14f;
    vec3 lightPosition = vec3(0.6f, 50.0f, 5.0f);
    vec3 lightFocus(0, -1, -1);  // light aiming direction
    vec3 lightDirection = normalize(lightFocus - lightPosition);

    float lightNearPlane = 0.01f;
    float lightFarPlane = 400.0f;

    mat4 lightProjMatrix =
      perspective(50.0f, (float)DEPTH_MAP_TEXTURE_SIZE /(float)DEPTH_MAP_TEXTURE_SIZE, lightNearPlane, lightFarPlane);
    mat4 lightViewMatrix = lookAt(lightPosition, lightFocus, vec3(0, 1, 0));

    SetUniformMat4(shaderScene, "light_proj_view_matrix", lightProjMatrix * lightViewMatrix);
    SetUniform1Value(shaderScene, "light_near_plane", lightNearPlane);
    SetUniform1Value(shaderScene, "light_far_plane", lightFarPlane);
    SetUniformVec3(shaderScene, "light_position", lightPosition);
    SetUniformVec3(shaderScene, "light_direction", lightDirection);

    // Propeller spin driver
    spinningAngle += 45.0f * dt;  // degrees per second

    // Update all planes
    for (auto& p : planes) p.update(dt);

    // Set the view matrix for first person camera and send to scene shader
    mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraLookAt, cameraUp);
    SetUniformMat4(shaderScene, "view_matrix", viewMatrix);

    // Precompute L*V
    mat4 lightProjView = lightProjMatrix * lightViewMatrix;

    // ---------- 1) SHADOW PASS: clear once, draw all planes ----------
    glUseProgram(shaderShadow);
    glViewport(0, 0, DEPTH_MAP_TEXTURE_SIZE, DEPTH_MAP_TEXTURE_SIZE);
    glBindFramebuffer(GL_FRAMEBUFFER, depth_map_fbo);
    glClear(GL_DEPTH_BUFFER_BIT);

    for (const auto& p : planes) {
      if (!p.isAlive()) continue;
      DrawPlaneShadowOnly(p, meshes, shaderShadow, lightProjView, spinningAngle);
    }

    // ---------- 2) SCENE PASS: clear once, draw all planes ----------
    glUseProgram(shaderScene);
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.2f, 0.35f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& p : planes) {
      if (!p.isAlive()) continue;
      DrawPlaneSceneOnly(p, meshes, shaderScene, spinningAngle);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();

    
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(window, true);

    
    bool fastCam = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                   glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    float currentCameraSpeed = (fastCam) ? cameraFastSpeed : cameraSpeed;

    cameraLookAt = computeCameraLookAt(lastMousePosX, lastMousePosY, dt);
        
    vec3 cameraSideVector = glm::cross(cameraLookAt, vec3(0.0f, 1.0f, 0.0f));
    glm::normalize(cameraSideVector);

    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
      cameraPosition += cameraLookAt * dt * currentCameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
      cameraPosition -= cameraLookAt * dt * currentCameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
      cameraPosition += cameraSideVector * dt * currentCameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
      cameraPosition -= cameraSideVector * dt * currentCameraSpeed;
    }
  }

  glfwTerminate();
  return 0;
}

vec3 computeCameraLookAt(double &lastMousePosX, double &lastMousePosY, float dt) {
  
  double mousePosX, mousePosY;
  glfwGetCursorPos(window, &mousePosX, &mousePosY);

  double dx = mousePosX - lastMousePosX;
  double dy = mousePosY - lastMousePosY;

  lastMousePosX = mousePosX;
  lastMousePosY = mousePosY;

  
  const float cameraAngularSpeed = 120.0f;
  static float cameraHorizontalAngle = 90.0f;
  static float cameraVerticalAngle = 0.0f;
  cameraHorizontalAngle -= (float)dx * cameraAngularSpeed * dt;
  cameraVerticalAngle   -= (float)dy * cameraAngularSpeed * dt;

  
  cameraVerticalAngle = glm::max(-85.0f, glm::min(85.0f, cameraVerticalAngle));

  float theta = radians(cameraHorizontalAngle);
  float phi   = radians(cameraVerticalAngle);

  return vec3(cosf(phi) * cosf(theta), sinf(phi), -cosf(phi) * sinf(theta));
}

GLuint setupModelVBO(string path, int& vertexCount) {
  std::vector<glm::vec3> vertices;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> UVs;

  // read vertex data from OBJ
  loadOBJ(path.c_str(), vertices, normals, UVs);

  GLuint VAO;
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);  // becomes active VAO

  // ver VBO setup
  GLuint vertices_VBO;
  glGenBuffers(1, &vertices_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, vertices_VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3),
               &vertices.front(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat),
                        (GLvoid*)0);
  glEnableVertexAttribArray(0);

  // norm VBO setup
  GLuint normals_VBO;
  glGenBuffers(1, &normals_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, normals_VBO);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3),
               &normals.front(), GL_STATIC_DRAW);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat),
                        (GLvoid*)0);
  glEnableVertexAttribArray(1);

  // UVs VBO setup
  GLuint uvs_VBO;
  glGenBuffers(1, &uvs_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, uvs_VBO);
  glBufferData(GL_ARRAY_BUFFER, UVs.size() * sizeof(glm::vec2), &UVs.front(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                        (GLvoid*)0);
  glEnableVertexAttribArray(2);

  glBindVertexArray(0);
  vertexCount = (int)vertices.size();
  return VAO;
}

GLuint setupModelEBO(string path, int& vertexCount) {
  vector<int> vertexIndices;
  vector<glm::vec3> vertices;
  vector<glm::vec3> normals;
  vector<glm::vec2> UVs;

  loadOBJ2(path.c_str(), vertexIndices, vertices, normals, UVs);

  GLuint VAO;
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);  // becomes active VAO

  // ver VBO setup
  GLuint vertices_VBO;
  glGenBuffers(1, &vertices_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, vertices_VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3),
               &vertices.front(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat),
                        (GLvoid*)0);
  glEnableVertexAttribArray(0);

  // norm VBO setup
  GLuint normals_VBO;
  glGenBuffers(1, &normals_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, normals_VBO);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3),
               &normals.front(), GL_STATIC_DRAW);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat),
                        (GLvoid*)0);
  glEnableVertexAttribArray(1);

  // UVs VBO setup
  GLuint uvs_VBO;
  glGenBuffers(1, &uvs_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, uvs_VBO);
  glBufferData(GL_ARRAY_BUFFER, UVs.size() * sizeof(glm::vec2), &UVs.front(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                        (GLvoid*)0);
  glEnableVertexAttribArray(2);

  // EBO setup
  GLuint EBO;
  glGenBuffers(1, &EBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertexIndices.size() * sizeof(int),
               &vertexIndices.front(), GL_STATIC_DRAW);

  glBindVertexArray(0);
  vertexCount = (int)vertexIndices.size();
  return VAO;
}

bool InitContext() {

  glfwInit();


  window = glfwCreateWindow(WIDTH, HEIGHT, "Assignment2", NULL, NULL);
  if (window == NULL) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return false;
  }
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
  glfwMakeContextCurrent(window);

  glewExperimental = true;
  if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to create GLEW" << std::endl;
    glfwTerminate();
    return false;
  }

  return true;
}
