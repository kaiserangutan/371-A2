#include <iostream>
#include <algorithm>
#include <vector>

#include <iostream>
#include <list>

#define GLEW_STATIC 1 // This allows linking with Static Library on Windows, without DLL
#include <GL/glew.h>  // Include GLEW - OpenGL Extension Wrangler

#include <GLFW/glfw3.h> // cross-platform interface for creating a graphical context,
                        // initializing OpenGL and binding inputs

#include <glm/glm.hpp>                  // GLM is an optimized math library with syntax to similar to OpenGL Shading Language
#include <glm/gtc/matrix_transform.hpp> // include this to create transformation matrices
#include <glm/common.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "shaderloader.h"
#include "OBJloader.h"  //For loading .obj files
#include "OBJloaderV2.h"  //For loading .obj files using a polygon list format

using namespace glm;
using namespace std;




class ShadowFrameBuffer {
private:
    GLuint framebuffer, depthTexture;
    const GLuint SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

public:
    ShadowFrameBuffer() {
        glGenFramebuffers(1, &framebuffer);

        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT,
            0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void bindForWriting() {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void bindShadowTexture(GLenum textureUnit = GL_TEXTURE1) {
        glActiveTexture(textureUnit);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
    }

    void unbind(int screenWidth, int screenHeight) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth, screenHeight);
    }
};

void setupSceneLighting(GLuint shaderProgram, vec3 lightPos, vec3 lightDir, vec3 lightColor, vec3 viewPos) {
    glUseProgram(shaderProgram);

    GLuint lightPosLoc = glGetUniformLocation(shaderProgram, "light_position");
    GLuint lightColorLoc = glGetUniformLocation(shaderProgram, "light_color");
    GLuint lightDirLoc = glGetUniformLocation(shaderProgram, "light_direction");
    GLuint viewPosLoc = glGetUniformLocation(shaderProgram, "view_position");
    GLuint objColorLoc = glGetUniformLocation(shaderProgram, "object_color");

    glUniform3fv(lightPosLoc, 1, &lightPos[0]);
    glUniform3fv(lightColorLoc, 1, &lightColor[0]);
    glUniform3fv(lightDirLoc, 1, &glm::normalize(lightDir)[0]);
    glUniform3fv(viewPosLoc, 1, &viewPos[0]);
    glUniform3fv(objColorLoc, 1, &vec3(1.0f, 1.0f, 1.0f)[0]);

    GLuint cutoffInnerLoc = glGetUniformLocation(shaderProgram, "light_cutoff_inner");
    GLuint cutoffOuterLoc = glGetUniformLocation(shaderProgram, "light_cutoff_outer");
    GLuint nearPlaneLoc = glGetUniformLocation(shaderProgram, "light_near_plane");
    GLuint farPlaneLoc = glGetUniformLocation(shaderProgram, "light_far_plane");

    glUniform1f(cutoffInnerLoc, glm::cos(glm::radians(12.5f)));
    glUniform1f(cutoffOuterLoc, glm::cos(glm::radians(17.5f)));
    glUniform1f(nearPlaneLoc, 1.0f);
    glUniform1f(farPlaneLoc, 25.0f);

    GLuint albedoLoc = glGetUniformLocation(shaderProgram, "albedo_tex");
    GLuint shadowMapLoc = glGetUniformLocation(shaderProgram, "shadow_map");
    glUniform1i(albedoLoc, 0);
    glUniform1i(shadowMapLoc, 1);
}

class Projectile
{
public:
    Projectile(vec3 position, vec3 velocity, GLuint shaderProgram) : mPosition(position), mVelocity(velocity)
    {
        mWorldMatrixLocation = glGetUniformLocation(shaderProgram, "model_matrix"); // CHANGED
    }

    void Update(float dt)
    {
        mPosition += mVelocity * dt;
    }

    
    void Draw() {
        mat4 worldMatrix = translate(mat4(1.0f), mPosition) *
            rotate(mat4(1.0f), radians(180.0f), vec3(0.0f, 1.0f, 0.0f)) *
            scale(mat4(1.0f), vec3(0.02f, 0.02f, 0.02f));
        glUniformMatrix4fv(mWorldMatrixLocation, 1, GL_FALSE, &worldMatrix[0][0]);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }

private:
    GLuint mWorldMatrixLocation;
    vec3 mPosition;
    vec3 mVelocity;
};

GLuint loadTexture(const char* filename);



GLuint loadTexture(const char* filename)
{
    // load texture dimension data
    int width, height, nrChannels;
    unsigned char* data = stbi_load(filename, &width, &height, &nrChannels, 0);
    if (!data)
    {
        std::cerr << "ERROR::texture could not load texture file\n"
            << filename << std::endl;
        return 0;
    }

    // step 2 create and bind texture
    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    assert(textureID != 0);

    glBindTexture(GL_TEXTURE_2D, textureID);

    // step 3 set filter parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // step 4 upload texture to the pu
    GLenum format = 0;
    if (nrChannels == 1)
    {
        format = GL_RED;
    }
    else if (nrChannels == 3)
    {
        format = GL_RGB;
    }
    else if (nrChannels == 4)
    {
        format = GL_RGBA;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    // step 5 free resources
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}


void setProjectionMatrix(GLuint shaderProgram, mat4 projectionMatrix)
{
    glUseProgram(shaderProgram);
    GLuint projectionMatrixLocation = glGetUniformLocation(shaderProgram, "projection_matrix"); // CHANGED
    glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, &projectionMatrix[0][0]);
}

void setViewMatrix(GLuint shaderProgram, mat4 viewMatrix)
{
    glUseProgram(shaderProgram);
    GLuint viewMatrixLocation = glGetUniformLocation(shaderProgram, "view_matrix"); // CHANGED
    glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, &viewMatrix[0][0]);
}

void setModelMatrix(GLuint shaderProgram, mat4 modelMatrix) //CHANGED
{
    glUseProgram(shaderProgram);
    GLuint modelMatrixLocation = glGetUniformLocation(shaderProgram, "model_matrix"); // CHANGED
    glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, &modelMatrix[0][0]);
}

// Draw gun (FINAL UPDATED)
void drawFirstPersonGun(GLuint shaderProgram, GLuint gunVAO, int gunVertices,
    vec3 cameraPosition, vec3 cameraLookAt, vec3 cameraUp) {
    // Calculate camera's right vector
    vec3 cameraRight = normalize(cross(cameraLookAt, cameraUp));

    // Position the gun relative to the camera
    vec3 gunOffset = cameraRight * 0.3f +      // 0.3 units to the right
        cameraLookAt * 0.8f +      // 0.8 units forward
        vec3(0.0f, -0.4f, 0.0f);   // 0.4 units down

    vec3 gunPosition = cameraPosition + gunOffset;

    // Calculate gun rotation to align with camera look direction
    float yaw = atan2(-cameraLookAt.z, cameraLookAt.x);
    float pitch = asin(cameraLookAt.y);

    // Create gun world matrix
    mat4 gunWorldMatrix = translate(mat4(1.0f), gunPosition) *
        rotate(mat4(1.0f), yaw, vec3(0.0f, 1.0f, 0.0f)) *
        rotate(mat4(1.0f), -pitch, vec3(0.0f, 0.0f, 1.0f)) *
        rotate(mat4(1.0f), radians(-90.0f), vec3(1.0f, 0.0f, 0.0f)) *
        scale(mat4(1.0f), vec3(0.15f, 0.15f, 0.15f)); // Adjust scale as needed

    // Set the world matrix and draw
    setModelMatrix(shaderProgram, gunWorldMatrix);
    glBindVertexArray(gunVAO);
    glDrawElements(GL_TRIANGLES, gunVertices, GL_UNSIGNED_INT, 0);
}



struct TexturedColoredVertex
{
    TexturedColoredVertex(vec3 _position, vec3 _normal, vec2 _uv)
        : position(_position), normal(_normal), uv(_uv) {
    }

    vec3 position;
    vec3 normal;
    vec2 uv;
};

GLuint setupModelVBO(string path, int& vertexCount) {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> UVs;

    //read the vertex data from the model's OBJ file
    loadOBJ(path.c_str(), vertices, normals, UVs);

    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO); //Becomes active VAO
    // Bind the Vertex Array Object first, then bind and set vertex buffer(s) and attribute pointer(s).

    //Vertex VBO setup
    GLuint vertices_VBO;
    glGenBuffers(1, &vertices_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, vertices_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices.front(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    //Normals VBO setup
    GLuint normals_VBO;
    glGenBuffers(1, &normals_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, normals_VBO);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals.front(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(1);

    //UVs VBO setup
    GLuint uvs_VBO;
    glGenBuffers(1, &uvs_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, uvs_VBO);
    glBufferData(GL_ARRAY_BUFFER, UVs.size() * sizeof(glm::vec2), &UVs.front(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(2);

    glBindVertexArray(0); // Unbind VAO (it's always a good thing to unbind any buffer/array to prevent strange bugs, as we are using multiple VAOs)
    vertexCount = vertices.size();
    return VAO;
}


//Sets up a model using an Element Buffer Object to refer to vertex data
GLuint setupModelEBO(string path, int& vertexCount)
{
    vector<int> vertexIndices; //The contiguous sets of three indices of vertices, normals and UVs, used to make a triangle
    vector<glm::vec3> vertices;
    vector<glm::vec3> normals;
    vector<glm::vec2> UVs;

    //read the vertices from the cube.obj file
    //We won't be needing the normals or UVs for this program
    loadOBJ2(path.c_str(), vertexIndices, vertices, normals, UVs);

    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO); //Becomes active VAO
    // Bind the Vertex Array Object first, then bind and set vertex buffer(s) and attribute pointer(s).

    //Vertex VBO setup
    GLuint vertices_VBO;
    glGenBuffers(1, &vertices_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, vertices_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices.front(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    //Normals VBO setup
    GLuint normals_VBO;
    glGenBuffers(1, &normals_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, normals_VBO);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals.front(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(1);

    //UVs VBO setup
    GLuint uvs_VBO;
    glGenBuffers(1, &uvs_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, uvs_VBO);
    glBufferData(GL_ARRAY_BUFFER, UVs.size() * sizeof(glm::vec2), &UVs.front(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(2);

    //EBO setup
    GLuint EBO;
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertexIndices.size() * sizeof(int), &vertexIndices.front(), GL_STATIC_DRAW);

    glBindVertexArray(0); // Unbind VAO (it's always a good thing to unbind any buffer/array to prevent strange bugs), remember: do NOT unbind the EBO, keep it bound to this VAO
    vertexCount = vertexIndices.size();
    return VAO;
}


const TexturedColoredVertex texturedPrism2VertexArray[] = {
    // left face - red
    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(1, 0, 0), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, 0.5f), vec3(1, 0, 0), vec2(1.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, 1.5f, 0.5f), vec3(1, 0, 0), vec2(1.0f, 1.0f)),

    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(1, 0, 0), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, 1.5f, 0.5f), vec3(1, 0, 0), vec2(1.0f, 1.0f)),
    TexturedColoredVertex(vec3(-0.5f, 1.5f, -0.5f), vec3(1, 0, 0), vec2(0.0f, 1.0f)),

    // far face - blue
    TexturedColoredVertex(vec3(0.5f, 1.5f, -0.5f), vec3(0, 0, 1), vec2(1.0f, 1.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(0, 0, 1), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, 1.5f, -0.5f), vec3(0, 0, 1), vec2(0.0f, 1.0f)),

    TexturedColoredVertex(vec3(0.5f, 1.5f, -0.5f), vec3(0, 0, 1), vec2(1.0f, 1.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, -0.5f), vec3(0, 0, 1), vec2(1.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(0, 0, 1), vec2(0.0f, 0.0f)),

    // bottom face - turquoise
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(0, 1, 1), vec2(1.0f, 1.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(0, 1, 1), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, -0.5f), vec3(0, 1, 1), vec2(1.0f, 0.0f)),

    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(0, 1, 1), vec2(1.0f, 1.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, 0.5f), vec3(0, 1, 1), vec2(0.0f, 1.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(0, 1, 1), vec2(0.0f, 0.0f)),

    // near face - green
    TexturedColoredVertex(vec3(-0.5f, 1.5f, 0.5f), vec3(0, 1, 0), vec2(0.0f, 1.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, 0.5f), vec3(0, 1, 0), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(0, 1, 0), vec2(1.0f, 0.0f)),

    TexturedColoredVertex(vec3(0.5f, 1.5f, 0.5f), vec3(0, 1, 0), vec2(1.0f, 1.0f)),
    TexturedColoredVertex(vec3(-0.5f, 1.5f, 0.5f), vec3(0, 1, 0), vec2(0.0f, 1.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(0, 1, 0), vec2(1.0f, 0.0f)),

    // right face - purple
    TexturedColoredVertex(vec3(0.5f, 1.5f, 0.5f), vec3(1, 0, 1), vec2(1.0f, 1.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, -0.5f), vec3(1, 0, 1), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, 1.5f, -0.5f), vec3(1, 0, 1), vec2(0.0f, 1.0f)),

    TexturedColoredVertex(vec3(0.5f, -0.5f, -0.5f), vec3(1, 0, 1), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, 1.5f, 0.5f), vec3(1, 0, 1), vec2(1.0f, 1.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(1, 0, 1), vec2(1.0f, 0.0f)),

    // top face - gray
    TexturedColoredVertex(vec3(0.5f, 1.5f, 0.5f), vec3(0.5f, 0.5f, 0.5f), vec2(1.0f, 1.0f)),
    TexturedColoredVertex(vec3(0.5f, 1.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f), vec2(1.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, 1.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f), vec2(0.0f, 0.0f)),

    TexturedColoredVertex(vec3(0.5f, 1.5f, 0.5f), vec3(0.5f, 0.5f, 0.5f), vec2(1.0f, 1.0f)),
    TexturedColoredVertex(vec3(-0.5f, 1.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, 1.5f, 0.5f), vec3(0.5f, 0.5f, 0.5f), vec2(0.0f, 1.0f)) };

const TexturedColoredVertex texturedPyramidVertexArray[] = {
    // left face (red)
    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(1, 0, 0), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, 0.5f), vec3(1, 0, 0), vec2(1.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.0f, 1.5f, 0.0f), vec3(1, 0, 0), vec2(0.5f, 1.0f)),

    // back face (blue)
    TexturedColoredVertex(vec3(0.5f, -0.5f, -0.5f), vec3(0, 0, 1), vec2(1.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(0, 0, 1), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.0f, 1.5f, 0.0f), vec3(0, 0, 1), vec2(0.5f, 1.0f)),

    // right face (purple)
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(1, 0, 1), vec2(1.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, -0.5f), vec3(1, 0, 1), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.0f, 1.5f, 0.0f), vec3(1, 0, 1), vec2(0.5f, 1.0f)),

    // front face (green)
    TexturedColoredVertex(vec3(-0.5f, -0.5f, 0.5f), vec3(0, 1, 0), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(0, 1, 0), vec2(1.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.0f, 1.5f, 0.0f), vec3(0, 1, 0), vec2(0.5f, 1.0f)),

    // bottom face - first triangle (cyan)
    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(0, 1, 1), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, -0.5f), vec3(0, 1, 1), vec2(1.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(0, 1, 1), vec2(1.0f, 1.0f)),

    // bottom face - second triangle (cyan)
    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(0, 1, 1), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(0, 1, 1), vec2(1.0f, 1.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, 0.5f), vec3(0, 1, 1), vec2(0.0f, 1.0f)),
};

const TexturedColoredVertex texturedTetraVertexArray[] = {
    // red face
    TexturedColoredVertex(vec3(1.f, 1.f, 1.f), vec3(1.f, 0.f, 0.f), vec2(0.5f, 1.0f)),
    TexturedColoredVertex(vec3(1.f, -1.f, -1.f), vec3(1.f, 0.f, 0.f), vec2(1.0f, 0.0f)),
    TexturedColoredVertex(vec3(-1.f, 1.f, -1.f), vec3(1.f, 0.f, 0.f), vec2(0.0f, 0.0f)),

    // green face
    TexturedColoredVertex(vec3(1.f, 1.f, 1.f), vec3(0.f, 1.f, 0.f), vec2(0.5f, 1.0f)),
    TexturedColoredVertex(vec3(-1.f, -1.f, 1.f), vec3(0.f, 1.f, 0.f), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(1.f, -1.f, -1.f), vec3(0.f, 1.f, 0.f), vec2(1.0f, 0.0f)),

    // blue face
    TexturedColoredVertex(vec3(1.f, 1.f, 1.f), vec3(0.f, 0.f, 1.f), vec2(0.5f, 1.0f)),
    TexturedColoredVertex(vec3(-1.f, 1.f, -1.f), vec3(0.f, 0.f, 1.f), vec2(1.0f, 0.0f)),
    TexturedColoredVertex(vec3(-1.f, -1.f, 1.f), vec3(0.f, 0.f, 1.f), vec2(0.0f, 0.0f)),

    // yellow face
    TexturedColoredVertex(vec3(1.f, -1.f, -1.f), vec3(1.f, 1.f, 0.f), vec2(0.5f, 1.0f)),
    TexturedColoredVertex(vec3(-1.f, -1.f, 1.f), vec3(1.f, 1.f, 0.f), vec2(1.0f, 0.0f)),
    TexturedColoredVertex(vec3(-1.f, 1.f, -1.f), vec3(1.f, 1.f, 0.f), vec2(0.0f, 0.0f)),
};

const TexturedColoredVertex texturedGroundVertexArray[] = {
    // Top face (Y+)
    TexturedColoredVertex(vec3(-0.5f, 0.5f, -0.5f), vec3(0.6f), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, 0.5f, -0.5f), vec3(0.6f), vec2(10.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, 0.5f, 0.5f), vec3(0.6f), vec2(0.0f, 10.0f)),
    TexturedColoredVertex(vec3(0.5f, 0.5f, -0.5f), vec3(0.6f), vec2(10.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, 0.5f, 0.5f), vec3(0.6f), vec2(10.0f, 10.0f)),
    TexturedColoredVertex(vec3(-0.5f, 0.5f, 0.5f), vec3(0.6f), vec2(0.0f, 10.0f)),

    // Bottom face (Y-)
    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(0.6f), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, 0.5f), vec3(0.6f), vec2(0.0f, 10.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, -0.5f), vec3(0.6f), vec2(10.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, -0.5f), vec3(0.6f), vec2(10.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, 0.5f), vec3(0.6f), vec2(0.0f, 10.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(0.6f), vec2(10.0f, 10.0f)),

    // Front face (Z+)
    TexturedColoredVertex(vec3(-0.5f, -0.5f, 0.5f), vec3(0.6f), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(0.6f), vec2(10.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, 0.5f, 0.5f), vec3(0.6f), vec2(0.0f, 10.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(0.6f), vec2(10.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, 0.5f, 0.5f), vec3(0.6f), vec2(10.0f, 10.0f)),
    TexturedColoredVertex(vec3(-0.5f, 0.5f, 0.5f), vec3(0.6f), vec2(0.0f, 10.0f)),

    // Back face (Z-)
    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(0.6f), vec2(10.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, 0.5f, -0.5f), vec3(0.6f), vec2(10.0f, 10.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, -0.5f), vec3(0.6f), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, -0.5f), vec3(0.6f), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, 0.5f, -0.5f), vec3(0.6f), vec2(10.0f, 10.0f)),
    TexturedColoredVertex(vec3(0.5f, 0.5f, -0.5f), vec3(0.6f), vec2(0.0f, 10.0f)),

    // Left face (X-)
    TexturedColoredVertex(vec3(-0.5f, -0.5f, -0.5f), vec3(0.6f), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, 0.5f, -0.5f), vec3(0.6f), vec2(0.0f, 10.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, 0.5f), vec3(0.6f), vec2(10.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, -0.5f, 0.5f), vec3(0.6f), vec2(10.0f, 0.0f)),
    TexturedColoredVertex(vec3(-0.5f, 0.5f, -0.5f), vec3(0.6f), vec2(0.0f, 10.0f)),
    TexturedColoredVertex(vec3(-0.5f, 0.5f, 0.5f), vec3(0.6f), vec2(10.0f, 10.0f)),

    // Right face (X+)
    TexturedColoredVertex(vec3(0.5f, -0.5f, -0.5f), vec3(0.6f), vec2(10.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(0.6f), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, 0.5f, -0.5f), vec3(0.6f), vec2(10.0f, 10.0f)),
    TexturedColoredVertex(vec3(0.5f, -0.5f, 0.5f), vec3(0.6f), vec2(0.0f, 0.0f)),
    TexturedColoredVertex(vec3(0.5f, 0.5f, 0.5f), vec3(0.6f), vec2(0.0f, 10.0f)),
    TexturedColoredVertex(vec3(0.5f, 0.5f, -0.5f), vec3(0.6f), vec2(10.0f, 10.0f)),
};

// cubes
vec3 prism1VertexArray[] = {                           // position,                            color
    vec3(-0.5f, -0.5f, -0.5f), vec3(1.0f, 0.0f, 0.0f), // left - red
    vec3(-0.5f, -0.5f, 0.5f), vec3(1.0f, 0.0f, 0.0f),
    vec3(-0.5f, 0.5f, 0.5f), vec3(1.0f, 0.0f, 0.0f),

    vec3(-0.5f, -0.5f, -0.5f), vec3(1.0f, 0.0f, 0.0f),
    vec3(-0.5f, 0.5f, 0.5f), vec3(1.0f, 0.0f, 0.0f),
    vec3(-0.5f, 0.5f, -0.5f), vec3(1.0f, 0.0f, 0.0f),

    vec3(0.5f, 0.5f, -0.5f), vec3(0.0f, 0.0f, 1.0f), // far - blue
    vec3(-0.5f, -0.5f, -0.5f), vec3(0.0f, 0.0f, 1.0f),
    vec3(-0.5f, 0.5f, -0.5f), vec3(0.0f, 0.0f, 1.0f),

    vec3(0.5f, 0.5f, -0.5f), vec3(0.0f, 0.0f, 1.0f),
    vec3(0.5f, -0.5f, -0.5f), vec3(0.0f, 0.0f, 1.0f),
    vec3(-0.5f, -0.5f, -0.5f), vec3(0.0f, 0.0f, 1.0f),

    vec3(0.5f, -0.5f, 0.5f), vec3(0.0f, 1.0f, 1.0f), // bottom - turquoise
    vec3(-0.5f, -0.5f, -0.5f), vec3(0.0f, 1.0f, 1.0f),
    vec3(0.5f, -0.5f, -0.5f), vec3(0.0f, 1.0f, 1.0f),

    vec3(0.5f, -0.5f, 0.5f), vec3(0.0f, 1.0f, 1.0f),
    vec3(-0.5f, -0.5f, 0.5f), vec3(0.0f, 1.0f, 1.0f),
    vec3(-0.5f, -0.5f, -0.5f), vec3(0.0f, 1.0f, 1.0f),

    vec3(-0.5f, 0.5f, 0.5f), vec3(0.0f, 1.0f, 0.0f), // near - green
    vec3(-0.5f, -0.5f, 0.5f), vec3(0.0f, 1.0f, 0.0f),
    vec3(0.5f, -0.5f, 0.5f), vec3(0.0f, 1.0f, 0.0f),

    vec3(0.5f, 0.5f, 0.5f), vec3(0.0f, 1.0f, 0.0f),
    vec3(-0.5f, 0.5f, 0.5f), vec3(0.0f, 1.0f, 0.0f),
    vec3(0.5f, -0.5f, 0.5f), vec3(0.0f, 1.0f, 0.0f),

    vec3(0.5f, 0.5f, 0.5f), vec3(1.0f, 0.0f, 1.0f), // right - purple
    vec3(0.5f, -0.5f, -0.5f), vec3(1.0f, 0.0f, 1.0f),
    vec3(0.5f, 0.5f, -0.5f), vec3(1.0f, 0.0f, 1.0f),

    vec3(0.5f, -0.5f, -0.5f), vec3(1.0f, 0.0f, 1.0f),
    vec3(0.5f, 0.5f, 0.5f), vec3(1.0f, 0.0f, 1.0f),
    vec3(0.5f, -0.5f, 0.5f), vec3(1.0f, 0.0f, 1.0f),

    vec3(0.5f, 0.5f, 0.5f), vec3(0.5f, 0.5f, 0.5f), // top - gray
    vec3(0.5f, 0.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 0.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f),

    vec3(0.5f, 0.5f, 0.5f), vec3(0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 0.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 0.5f, 0.5f), vec3(0.5f, 0.5f, 0.5f) };
vec3 prism2VertexArray[] = {                           // position,                            color
    vec3(-0.5f, -0.5f, -0.5f), vec3(1.0f, 0.0f, 0.0f), // left - red
    vec3(-0.5f, -0.5f, 0.5f), vec3(1.0f, 0.0f, 0.0f),
    vec3(-0.5f, 1.5f, 0.5f), vec3(1.0f, 0.0f, 0.0f),

    vec3(-0.5f, -0.5f, -0.5f), vec3(1.0f, 0.0f, 0.0f),
    vec3(-0.5f, 1.5f, 0.5f), vec3(1.0f, 0.0f, 0.0f),
    vec3(-0.5f, 1.5f, -0.5f), vec3(1.0f, 0.0f, 0.0f),

    vec3(0.5f, 1.5f, -0.5f), vec3(0.0f, 0.0f, 1.0f), // far - blue
    vec3(-0.5f, -0.5f, -0.5f), vec3(0.0f, 0.0f, 1.0f),
    vec3(-0.5f, 1.5f, -0.5f), vec3(0.0f, 0.0f, 1.0f),

    vec3(0.5f, 1.5f, -0.5f), vec3(0.0f, 0.0f, 1.0f),
    vec3(0.5f, -0.5f, -0.5f), vec3(0.0f, 0.0f, 1.0f),
    vec3(-0.5f, -0.5f, -0.5f), vec3(0.0f, 0.0f, 1.0f),

    vec3(0.5f, -0.5f, 0.5f), vec3(0.0f, 1.0f, 1.0f), // bottom - turquoise
    vec3(-0.5f, -0.5f, -0.5f), vec3(0.0f, 1.0f, 1.0f),
    vec3(0.5f, -0.5f, -0.5f), vec3(0.0f, 1.0f, 1.0f),

    vec3(0.5f, -0.5f, 0.5f), vec3(0.0f, 1.0f, 1.0f),
    vec3(-0.5f, -0.5f, 0.5f), vec3(0.0f, 1.0f, 1.0f),
    vec3(-0.5f, -0.5f, -0.5f), vec3(0.0f, 1.0f, 1.0f),

    vec3(-0.5f, 1.5f, 0.5f), vec3(0.0f, 1.0f, 0.0f), // near - green
    vec3(-0.5f, -0.5f, 0.5f), vec3(0.0f, 1.0f, 0.0f),
    vec3(0.5f, -0.5f, 0.5f), vec3(0.0f, 1.0f, 0.0f),

    vec3(0.5f, 1.5f, 0.5f), vec3(0.0f, 1.0f, 0.0f),
    vec3(-0.5f, 1.5f, 0.5f), vec3(0.0f, 1.0f, 0.0f),
    vec3(0.5f, -0.5f, 0.5f), vec3(0.0f, 1.0f, 0.0f),

    vec3(0.5f, 1.5f, 0.5f), vec3(1.0f, 0.0f, 1.0f), // right - purple
    vec3(0.5f, -0.5f, -0.5f), vec3(1.0f, 0.0f, 1.0f),
    vec3(0.5f, 1.5f, -0.5f), vec3(1.0f, 0.0f, 1.0f),

    vec3(0.5f, -0.5f, -0.5f), vec3(1.0f, 0.0f, 1.0f),
    vec3(0.5f, 1.5f, 0.5f), vec3(1.0f, 0.0f, 1.0f),
    vec3(0.5f, -0.5f, 0.5f), vec3(1.0f, 0.0f, 1.0f),

    vec3(0.5f, 1.5f, 0.5f), vec3(0.5f, 0.5f, 0.5f), // top - gray
    vec3(0.5f, 1.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 1.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f),

    vec3(0.5f, 1.5f, 0.5f), vec3(0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 1.5f, -0.5f), vec3(0.5f, 0.5f, 0.5f),
    vec3(-0.5f, 1.5f, 0.5f), vec3(0.5f, 0.5f, 0.5f) };

glm::vec3 pyramidVertexArray[] = {
    // left face
    {-0.5f, -0.5f, -0.5f},
    {1, 0, 0},
    {-0.5f, -0.5f, 0.5f},
    {1, 0, 0},
    {0.0f, 1.5f, 0.0f},
    {1, 0, 0},

    // back face
    {0.5f, -0.5f, -0.5f},
    {0, 0, 1},
    {-0.5f, -0.5f, -0.5f},
    {0, 0, 1},
    {0.0f, 1.5f, 0.0f},
    {0, 0, 1},

    // right face
    {0.5f, -0.5f, 0.5f},
    {1, 0, 1},
    {0.5f, -0.5f, -0.5f},
    {1, 0, 1},
    {0.0f, 1.5f, 0.0f},
    {1, 0, 1},

    // front face
    {-0.5f, -0.5f, 0.5f},
    {0, 1, 0},
    {0.5f, -0.5f, 0.5f},
    {0, 1, 0},
    {0.0f, 1.5f, 0.0f},
    {0, 1, 0},

    {-0.5f, -0.5f, -0.5f},
    {0, 1, 1},
    {0.5f, -0.5f, -0.5f},
    {0, 1, 1},
    {0.5f, -0.5f, 0.5f},
    {0, 1, 1},

    {-0.5f, -0.5f, -0.5f},
    {0, 1, 1},
    {0.5f, -0.5f, 0.5f},
    {0, 1, 1},
    {-0.5f, -0.5f, 0.5f},
    {0, 1, 1},
};

glm::vec3 tetraVertexArray[] = {
    // face red
    {1.f, 1.f, 1.f},
    {1.f, 0.f, 0.f},
    {1.f, -1.f, -1.f},
    {1.f, 0.f, 0.f},
    {-1.f, 1.f, -1.f},
    {1.f, 0.f, 0.f},

    // face green
    {1.f, 1.f, 1.f},
    {0.f, 1.f, 0.f},
    {-1.f, -1.f, 1.f},
    {0.f, 1.f, 0.f},
    {1.f, -1.f, -1.f},
    {0.f, 1.f, 0.f},

    // face blue
    {1.f, 1.f, 1.f},
    {0.f, 0.f, 1.f},
    {-1.f, 1.f, -1.f},
    {0.f, 0.f, 1.f},
    {-1.f, -1.f, 1.f},
    {0.f, 0.f, 1.f},

    // face yellow
    {1.f, -1.f, -1.f},
    {1.f, 1.f, 0.f},
    {-1.f, -1.f, 1.f},
    {1.f, 1.f, 0.f},
    {-1.f, 1.f, -1.f},
    {1.f, 1.f, 0.f},
};





int createTexturedVertexArrayObject(const TexturedColoredVertex* vertexArray, int arraySize)
{
    // Create a vertex array
    GLuint vertexArrayObject;
    glGenVertexArrays(1, &vertexArrayObject);
    glBindVertexArray(vertexArrayObject);

    // Upload Vertex Buffer to the GPU, keep a reference to it (vertexBufferObject)
    GLuint vertexBufferObject;
    glGenBuffers(1, &vertexBufferObject);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, arraySize * sizeof(TexturedColoredVertex), vertexArray, GL_STATIC_DRAW);

    glVertexAttribPointer(0,                     // attribute 0 matches aPos in Vertex Shader
        3,                     // size
        GL_FLOAT,              // type
        GL_FALSE,              // normalized?
        sizeof(TexturedColoredVertex), // stride
        (void*)offsetof(TexturedColoredVertex, position)              // array buffer offset
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, // attribute 1 matches aNormal in Vertex Shader
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TexturedColoredVertex),
        (void*)offsetof(TexturedColoredVertex, normal)
    );
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, // attribute 2 matches aUV in Vertex Shader
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TexturedColoredVertex),
        (void*)offsetof(TexturedColoredVertex, uv)
    );
    glEnableVertexAttribArray(2);

    return vertexArrayObject;
}

GLfloat lightVertices[] = {
    -0.1f, -0.1f,  0.1f,
    -0.1f, -0.1f, -0.1f,
     0.1f, -0.1f, -0.1f,
     0.1f, -0.1f,  0.1f,
    -0.1f,  0.1f,  0.1f,
    -0.1f,  0.1f, -0.1f,
     0.1f,  0.1f, -0.1f,
     0.1f,  0.1f,  0.1f
};

GLuint lightIndices[] = {
    0, 1, 2,
    0, 2, 3,
    0, 4, 7,
    0, 7, 3,
    3, 7, 6,
    3, 6, 2,
    2, 6, 5,
    2, 5, 1,
    1, 5, 4,
    1, 4, 0,
    4, 5, 6,
    4, 6, 7
};
int createVertexArrayObject(const glm::vec3* vertexArray, int arraySize)
{
    // Create a vertex array
    GLuint vertexArrayObject;
    glGenVertexArrays(1, &vertexArrayObject);
    glBindVertexArray(vertexArrayObject);

    // Upload Vertex Buffer to the GPU, keep a reference to it (vertexBufferObject)
    GLuint vertexBufferObject;
    glGenBuffers(1, &vertexBufferObject);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, arraySize, vertexArray, GL_STATIC_DRAW);

    glVertexAttribPointer(0,                     // attribute 0 matches aPos in Vertex Shader
        3,                     // size
        GL_FLOAT,              // type
        GL_FALSE,              // normalized?
        2 * sizeof(glm::vec3), // stride - each vertex contain 2 vec3 (position, color)
        (void*)0              // array buffer offset
    );
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, // attribute 1 matches aColor in Vertex Shader
        3,
        GL_FLOAT,
        GL_FALSE,
        2 * sizeof(glm::vec3),
        (void*)sizeof(glm::vec3) // color is offseted a vec3 (comes after position)
    );
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return vertexArrayObject;
}


class Light {
public:
    vec3 position;

    Light(vec3 pos) : position(pos) {}
};
//FINAL UPDATED
list<Projectile> projectileList;

int main(int argc, char* argv[])
{
    // Initialize GLFW and OpenGL version
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Create Window and rendering context using GLFW, resolution is 800x600
    GLFWwindow* window = glfwCreateWindow(800, 600, "Comp371 - Project 1", NULL, NULL);
    if (window == NULL)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // @TODO 3 - Disable mouse cursor
    // ...
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Initialize GLEW
    glewExperimental = true; // Needed for core profile
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to create GLEW" << std::endl;
        glfwTerminate();
        return -1;
    }

    //Initialize shaders
    std::string shaderPathPrefix = "Shaders/";
    GLuint shaderScene = loadSHADER(shaderPathPrefix + "scene_vertex.glsl",
        shaderPathPrefix + "scene_fragment.glsl");
    GLuint shaderShadow = loadSHADER(shaderPathPrefix + "shadow_vertex.glsl",
        shaderPathPrefix + "shadow_fragment.glsl");


    // Load Textures
    GLuint brickTextureID = loadTexture("Textures/brick.jpg");
    GLuint cementTextureID = loadTexture("Textures/cement.jpg");
    GLuint stoneTextureID = loadTexture("Textures/grass2.jpg");
    GLuint graniteTextureID = loadTexture("Textures/granite.jpg");
    GLuint sandTextureID = loadTexture("Textures/soilsand.jpg");
    GLuint woodTextureID = loadTexture("Textures/wood.jpg");

    // Black background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

 



    //Setup models
    string cubePath = "Models/cube.obj";



    //Load models as EBOs
    int cubeVertices;
    GLuint cubeVAO = setupModelEBO(cubePath, cubeVertices);
    int activeVAOVertices = cubeVertices;
    GLuint activeVAO = cubeVAO;

    // Load vehicle model 
    string tankPath = "Models/tank.obj";
    int tankVertices;
    GLuint tankVAO = setupModelEBO(tankPath, tankVertices);


    //string gunPath = "Models/m60.obj"; //FINAL UPDATED
    //int gunVertices;
    //GLuint gunVAO = setupModelEBO(gunPath, gunVertices);




 //// depth map for shadows (CHECK LATER IS ON UTILS BUT IS HARDCODED FOR AIRPLANE OBJECTS)
 //   const unsigned int DEPTH_MAP_TEXTURE_SIZE = 1024;
 //   utils::DepthMap depth = utils::CreateDepthMap(DEPTH_MAP_TEXTURE_SIZE);

    // Camera parameters for view transform
    vec3 cameraPosition(0.6f, 1.0f, 10.0f);
    vec3 cameraLookAt(0.0f, 0.0f, -1.0f);
    vec3 cameraUp(0.0f, 1.0f, 0.0f);

    // Other camera parameters
    float cameraSpeed = 1.0f;
    float cameraFastSpeed = 2 * cameraSpeed;
    float cameraHorizontalAngle = 70.0f;
    float cameraVerticalAngle = 20.0f;
    bool cameraFirstPerson = true;  // press 1 or 2 to toggle this variable
    bool cameraMouseControl = true; // press m to switch to keyboard only camera controls

    // Spinning cube at camera position
    float spinningCubeAngle = 0.0f;


    // Tank parameters
    vec3 tankPosition(0.0f, 0.0f, 0.0f);
    float tankRotationY = 0.0f;
    float tankSpeed = 3.0f;
    float tankTurnSpeed = 90.0f; // Add this line - degrees per second

    // Camera offset from tank (fixed relationship)
    float cameraDistance = 1.6f;  // Camera distance from tank
    float cameraHeight = 2.3f;    // Camera height above tank



    // Set projection matrix
    mat4 projectionMatrix = glm::perspective(90.0f, 800.0f / 600.0f, 0.01f, 100.0f);




    // Define and upload geometry to the GPU here ...
    int texturedPyramidVAO = createTexturedVertexArrayObject(texturedPyramidVertexArray, sizeof(texturedPyramidVertexArray));
    int texturedVaoTetra = createTexturedVertexArrayObject(texturedTetraVertexArray, sizeof(texturedTetraVertexArray));
    int texturedVaoPrism = createTexturedVertexArrayObject(texturedPrism2VertexArray, sizeof(texturedPrism2VertexArray));

    int texturedGround = createTexturedVertexArrayObject(texturedGroundVertexArray, sizeof(texturedGroundVertexArray));

    // Create shadow framebuffer
    ShadowFrameBuffer* shadowFB = new ShadowFrameBuffer();

    GLuint lightVAO, lightVBO, lightEBO;
    glGenVertexArrays(1, &lightVAO);
    glGenBuffers(1, &lightVBO);
    glGenBuffers(1, &lightEBO);

    glBindVertexArray(lightVAO);

    glBindBuffer(GL_ARRAY_BUFFER, lightVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lightVertices), lightVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lightEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(lightIndices), lightIndices, GL_STATIC_DRAW);

    // Define and upload geometry to the GPU here ...
    //int vaoCube = createVertexArrayObject(prism1VertexArray, sizeof(prism1VertexArray));
    // int vaoPrism = createVertexArrayObject(prism2VertexArray, sizeof(prism2VertexArray));
    // int vaoPyramid = createVertexArrayObject(pyramidVertexArray, sizeof(pyramidVertexArray));
    // int vaoTetra = createVertexArrayObject(tetraVertexArray, sizeof(tetraVertexArray));

    // For frame time
    float lastFrameTime = glfwGetTime();
    int lastMouseLeftState = GLFW_RELEASE;
    double lastMousePosX, lastMousePosY;
    glfwGetCursorPos(window, &lastMousePosX, &lastMousePosY);

    // Other OpenGL states to set once
    // Enable Backface culling
    glEnable(GL_CULL_FACE);



   // MAIN WHILE LOOP
    while (!glfwWindowShouldClose(window))
    {
        // Frame time calculation
        float dt = glfwGetTime() - lastFrameTime;
        lastFrameTime += dt;

        // Update spinning animation
        spinningCubeAngle += 50.0f * dt;

        // Light setup
        vec3 lightPos = vec3(0.0f, 10.0f, 0.0f);
        vec3 lightDir = vec3(0.0f, -1.0f, 0.0f);
        vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);

        // Calculate light matrices
        mat4 lightProjection = glm::perspective(glm::radians(90.0f), 1.0f, 1.0f, 25.0f);
        mat4 lightView = glm::lookAt(lightPos, lightPos + lightDir, vec3(0.0, 1.0, 0.0));
        mat4 lightSpaceMatrix = lightProjection * lightView;

        // ===== SHADOW PASS =====
        shadowFB->bindForWriting();
        glUseProgram(shaderShadow);

        GLuint transformLoc = glGetUniformLocation(shaderShadow, "transform_in_light_space");

        // Render ground to shadow map
        mat4 groundWorldMatrix = translate(mat4(1.0f), vec3(0.0f, -0.01f, 0.0f)) *
            scale(mat4(1.0f), vec3(100.0f, 0.02f, 100.0f));
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, &(lightSpaceMatrix * groundWorldMatrix)[0][0]);
        glBindVertexArray(texturedGround);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Render tank to shadow map
        mat4 tankWorldMatrix = translate(mat4(1.0f), tankPosition) *
            rotate(mat4(1.0f), radians(tankRotationY + 180.0f), vec3(0.0f, 1.0f, 0.0f)) *
            scale(mat4(1.0f), vec3(0.4f, 0.4f, 0.4f));
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, &(lightSpaceMatrix * tankWorldMatrix)[0][0]);
        glBindVertexArray(tankVAO);
        glDrawElements(GL_TRIANGLES, tankVertices, GL_UNSIGNED_INT, 0);

        // Render other objects to shadow map
        mat4 prismWorldMatrix = translate(mat4(1.0f), vec3(0.0f, 0.5f, 0.8f));
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, &(lightSpaceMatrix * prismWorldMatrix)[0][0]);
        glBindVertexArray(texturedVaoPrism);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        shadowFB->unbind(800, 600);

        // ===== SCENE PASS =====
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderScene);

        // Set matrices
        mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraLookAt, cameraUp);
        setProjectionMatrix(shaderScene, projectionMatrix);
        setViewMatrix(shaderScene, viewMatrix);

        GLuint lightProjViewLoc = glGetUniformLocation(shaderScene, "light_proj_view_matrix");
        glUniformMatrix4fv(lightProjViewLoc, 1, GL_FALSE, &lightSpaceMatrix[0][0]);

        // Setup lighting
        setupSceneLighting(shaderScene, lightPos, lightDir, lightColor, cameraPosition);

        // Bind shadow texture
        shadowFB->bindShadowTexture(GL_TEXTURE1);

        // Draw ground
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, stoneTextureID);
        setModelMatrix(shaderScene, groundWorldMatrix);
        glBindVertexArray(texturedGround);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Draw prism
        glBindTexture(GL_TEXTURE_2D, woodTextureID);
        setModelMatrix(shaderScene, prismWorldMatrix);
        glBindVertexArray(texturedVaoPrism);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Draw tetra
        glBindTexture(GL_TEXTURE_2D, graniteTextureID);
        mat4 tetraWorldMatrix = translate(mat4(1.0f), vec3(2.0f, 0.7f, -1.5f)) *
            scale(mat4(1.0f), vec3(0.7f, 0.7f, 0.7f));
        setModelMatrix(shaderScene, tetraWorldMatrix);
        glBindVertexArray(texturedVaoTetra);
        glDrawArrays(GL_TRIANGLES, 0, 12);

        // Draw pyramid
        glBindTexture(GL_TEXTURE_2D, sandTextureID);
        mat4 pyramidWorldMatrix = translate(mat4(1.0f), vec3(-2.0f, 0.5f, -1.f));
        setModelMatrix(shaderScene, pyramidWorldMatrix);
        glBindVertexArray(texturedPyramidVAO);
        glDrawArrays(GL_TRIANGLES, 0, 18);

        // For colored objects, set object_color uniform
        GLuint objColorLoc = glGetUniformLocation(shaderScene, "object_color");

        // Draw tank (olive green)
        glUniform3f(objColorLoc, 0.4f, 0.6f, 0.4f);
        setModelMatrix(shaderScene, tankWorldMatrix);
        glBindVertexArray(tankVAO);
        glDrawElements(GL_TRIANGLES, tankVertices, GL_UNSIGNED_INT, 0);

        // Draw spinning cubes
        float orbitRadius2 = 3.5f;
        vec3 orbitingCube1Position = vec3(0.0f, 6.0f, 0.0f) +
            vec3(cos(radians(spinningCubeAngle)) * orbitRadius2, 0.0f, sin(radians(spinningCubeAngle)) * orbitRadius2);

        // Center cube (red)
        glUniform3f(objColorLoc, 6.0f, 0.0f, 0.0f);
        mat4 centreCube = translate(mat4(1.0f), vec3(0.0f, 6.0f, 0.0f)) *
            rotate(mat4(1.0f), radians(spinningCubeAngle), vec3(0.0f, 1.0f, 0.0f)) *
            scale(mat4(1.0f), vec3(0.1f));
        setModelMatrix(shaderScene, centreCube);
        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, cubeVertices, GL_UNSIGNED_INT, 0);

        // Orbiting cube (green)
        glUniform3f(objColorLoc, 0.0f, 6.0f, 0.0f);
        mat4 orbitingCube1 = translate(mat4(1.0f), orbitingCube1Position) *
            rotate(mat4(1.0f), radians(spinningCubeAngle), vec3(0.0f, 1.0f, 0.0f)) *
            scale(mat4(1.0f), vec3(0.07f));
        setModelMatrix(shaderScene, orbitingCube1);
        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, cubeVertices, GL_UNSIGNED_INT, 0);

        // Update and draw projectiles
        glUniform3f(objColorLoc, 3.0f, 1.8f, 0.6f);
        for (list<Projectile>::iterator it = projectileList.begin(); it != projectileList.end(); ++it)
        {
            it->Update(dt);
            it->Draw();
        }

        // End Frame
        glfwSwapBuffers(window);
        glfwPollEvents();


        // Handle inputs
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) // move camera down
        {
            cameraFirstPerson = true;
        }

        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) // move camera down
        {
            cameraFirstPerson = false;
        }
        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) // toggle mouse control
        {
            cameraMouseControl = false;
        }
        if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) // toggle mouse control
        {
            cameraMouseControl = true;
        }

        if (lastMouseLeftState == GLFW_RELEASE && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            const float projectileSpeed = 150.0f; // projectile speed

            // Calculate tank's forward direction for spawn position
            vec3 tankForward = vec3(sin(radians(tankRotationY)), 0.0f, cos(radians(tankRotationY)));
            vec3 tankRight = vec3(cos(radians(tankRotationY)), 0.0f, -sin(radians(tankRotationY)));

            // Spawn position from front of tank
            vec3 spawnPosition = tankPosition +
                tankForward * 1.0f +           // 1.0 units in front of tank
                tankRight * 0.0f +             // 0.0 units to the side (center)
                vec3(0.0f, 0.5f, 0.0f);        // 0.5 units above tank

            // Calculate target point far ahead in camera's look direction
            vec3 targetPoint = cameraPosition + cameraLookAt * 100.0f; // 100 units ahead where camera is looking

            // Calculate direction from tank to camera target
            vec3 projectileDirection = normalize(targetPoint - spawnPosition);

            // Create projectile that travels from tank toward camera center
            projectileList.push_back(Projectile(spawnPosition, projectileSpeed * projectileDirection, shaderScene));
        }
        lastMouseLeftState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);



        // This was solution for Lab02 - Moving camera exercise
        // We'll change this to be a first or third person camera
        bool fastCam = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        float currentCameraSpeed = (fastCam) ? cameraFastSpeed : cameraSpeed;

        // @TODO 4 - Calculate mouse motion dx and dy
        //         - Update camera horizontal and vertical angle
        //...
        if (cameraMouseControl)
        {
            double mousePosx, mousePosy;
            glfwGetCursorPos(window, &mousePosx, &mousePosy);
            double dx = lastMousePosX - mousePosx;
            double dy = lastMousePosY - mousePosy;
            lastMousePosX = mousePosx;
            lastMousePosY = mousePosy;
            const float cameraAngularSpeed = 50.f;
            cameraHorizontalAngle += dx * dt * cameraAngularSpeed;
            cameraVerticalAngle += dy * dt * cameraAngularSpeed * 0.75f;
        }
        else
        {
            double dx = 0.;
            double dy = 0.;
            if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
            {
                dy = 1.;
            }
            if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
            {
                dy = -1.;
            }
            if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
            {
                dx = 1.;
            }
            if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
            {
                dx = -1.;
            }

            const float cameraAngularSpeed = 50.f;
            cameraHorizontalAngle += dx * dt * cameraAngularSpeed;
            cameraVerticalAngle += dy * dt * cameraAngularSpeed * 0.75f;
        }

        cameraHorizontalAngle = std::fmod(cameraHorizontalAngle, 360.f);
        cameraVerticalAngle = std::max(-85.f, std::min(cameraVerticalAngle, 85.f));

        float theta = glm::radians(cameraHorizontalAngle);
        float phi = glm::radians(cameraVerticalAngle);
        cameraLookAt = vec3(cosf(phi) * cosf(theta), sinf(phi), -cosf(phi) * sinf(theta));
        vec3 cameraSideVector = glm::cross(cameraLookAt, vec3(0.f, 1.f, 0.f));
        glm::normalize(cameraSideVector);




        //// Calculate ground-relative movement vectors (FINAL UPDATED)
        //vec3 groundForward = normalize(vec3(cameraLookAt.x, 0.0f, cameraLookAt.z)); // Forward on ground plane
        //vec3 groundRight = normalize(cross(groundForward, vec3(0.0f, 1.0f, 0.0f))); // Right on ground plane

        //if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) // move camera to the left
        //{
        //    cameraPosition -= currentCameraSpeed * dt * groundRight;
        //}

        //if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) // move camera to the right
        //{
        //    cameraPosition += currentCameraSpeed * dt * groundRight;
        //}

        //if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) // move camera backward
        //{
        //    cameraPosition -= groundForward * currentCameraSpeed * dt;
        //}

        //if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) // move camera forward
        //{
        //    cameraPosition += groundForward * currentCameraSpeed * dt;
        //}

        //// Keep camera above ground
        //float groundLevel = 0.0f;
        //float minCameraHeight = 1.5f;
        //cameraPosition.y = std::max(cameraPosition.y, groundLevel + minCameraHeight);



        //viewMatrix = lookAt(cameraPosition, cameraPosition + cameraLookAt, cameraUp);


       // Tank movement relative to camera direction
        bool fastTank = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        float currentTankSpeed = (fastTank) ? tankSpeed * 2.0f : tankSpeed;

        // Calculate tank's own forward direction (independent of camera look direction)
        vec3 tankForward = vec3(sin(radians(tankRotationY)), 0.0f, cos(radians(tankRotationY)));
        vec3 tankRight = vec3(cos(radians(tankRotationY)), 0.0f, -sin(radians(tankRotationY)));

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) // move tank forward in its own direction
        {
            tankPosition += tankForward * currentTankSpeed * dt;
        }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) // move tank backward in its own direction
        {
            tankPosition -= tankForward * currentTankSpeed * dt;
        }

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) // turn tank left
        {
            tankRotationY += tankTurnSpeed * dt;
        }

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) // turn tank right
        {
            tankRotationY -= tankTurnSpeed * dt;
        }

        // Keep tank on ground
        float groundLevel = 0.0f;
        tankPosition.y = groundLevel;

        // Mouse control for camera (independent of tank movement)
        if (cameraMouseControl)
        {
            double mousePosx, mousePosy;
            glfwGetCursorPos(window, &mousePosx, &mousePosy);
            double dx = lastMousePosX - mousePosx;
            double dy = lastMousePosY - mousePosy;
            lastMousePosX = mousePosx;
            lastMousePosY = mousePosy;
            const float cameraAngularSpeed = 50.f;
            cameraHorizontalAngle += dx * dt * cameraAngularSpeed;
            cameraVerticalAngle += dy * dt * cameraAngularSpeed * 0.75f;
        }
        else
        {
            // Keyboard camera control 
        }

        cameraHorizontalAngle = std::fmod(cameraHorizontalAngle, 360.f);
        cameraVerticalAngle = std::max(-85.f, std::min(cameraVerticalAngle, 85.f));

        // Calculate camera direction for camera positioning only
        theta = glm::radians(cameraHorizontalAngle);
        phi = glm::radians(cameraVerticalAngle);
        cameraLookAt = vec3(cosf(phi) * cosf(theta), sinf(phi), -cosf(phi) * sinf(theta));

        // Position camera at fixed offset from tank
        cameraPosition = tankPosition + vec3(0.0f, cameraHeight, cameraDistance);

        // Update view matrix  
        viewMatrix = lookAt(cameraPosition, cameraPosition + cameraLookAt, cameraUp);
        glUseProgram(shaderScene);  // Make sure correct shader is active
        GLuint viewMatrixLocation = glGetUniformLocation(shaderScene, "view_matrix");  // Correct name
        glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, &viewMatrix[0][0]);
    }

    // Shutdown GLFW
    glfwTerminate();

    return 0;
}