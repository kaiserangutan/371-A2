#ifndef SCENE_H
#define SCENE_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include <string>

class Scene {
public:
    Scene();
    ~Scene();

    bool Initialize();
    void Render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, GLuint groundTexture);
    void Cleanup();

    void RenderGroundShadow(const glm::mat4& lightProjView);
    GLuint GetGroundVAO() const { return m_groundVAO; }

private:
    // Ground plane methods
    void CreateGroundPlane();
    void RenderGroundPlane(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, GLuint groundTexture);

    // Ground plane data
    GLuint m_groundVAO;
    GLuint m_groundVBO;
    GLuint m_groundEBO;

    bool m_initialized;
};

#endif // SCENE_H