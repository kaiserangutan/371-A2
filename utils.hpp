#pragma once
#include <string>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "OBJloader.h"
#include "OBJloaderV3.h"
#include "Airplane.h"

namespace utils {


 void SetUniformMat4(GLuint shader, const char* name, const glm::mat4& m) {
  glUseProgram(shader);
  glUniformMatrix4fv(glGetUniformLocation(shader, name), 1, GL_FALSE, &m[0][0]);
}

 void SetUniformVec3(GLuint shader, const char* name, const glm::vec3& v) {
  glUseProgram(shader);
  glUniform3fv(glGetUniformLocation(shader, name), 1, &v[0]);
}

 void SetUniform1f(GLuint shader, const char* name, float v) {
  glUseProgram(shader);
  glUniform1f(glGetUniformLocation(shader, name), v);
}

 void SetUniform1i(GLuint shader, const char* name, int v) {
  glUseProgram(shader);
  glUniform1i(glGetUniformLocation(shader, name), v);
}


struct Mesh {
  GLuint vao = 0;
  int    vertices = 0;   // for glDrawArrays
  GLuint texture = 0;    
};

struct PlaneMeshes {
  Mesh plane;
  Mesh prop;
};

// loading models into vao
 Mesh SetupModelVBO(const std::string& path) {
  std::vector glmVertices(0, glm::vec3{});
  std::vector glmNormals(0, glm::vec3{});
  std::vector glmUVs(0, glm::vec2{});

  loadOBJ(path.c_str(), glmVertices, glmNormals, glmUVs);

  Mesh m;
  glGenVertexArrays(1, &m.vao);
  glBindVertexArray(m.vao);

  GLuint vboV, vboN, vboUV;
  glGenBuffers(1, &vboV);
  glBindBuffer(GL_ARRAY_BUFFER, vboV);
  glBufferData(GL_ARRAY_BUFFER, glmVertices.size()*sizeof(glm::vec3), glmVertices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
  glEnableVertexAttribArray(0);

  glGenBuffers(1, &vboN);
  glBindBuffer(GL_ARRAY_BUFFER, vboN);
  glBufferData(GL_ARRAY_BUFFER, glmNormals.size()*sizeof(glm::vec3), glmNormals.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
  glEnableVertexAttribArray(1);

  glGenBuffers(1, &vboUV);
  glBindBuffer(GL_ARRAY_BUFFER, vboUV);
  glBufferData(GL_ARRAY_BUFFER, glmUVs.size()*sizeof(glm::vec2), glmUVs.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
  glEnableVertexAttribArray(2);

  glBindVertexArray(0);
  m.vertices = static_cast<int>(glmVertices.size());
  return m;
}

// depth map texture and fbo
struct DepthMap {
  GLuint texture = 0;
  GLuint fbo = 0;
  GLsizei size = 0;
};

DepthMap CreateDepthMap(GLsizei texSize) {
  DepthMap d; d.size = texSize;

  glGenTextures(1, &d.texture);
  glBindTexture(GL_TEXTURE_2D, d.texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, texSize, texSize, 0,
               GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  

  glGenFramebuffers(1, &d.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, d.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, d.texture, 0);
  glDrawBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return d;
}

void BindShadowMap(GLuint depthTex) {
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, depthTex);
}


GLuint LoadTexture2D(const std::string& path);



// drawing helpers
glm::mat4 BuildPlaneBaseModel(const Airplane& p) {
  using namespace glm;
  const mat4 T   = translate(mat4(1.f), p.position());
  const mat4 Y   = p.velocityYawMatrix();
  const mat4 Br  = rotate(mat4(1.f), radians(p.bankRollDeg()), vec3(0,0,1));
  const mat4 Fix = rotate(mat4(1.f), radians(-90.f), vec3(1,0,0)); // model axis fix
  const mat4 S   = scale(mat4(1.f), vec3(0.2f));
  return T * Y * Br * Fix * S;
}

glm::mat4 BuildFloorBaseModel() {
  using namespace glm;
  const mat4 T = translate(mat4(1.f), vec3(0.f, -3.f, 0.f));
  const mat4 S = scale(mat4(1.f), vec3(40.f, 0.01f, 40.f));
  return T * S;
}

glm::mat4 BuildTankModel(const glm::vec3& pos, const glm::vec3& lookDir) {
  using namespace glm;
  glm::vec3 f = lookDir;
  f.y = 0.0f; f = glm::normalize(f);

  float yaw = -std::atan2(f.x, -f.z);          
  const glm::mat4 T   = glm::translate(glm::mat4(1.f), pos);
  const glm::mat4 Y   = glm::rotate(glm::mat4(1.f), yaw, glm::vec3(0,1,0));
  const glm::mat4 Fix = glm::rotate(glm::mat4(1.f), radians(180.f), vec3(0,1,0));
  const glm::mat4 S   = glm::mat4(1.f);

  return T * Y * Fix * S;
}
void DrawFloorShadowOnly(const Mesh& floorMesh, GLuint shaderShadow, const glm::mat4& lightProjView)
{ 
  using namespace glm;
  const mat4 floorModel = BuildFloorBaseModel();
  SetUniformMat4(shaderShadow, "transform_in_light_space", lightProjView * floorModel);

  glBindVertexArray(floorMesh.vao);
  glDrawArrays(GL_TRIANGLES, 0, floorMesh.vertices);
  glBindVertexArray(0);
}

void DrawTankShadowOnly(const glm::vec3& pos, const glm::vec3& lookDir,
                               const Mesh& tankMesh, GLuint shaderShadow,
                               const glm::mat4& lightProjView)
{
  using namespace glm;
  const mat4 model = BuildTankModel(pos, lookDir);
  SetUniformMat4(shaderShadow, "transform_in_light_space", lightProjView * model);
  glBindVertexArray(tankMesh.vao);
  glDrawArrays(GL_TRIANGLES, 0, tankMesh.vertices);
  glBindVertexArray(0);
}

void DrawCubeShadowOnly(const Mesh& floorMesh, GLuint shaderShadow, const glm::mat4& lightProjView)
{ 
  using namespace glm;
  const mat4 cubeModel = mat4(1.f);
  SetUniformMat4(shaderShadow, "transform_in_light_space", lightProjView * cubeModel);

  glBindVertexArray(floorMesh.vao);
  glDrawArrays(GL_TRIANGLES, 0, floorMesh.vertices);
  glBindVertexArray(0);
}

inline void DrawTankSceneOnly(const glm::vec3& pos, const glm::vec3& lookDir,
                              const Mesh& tankMesh, GLuint shaderScene)
{
  using namespace glm;
  const mat4 model = BuildTankModel(pos, lookDir);
  SetUniformMat4(shaderScene, "model_matrix", model);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tankMesh.texture);
  glBindVertexArray(tankMesh.vao);
  glDrawArrays(GL_TRIANGLES, 0, tankMesh.vertices);
  glBindVertexArray(0);
}

void DrawCubeSceneOnly(const Mesh& floorMesh, GLuint shaderScene)
{ 
  using namespace glm;
  const mat4 cubeModel = mat4(1.f);
  SetUniformMat4(shaderScene, "model_matrix", cubeModel);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, floorMesh.texture);

  glBindVertexArray(floorMesh.vao);
  glDrawArrays(GL_TRIANGLES, 0, floorMesh.vertices);
  glBindVertexArray(0);
}


void DrawFloorSceneOnly(const Mesh& floorMesh, GLuint shaderScene)
{ 
  using namespace glm;
  const mat4 floorModel = BuildFloorBaseModel();
  SetUniformMat4(shaderScene, "model_matrix", floorModel);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, floorMesh.texture);

  glBindVertexArray(floorMesh.vao);
  glDrawArrays(GL_TRIANGLES, 0, floorMesh.vertices);
  glBindVertexArray(0);
}
 void DrawPlaneShadowOnly(const Airplane& p,
                                const PlaneMeshes& mesh,
                                GLuint shaderShadow,
                                const glm::mat4& lightProjView,
                                float propSpinDeg)
{
  using namespace glm;
  const mat4 base = BuildPlaneBaseModel(p);

  const mat4 planeModel = base;
  const mat4 propModel  = base *
      translate(mat4(1.f), vec3(0.f, -15.0f, 0.8f)) *
      rotate(mat4(1.f), radians(propSpinDeg * 50.f), vec3(0,1,0)) *
      scale(mat4(1.f), vec3(1.3f));

  SetUniformMat4(shaderShadow, "transform_in_light_space", lightProjView * planeModel);
  glBindVertexArray(mesh.plane.vao);
  glDrawArrays(GL_TRIANGLES, 0, mesh.plane.vertices);
  glBindVertexArray(0);

  SetUniformMat4(shaderShadow, "transform_in_light_space", lightProjView * propModel);
  glBindVertexArray(mesh.prop.vao);
  glDrawArrays(GL_TRIANGLES, 0, mesh.prop.vertices);
  glBindVertexArray(0);
}

void DrawPlaneSceneOnly(const Airplane& p,
                               const PlaneMeshes& mesh,
                               GLuint shaderScene,
                               float propSpinDeg)
{
  using namespace glm;
  const mat4 base = BuildPlaneBaseModel(p);

  const mat4 planeModel = base;
  const mat4 propModel  = base *
      translate(mat4(1.f), vec3(0.f, -15.0f, 0.8f)) *
      rotate(mat4(1.f), radians(propSpinDeg * 50.f), vec3(0,1,0)) *
      scale(mat4(1.f), vec3(1.3f));

  // plane
  SetUniformMat4(shaderScene, "model_matrix", planeModel);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, mesh.plane.texture);
  glBindVertexArray(mesh.plane.vao);
  glDrawArrays(GL_TRIANGLES, 0, mesh.plane.vertices);
  glBindVertexArray(0);

  // prop
  SetUniformMat4(shaderScene, "model_matrix", propModel);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, mesh.prop.texture);
  glBindVertexArray(mesh.prop.vao);
  glDrawArrays(GL_TRIANGLES, 0, mesh.prop.vertices);
  glBindVertexArray(0);
}




} // namespace utils

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace utils {
GLuint LoadTexture2D(const std::string& path) {
  int w, h, n;
  
  unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, STBI_rgb); // force 3 channels

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);

  if (data) {
    GLenum fmt = GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
  } else {
    cout<<"Texture loading failed! "<<path;
    unsigned char white[4] = {255,255,255,255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  return tex;
}
} // namespace utils

