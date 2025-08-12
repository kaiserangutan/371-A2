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
#include "Bullet.h"
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
  utils::Mesh bulletMesh = utils::SetupModelVBO(cubePath);
  utils::Mesh planeMesh = utils::SetupModelVBO(planePath);
  utils::Mesh propMesh  = utils::SetupModelVBO(propPath);
  utils::PlaneMeshes meshes{ planeMesh, propMesh };

  string floorTexturePath = "Textures/desert.jpg";
  string bulletTexturePath = "Textures/steel.jpg";
  string tankTexturePath = "Textures/camo2.jpg";
  string planeTexturePath = "Textures/camo2.png";
  string propTexturePath = "Textures/steel.png";

  floorMesh.texture = utils::LoadTexture2D(floorTexturePath);
  tankMesh.texture = utils::LoadTexture2D(tankTexturePath);
  bulletMesh.texture = utils::LoadTexture2D(bulletTexturePath);
  meshes.plane.texture = utils::LoadTexture2D(planeTexturePath);
  meshes.prop.texture  = utils::LoadTexture2D(propTexturePath);

  // depth map for shadows
  const unsigned int DEPTH_MAP_TEXTURE_SIZE = 2048;
  utils::DepthMap depth = utils::CreateDepthMap(DEPTH_MAP_TEXTURE_SIZE);
  utils::DepthMap depthCam  = utils::CreateDepthMap(DEPTH_MAP_TEXTURE_SIZE);


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


  mat4 projectionMatrix = perspective(radians(75.0f), WIDTH * 1.0f / HEIGHT, 0.01f, 400.0f);
  mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraLookAt, cameraUp);


  utils::SetUniformMat4(shaderScene, "projection_matrix", projectionMatrix);
  utils::SetUniformMat4(shaderScene, "view_matrix", viewMatrix);

  float lightAngleOuter = radians(100.0f);
  float lightAngleInner = radians(99.0f);
  utils::SetUniform1f(shaderScene, "light_cutoff_inner", cos(lightAngleInner));
  utils::SetUniform1f(shaderScene, "light_cutoff_outer", cos(lightAngleOuter));
  utils::SetUniformVec3(shaderScene, "light_color", vec3(1.f, 1.f, .95f));
  utils::SetUniformVec3(shaderScene, "object_color", vec3(1));

  

  // tell shader which texture units to use
  utils::SetUniform1i(shaderScene, "albedo_tex", 0); // albedo on unit 0
  utils::SetUniform1i(shaderScene, "shadow_map", 1); // shadow map on unit 1
  utils::SetUniform1i(shaderScene, "cam_shadow_map", 2); // floodlight shadow map on unit 2

  // for camera floodlight
  utils::SetUniformVec3(shaderScene, "camLight_color", vec3(1.0f, 1.0f, 0.6f));
  utils::SetUniform1f (shaderScene, "camLight_cutoff_inner", cos(radians(10.0f)));
  utils::SetUniform1f (shaderScene, "camLight_cutoff_outer", cos(radians(14.0f)));
  utils::SetUniform1f (shaderScene, "camLight_intensity",    3.0f);

  float camNear = 0.03f, camFar = 100.0f;
    glm::mat4 camLightProj = glm::perspective(glm::radians(14.f * 2.0f), 1.0f, camNear, camFar);
    glm::mat4 camLightView = glm::lookAt(cameraPosition, cameraPosition + cameraLookAt, cameraUp);
    glm::mat4 camLightProjView = camLightProj * camLightView;


  float lastFrameTime = glfwGetTime();

  double lastMousePosX, lastMousePosY;
  glfwGetCursorPos(window, &lastMousePosX, &lastMousePosY);

  glEnable(GL_DEPTH_TEST);

  // Make some planes
  std::vector<Airplane> planes;
  std::vector<Bullet> bullets;
  

  float propSpinDeg = 0.f;
  float planeSpawnTimer = 4.f;
  float gunCDTimer = 0.f;

  bool floodLightOn = false;
  

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
    utils::SetUniformMat4(shaderScene, "camLight_proj_view_matrix", camLightProjView);
    utils::SetUniform1f(shaderScene, "light_near_plane", lightNearPlane);
    utils::SetUniform1f(shaderScene, "light_far_plane",  lightFarPlane);
    utils::SetUniformVec3(shaderScene, "light_position",  lightPosition);
    utils::SetUniformVec3(shaderScene, "light_direction", lightDirection);

    
    for (auto& p : planes) p.update(dt);
    for (auto& b : bullets) b.update(dt);

    
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
      for (auto& b : bullets) {
        if (!b.isAlive()) continue;
        utils::DrawBulletShadowOnly(b, bulletMesh, shaderShadow, lightProjView);
        for (auto& p : planes) {
          if (!p.isAlive()) continue;
          else if (glm::distance(b.position(), p.position()) < 3.f){
            b.kill();p.kill();
          }
        }
    }
    glViewport(0, 0, depthCam.size, depthCam.size);
    glBindFramebuffer(GL_FRAMEBUFFER, depthCam.fbo);
    glClear(GL_DEPTH_BUFFER_BIT);

    utils::DrawFloorShadowOnly(floorMesh, shaderShadow, camLightProjView);
    utils::DrawTankShadowOnly(tankPosition, tankLookAt, tankMesh, shaderShadow, camLightProjView);
    for (const auto& p : planes) if (p.isAlive()) utils::DrawPlaneShadowOnly(p, meshes, shaderShadow, camLightProjView, propSpinDeg);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0); 
    glDisable(GL_POLYGON_OFFSET_FILL);
    

    // SCENE PASS!!!
    glUseProgram(shaderScene);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.2f, 0.35f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glm::vec3 camPos = cameraPosition;
    glm::vec3 camDir = glm::normalize(cameraLookAt);
    vec3 cameraSideVector = normalize(glm::cross(cameraLookAt, vec3(0,1,0)));
    if (floodLightOn){

      utils::SetUniformVec3(shaderScene, "camLight_position",   camPos);
      utils::SetUniformVec3(shaderScene, "camLight_direction",  glm::rotate(mat4(1.f), radians(20.f), normalize(cameraUp))* glm::rotate(mat4(1.f), radians(-20.f), normalize(cameraSideVector)) * vec4(camDir, 1.f));
      utils::SetUniformVec3(shaderScene, "camLight_color",      glm::vec3(1.0f, 1.0f, 0.6f));
      utils::SetUniform1f (shaderScene, "camLight_intensity",   3.0f);
      utils::SetUniform1f (shaderScene, "camLight_cutoff_inner", cos(glm::radians(10.0f)));
      utils::SetUniform1f (shaderScene, "camLight_cutoff_outer", cos(glm::radians(14.0f)));
      glUniform2f(glGetUniformLocation(shaderScene, "uv_scale"), 1.0f, 1.0f);
    }
    else {utils::SetUniform1f (shaderScene, "camLight_intensity",   0.0f);}



    utils::BindShadowMap(depth.texture, depthCam.texture); //binds to tex unit 1 by default
    glUniform2f(glGetUniformLocation(shaderScene, "uv_scale"), 10.0f, 10.0f);

    utils::DrawFloorSceneOnly(floorMesh, shaderScene);
    glUniform2f(glGetUniformLocation(shaderScene, "uv_scale"), 1.0f, 1.0f);
    utils::DrawTankSceneOnly(tankPosition, tankLookAt, tankMesh, shaderScene);
    for (const auto& p : planes) {
      if (!p.isAlive()) continue;
      utils::DrawPlaneSceneOnly(p, meshes, shaderScene, propSpinDeg);
    }
    for (const auto& b : bullets) {
      if (!b.isAlive()) continue;
      utils::DrawBulletSceneOnly(b, bulletMesh, shaderScene);
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
    

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) tankYaw += TANK_TURN_SPEED * dt;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) tankYaw -= TANK_TURN_SPEED * dt;
    glm::vec3 tankForward = glm::vec3(std::sin(tankYaw), 0.0f, -std::cos(tankYaw));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) tankPosition += tankForward * (TANK_SPEED * dt);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) tankPosition -= tankForward * (TANK_SPEED * dt);
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) floodLightOn = true;
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) floodLightOn = false;
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && gunCDTimer > 0.3f) {
      vec3 gunLookAt = cameraLookAt;
      bullets.emplace_back(cameraPosition, glm::rotate(mat4(1.f), radians(20.f), normalize(cameraUp))* glm::rotate(mat4(1.f), radians(-20.f), normalize(cameraSideVector)) * vec4(gunLookAt, 1.f));
      gunCDTimer = 0.f;
    }
    gunCDTimer += dt;

    tankLookAt = glm::normalize(tankForward);
    cameraPosition = tankPosition - vec3(0.f, -4.f, 0.f);
    

    
      
  }

  glfwTerminate();
  return 0;
}

vec3 computeCameraLookAt(double &lastMousePosX, double &lastMousePosY, float dt) {
      // - Calculate mouse motion dx and dy
    // - Update camera horizontal and vertical angle
    double mousePosX, mousePosY;
    glfwGetCursorPos(window, &mousePosX, &mousePosY);

    double dx = mousePosX - lastMousePosX;
    double dy = mousePosY - lastMousePosY;

    lastMousePosX = mousePosX;
    lastMousePosY = mousePosY;

    // Convert to spherical coordinates
    const float cameraAngularSpeed = 120.0f;
    static float cameraHorizontalAngle = 90.0f;
    static float cameraVerticalAngle = 0.0f;
    cameraHorizontalAngle -= dx * cameraAngularSpeed * dt;
    cameraVerticalAngle -= dy * cameraAngularSpeed * dt;

    // Clamp vertical angle to [-85, 85] degrees
    cameraVerticalAngle =
        glm::max(-85.0f, glm::min(85.0f, cameraVerticalAngle));

    float theta = radians(cameraHorizontalAngle);
    float phi = radians(cameraVerticalAngle);

    return vec3(cosf(phi) * cosf(theta), sinf(phi), -cosf(phi) * sinf(theta));
}
bool InitContext() {
  glfwInit();


  window = glfwCreateWindow(WIDTH, HEIGHT, "Assignment 2", NULL, NULL);
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
