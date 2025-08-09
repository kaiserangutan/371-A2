#include "Scene.h"
#include <iostream>
#include "stb/stb_image.h"

using namespace std;

Scene::Scene()
    : m_groundVAO(0)
    , m_groundVBO(0)
    , m_groundEBO(0)
    , m_initialized(false)
{
}

Scene::~Scene()
{
    Cleanup();
}

bool Scene::Initialize()
{
    if (m_initialized) {
        std::cerr << "Scene already initialized!" << std::endl;
        return false;
    }

    // Create ground plane geometry
    CreateGroundPlane();

    m_initialized = true;
    return true;
}

void Scene::CreateGroundPlane()
{
    float size = 300.0f;

    // Vertex data: position (x,y,z), normal (x,y,z), texture coordinates (u,v)
    float vertices[] = {
        // Positions          // Normals        // Texture Coords
        -size, 0.0f, -size,   0.0f, 1.0f, 0.0f,  0.0f, 0.0f,   // Bottom-left
         size, 0.0f, -size,   0.0f, 1.0f, 0.0f,  20.0f, 0.0f,  // Bottom-right  
         size, 0.0f,  size,   0.0f, 1.0f, 0.0f,  20.0f, 20.0f, // Top-right
        -size, 0.0f,  size,   0.0f, 1.0f, 0.0f,  0.0f, 20.0f   // Top-left
    };

    unsigned int indices[] = {
        0, 1, 2,  // First triangle
        0, 2, 3   // Second triangle
    };

    glGenVertexArrays(1, &m_groundVAO);
    glBindVertexArray(m_groundVAO);

    glGenBuffers(1, &m_groundVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &m_groundEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_groundEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Texture coordinate attribute (location 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void Scene::Render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, GLuint groundTexture)
{
    if (!m_initialized) return;

    RenderGroundPlane(viewMatrix, projectionMatrix, groundTexture);
}

void Scene::RenderGroundPlane(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, GLuint groundTexture)
{
    std::cout << "RenderGroundPlane called" << std::endl;

    // Error check
    if (m_groundVAO == 0) {
        std::cerr << "ERROR: Ground VAO is 0!" << std::endl;
        return;
    }

    if (groundTexture == 0) {
        std::cerr << "ERROR: Ground texture is 0!" << std::endl;
        return;
    }

    std::cout << "VAO: " << m_groundVAO << ", Texture: " << groundTexture << std::endl;

    glm::mat4 groundModel = glm::mat4(1.0f);



    // Set model matrix
    GLint currentProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

    if (currentProgram == 0) {
        std::cerr << "ERROR: No shader program active!" << std::endl;
        return;
    }

    GLint modelLocation = glGetUniformLocation(currentProgram, "model_matrix");
    if (modelLocation != -1) {
        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, &groundModel[0][0]);
    }

    // Bind grass texture to unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, groundTexture);

    // Render the ground plane
    glBindVertexArray(m_groundVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    std::cout << "RenderGroundPlane completed" << std::endl;
}

void Scene::RenderGroundShadow(const glm::mat4& lightProjView)
{
    std::cout << "RenderGroundShadow called" << std::endl;

    if (!m_initialized) {
        std::cout << "Scene not initialized in shadow pass" << std::endl;
        return;
    }

    if (m_groundVAO == 0) {
        std::cout << "Ground VAO is 0 in shadow pass" << std::endl;
        return;
    }

    std::cout << "Shadow pass: VAO=" << m_groundVAO << std::endl;

    glm::mat4 groundModel = glm::mat4(1.0f);
    std::cout << "Created ground model matrix" << std::endl;

    GLint currentProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    std::cout << "Shadow shader program: " << currentProgram << std::endl;

    if (currentProgram == 0) {
        std::cout << "No shader program active in shadow pass!" << std::endl;
        return;
    }

    GLint transformLocation = glGetUniformLocation(currentProgram, "transform_in_light_space");
    std::cout << "Transform location: " << transformLocation << std::endl;

    if (transformLocation != -1) {
        std::cout << "About to set transform uniform" << std::endl;
        glm::mat4 transform = lightProjView * groundModel;
        glUniformMatrix4fv(transformLocation, 1, GL_FALSE, &transform[0][0]);
        std::cout << "Set transform uniform successfully" << std::endl;
    }

    std::cout << "About to bind VAO and draw" << std::endl;
    glBindVertexArray(m_groundVAO);
    std::cout << "VAO bound, about to draw elements" << std::endl;
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    std::cout << "Draw elements completed" << std::endl;
    glBindVertexArray(0);
    std::cout << "Shadow render completed" << std::endl;
}

void Scene::Cleanup()
{
    if (m_groundVAO != 0) {
        glDeleteVertexArrays(1, &m_groundVAO);
        m_groundVAO = 0;
    }

    if (m_groundVBO != 0) {
        glDeleteBuffers(1, &m_groundVBO);
        m_groundVBO = 0;
    }

    if (m_groundEBO != 0) {
        glDeleteBuffers(1, &m_groundEBO);
        m_groundEBO = 0;
    }

    m_initialized = false;
}

