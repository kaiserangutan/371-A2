#include <algorithm>
#include <iostream>
#include <vector>

#define GLEW_STATIC 1
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "shaderloader.h"
#include "Airplane.h"
#include "utils.hpp"       // <— new helpers

using namespace glm;
using namespace std;

const GLuint WIDTH = 1024, HEIGHT = 768;

GLFWwindow* window = nullptr;
bool InitContext();

vec3 computeCameraLookAt(double &lastMousePosX, double &lastMousePosY, float dt);


int main(int argc, char* argv[]) {
  
  if (!InitContext()) return -1;
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  std::string shaderPathPrefix = "Shaders/";
  GLuint shaderScene  = loadSHADER(shaderPathPrefix + "scene_vertex.glsl",
                                   shaderPathPrefix + "scene_fragment.glsl");
  GLuint shaderShadow = loadSHADER(shaderPathPrefix + "shadow_vertex.glsl",
                                   shaderPathPrefix + "shadow_fragment.glsl");

  // Models
  string planePath = "Models/airplane3.obj";
  string propPath  = "Models/propeller3.obj";
  string cubePath = "Models/cube.obj";
  string tankPath = "Models/tank.obj";

  utils::Mesh tankMesh = utils::SetupModelVBO(tankPath);
  utils::Mesh floorMesh = utils::SetupModelVBO(cubePath);
  utils::Mesh planeMesh = utils::SetupModelVBO(planePath);
  utils::Mesh propMesh  = utils::SetupModelVBO(propPath);
  utils::PlaneMeshes meshes{ planeMesh, propMesh };

  string floorTexturePath = "Textures/grass2.jpg";
  string tankTexturePath = "Textures/camo.jpg";
  string planeTexturePath = "Textures/steel.png";
  string propTexturePath = "Textures/steel.png";

  floorMesh.texture = utils::LoadTexture2D(floorTexturePath);
  tankMesh.texture = utils::LoadTexture2D(tankTexturePath);
  meshes.plane.texture = utils::LoadTexture2D(planeTexturePath);
  meshes.prop.texture  = utils::LoadTexture2D(propTexturePath);

  // depth map for shadows
  const unsigned int DEPTH_MAP_TEXTURE_SIZE = 2048;
  utils::DepthMap depth = utils::CreateDepthMap(DEPTH_MAP_TEXTURE_SIZE);


  vec3 cameraPosition(0.6f, 2.f, 2.0f);
  vec3 tankPosition = cameraPosition + vec3(0.f, -2.1f, 0.f);
  vec3 cameraLookAt(0.0f, 0.0f, -1.0f);
  vec3 tankLookAt = cameraLookAt;
  vec3 cameraUp(0.0f, 1.0f, 0.0f);
  float cameraSpeed = 10.0f;
  float cameraFastSpeed = 3 * cameraSpeed;

  mat4 tankViewMatrix = lookAt(tankPosition, tankPosition + tankLookAt, vec3(0,1,0));
  float tankYaw = std::atan2(tankLookAt.x, -tankLookAt.z); 
  const float TANK_SPEED = 10.0f;                          
  const float TANK_TURN_SPEED = glm::radians(90.0f); 


  mat4 projectionMatrix = perspective(radians(70.0f), WIDTH * 1.0f / HEIGHT, 0.01f, 800.0f);
  mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraLookAt, cameraUp);


  utils::SetUniformMat4(shaderScene, "projection_matrix", projectionMatrix);
  utils::SetUniformMat4(shaderScene, "view_matrix", viewMatrix);

  float lightAngleOuter = radians(85.0f);
  float lightAngleInner = radians(20.0f);
  utils::SetUniform1f(shaderScene, "light_cutoff_inner", cos(lightAngleInner));
  utils::SetUniform1f(shaderScene, "light_cutoff_outer", cos(lightAngleOuter));
  utils::SetUniformVec3(shaderScene, "light_color", vec3(1.f, 1.f, .95f));
  utils::SetUniformVec3(shaderScene, "object_color", vec3(1));

  // tell shader which texture units to use
  utils::SetUniform1i(shaderScene, "albedo_tex", 0); // albedo on unit 0
  utils::SetUniform1i(shaderScene, "shadow_map", 1); // shadow map on unit 1


  float lastFrameTime = glfwGetTime();

  double lastMousePosX, lastMousePosY;
  glfwGetCursorPos(window, &lastMousePosX, &lastMousePosY);

  glEnable(GL_DEPTH_TEST);

  // Make some planes
  std::vector<Airplane> planes;
  

  float propSpinDeg = 0.f;
  float planeSpawnTimer = 4.f;

  while (!glfwWindowShouldClose(window)) {
    float dt = glfwGetTime() - lastFrameTime;
    lastFrameTime = glfwGetTime();
    propSpinDeg += 45.f * dt;
    
    if (planeSpawnTimer > 4.f){
      planes.emplace_back(glm::vec3(10.f, 20.f, -30.f));
      planes.emplace_back(glm::vec3(-8.f, 23.f, -25.f));
      planeSpawnTimer = 0.f;
      
    }
    planeSpawnTimer += dt;
    


    vec3 lightPosition = vec3(30.f,50.0f,5.0f);
    vec3 lightFocus(0, 0, -1);
    vec3 lightDirection = normalize(lightFocus - lightPosition);
    float lightNearPlane = 0.01f, lightFarPlane = 400.0f;
    mat4 lightProjMatrix = perspective(radians(100.0f),
      (float)DEPTH_MAP_TEXTURE_SIZE /(float)DEPTH_MAP_TEXTURE_SIZE,
      lightNearPlane, lightFarPlane);
    mat4 lightViewMatrix = lookAt(lightPosition, lightFocus, vec3(0,1,0));
    mat4 lightProjView   = lightProjMatrix * lightViewMatrix;

    

    utils::SetUniformMat4(shaderScene, "light_proj_view_matrix", lightProjView);
    utils::SetUniform1f(shaderScene, "light_near_plane", lightNearPlane);
    utils::SetUniform1f(shaderScene, "light_far_plane",  lightFarPlane);
    utils::SetUniformVec3(shaderScene, "light_position",  lightPosition);
    utils::SetUniformVec3(shaderScene, "light_direction", lightDirection);

    
    for (auto& p : planes) p.update(dt);

    
    viewMatrix = lookAt(cameraPosition, cameraPosition + cameraLookAt, cameraUp);
    utils::SetUniformMat4(shaderScene, "view_matrix", viewMatrix);
    utils::SetUniformVec3(shaderScene, "view_position", cameraPosition);

    // SHADOW PASS!!!
    glUseProgram(shaderShadow);
    glViewport(0, 0, depth.size, depth.size);
    glBindFramebuffer(GL_FRAMEBUFFER, depth.fbo);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
    glUniform2f(glGetUniformLocation(shaderScene, "uv_scale"), 1.f, 1.f);
    // glUniform2f(glGetUniformLocation(shaderScene, "uv_scale"), 10.0f, 10.0f);
    utils::DrawFloorShadowOnly(floorMesh, shaderShadow, lightProjView);
    
    utils::DrawTankShadowOnly(tankPosition, tankLookAt, tankMesh, shaderShadow, lightProjView);



    for (const auto& p : planes) {
      if (!p.isAlive()) continue;
      utils::DrawPlaneShadowOnly(p, meshes, shaderShadow, lightProjView, propSpinDeg);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0); 
    glDisable(GL_POLYGON_OFFSET_FILL);
    

    // SCENE PASS!!!
    glUseProgram(shaderScene);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.2f, 0.35f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUniform2f(glGetUniformLocation(shaderScene, "uv_scale"), 1.0f, 1.0f);

    utils::BindShadowMap(depth.texture); //binds to tex unit 1 by default
    glUniform2f(glGetUniformLocation(shaderScene, "uv_scale"), 10.0f, 10.0f);

    utils::DrawFloorSceneOnly(floorMesh, shaderScene);
    glUniform2f(glGetUniformLocation(shaderScene, "uv_scale"), 1.0f, 1.0f);
    utils::DrawTankSceneOnly(tankPosition, tankLookAt, tankMesh, shaderScene);
    for (const auto& p : planes) {
      if (!p.isAlive()) continue;
      utils::DrawPlaneSceneOnly(p, meshes, shaderScene, propSpinDeg);
    }
    glfwSwapBuffers(window);
    glfwPollEvents();

    // Input
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(window, true);

    bool fastCam = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                   glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    float currentCameraSpeed = (fastCam) ? cameraFastSpeed : cameraSpeed;

    cameraLookAt = computeCameraLookAt(lastMousePosX, lastMousePosY, dt);
    // vec3 cameraSideVector = normalize(glm::cross(cameraLookAt, vec3(0,1,0)));

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) tankYaw += TANK_TURN_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) tankYaw -= TANK_TURN_SPEED * dt;
    glm::vec3 tankForward = glm::vec3(std::sin(tankYaw), 0.0f, -std::cos(tankYaw));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) tankPosition += tankForward * (TANK_SPEED * dt);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) tankPosition -= tankForward * (TANK_SPEED * dt);
    tankLookAt = glm::normalize(tankForward);
    cameraPosition = tankPosition - vec3(0.f, -4.f, 0.f);
      
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

  const float cameraAngularSpeed = 90.0f;
  static float cameraHorizontalAngle = 90.0f;

  static float cameraVerticalAngle = 0.0f;

  cameraHorizontalAngle -= float(dx) * cameraAngularSpeed * dt;
  
  cameraVerticalAngle   -= float(dy) * cameraAngularSpeed * dt;
  cameraVerticalAngle = glm::clamp(cameraVerticalAngle, -85.0f, 85.0f);

  float theta = radians(cameraHorizontalAngle);
  float phi   = radians(cameraVerticalAngle);
  return vec3(cosf(phi) * cosf(theta), sinf(phi), -cosf(phi) * sinf(theta));
}

bool InitContext() {
  glfwInit();


  window = glfwCreateWindow(WIDTH, HEIGHT, "Comp371 - Tut 06", NULL, NULL);
  if (!window) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return false;
  }
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
  glfwMakeContextCurrent(window);

  glewExperimental = true;
  if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to create GLEW\n";
    glfwTerminate();
    return false;
  }
  return true;
}
