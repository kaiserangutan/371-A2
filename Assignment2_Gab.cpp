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

#include "OBJloader.h"  //For loading .obj files
#include "OBJloaderV2.h"  //For loading .obj files using a polygon list format

using namespace glm;
using namespace std;

class Projectile
{
public:
    Projectile(vec3 position, vec3 velocity, int shaderProgram) : mPosition(position), mVelocity(velocity)
    {
        mWorldMatrixLocation = glGetUniformLocation(shaderProgram, "worldMatrix");
    }

    void Update(float dt)
    {
        mPosition += mVelocity * dt;
    }

    //FINAL UPDATED
    void Draw() {
        mat4 worldMatrix = translate(mat4(1.0f), mPosition) *
            rotate(mat4(1.0f), radians(180.0f), vec3(0.0f, 1.0f, 0.0f)) *
            scale(mat4(1.0f), vec3(0.02f, 0.02f, 0.02f)); //size/shape of projectile
        glUniformMatrix4fv(mWorldMatrixLocation, 1, GL_FALSE, &worldMatrix[0][0]);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }

private:
    GLuint mWorldMatrixLocation;
    vec3 mPosition;
    vec3 mVelocity;
};

GLuint loadTexture(const char* filename);

const char* getVertexShaderSource();

const char* getFragmentShaderSource();

const char* getTexturedVertexShaderSource();

const char* getTexturedFragmentShaderSource();

int compileAndLinkShaders(const char* vertexShaderSource, const char* fragmentShaderSource);

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



const char* getVertexShaderSource()
{
    return "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec3 aNormal;\n"
        "layout (location = 2) in vec2 aUV;\n"

        "\n"
        "out vec3 vertexNormal;\n"
        "out vec2 vertexUV;\n"
        "out vec3 worldPos;\n"  // Added for spotlight calculations
        "\n"
        "uniform mat4 worldMatrix;\n"
        "uniform mat4 viewMatrix;\n"
        "uniform mat4 projectionMatrix;\n"
        "\n"
        "void main()\n"
        "{\n"
        "   vertexUV = aUV;\n"
        "   vertexNormal = mat3(transpose(inverse(worldMatrix))) * aNormal;\n"
        "   worldPos = vec3(worldMatrix * vec4(aPos, 1.0));\n"  // Added world position
        "   mat4 modelViewProjection = projectionMatrix * viewMatrix * worldMatrix;\n"
        "   gl_Position = modelViewProjection * vec4(aPos, 1.0);\n"
        "}";
}


const char* getFragmentShaderSource()
{
    return "#version 330 core\n"
        "in vec3 vertexColor;\n"
        "in vec2 vertexUV;\n"
        "in vec3 vertexNormal;\n"
        "in vec3 worldPos;\n"
        "uniform sampler2D textureSampler;\n"
        "uniform bool useTexture;\n"
        "uniform vec3 objectColor;\n"
        "uniform vec3 spotlightPos[3];\n"                                    // Multiple spotlight uniforms - using explicit array size
        "uniform vec3 spotlightDir[3];\n"
        "uniform float spotlightCutoff[3];\n"
        "uniform float spotlightOuterCutoff[3];\n"
        "uniform vec3 spotlightColor[3];\n"
        "uniform float spotlightIntensity[3];\n"
        "\n"
        "out vec4 FragColor;\n"
        "float calculateSpotlight(vec3 lightPos, vec3 lightDir, float cutoff, float outerCutoff, vec3 fragPos)\n"
        "{\n"
        "    vec3 lightToFrag = normalize(fragPos - lightPos);\n"
        "    float theta = dot(lightToFrag, normalize(lightDir));\n"
        "    float epsilon = cutoff - outerCutoff;\n"
        "    float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);\n"
        "    return intensity;\n"
        "}\n"
        "void main()\n"
        "{\n"
        "   vec3 totalLightContribution = vec3(0.0);\n"                      // Calculate combined lighting from all 3 spotlights
        "   \n"
        "   for(int i = 0; i < 3; i++) {\n"                                 // Process each spotlight individually (unrolled loop for better compatibility)
        "       float spotIntensity = calculateSpotlight(spotlightPos[i], spotlightDir[i], spotlightCutoff[i], spotlightOuterCutoff[i], worldPos);\n"
        "       \n"
        "       vec3 normal = normalize(vertexNormal);\n"
        "       vec3 lightDirection = normalize(spotlightPos[i] - worldPos);\n"
        "       float diffuse = max(dot(normal, lightDirection), 0.0);\n"
        "       \n"
        "       float lightFactor = spotIntensity * diffuse * spotlightIntensity[i];\n"
        "       totalLightContribution += spotlightColor[i] * lightFactor;\n"
        "   }\n"
        "   \n"
        "   vec3 ambient = vec3(0.4);\n"                                     // Higher ambient lighting so models are always visible
        "   \n"
        "   if (useTexture) {\n"
        "       vec4 textureColor = texture(textureSampler, vertexUV);\n"
        "       FragColor = textureColor * vec4(ambient + totalLightContribution, 1.0);\n"
        "   } else {\n"
        "       float beam = sin(vertexUV.x * 10.0);\n"                      // Enhanced beam effect with better base color
        "       beam = beam * 0.5 + 0.5;\n"
        "       \n"
        "       vec3 baseColor = max(objectColor, vec3(0.6)) * 2.3;\n"       // Boost the objectColor to make it more visible and add beam multiplier
        "       vec3 beamColor = baseColor * (beam * 0.7 + 0.8);\n"         // Ensure minimum brightness
        "       vec3 finalColor = beamColor * (ambient + totalLightContribution * 1.2);\n"  // Beam effect with higher base
        "       \n"
        "       FragColor = vec4(finalColor, 1.0);\n"
        "   }\n"
        "}";
}





const char* getTexturedVertexShaderSource()
{
    return getVertexShaderSource(); // TODO: Replace this with the actual textured vertex shader
}

const char* getTexturedFragmentShaderSource()
{
    return getFragmentShaderSource(); // TODO: Replace this with the actual textured fragment shader
}

int compileAndLinkShaders(const char* vertexShaderSource, const char* fragmentShaderSource)
{
    // compile and link shader program
    // return shader program id
    // ------------------------------------

    // vertex shader
    int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
            << infoLog << std::endl;
    }

    // fragment shader
    int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
            << infoLog << std::endl;
    }

    // link shaders
    int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
            << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

void setProjectionMatrix(int shaderProgram, mat4 projectionMatrix)
{
    glUseProgram(shaderProgram);
    GLuint projectionMatrixLocation = glGetUniformLocation(shaderProgram, "projectionMatrix");
    glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, &projectionMatrix[0][0]);
}

void setViewMatrix(int shaderProgram, mat4 viewMatrix)
{
    glUseProgram(shaderProgram);
    GLuint viewMatrixLocation = glGetUniformLocation(shaderProgram, "viewMatrix");
    glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, &viewMatrix[0][0]);
}

void setWorldMatrix(int shaderProgram, mat4 worldMatrix)
{
    glUseProgram(shaderProgram);
    GLuint worldMatrixLocation = glGetUniformLocation(shaderProgram, "worldMatrix");
    glUniformMatrix4fv(worldMatrixLocation, 1, GL_FALSE, &worldMatrix[0][0]);
}
//Spotlight Track multiple objects 
void setMultipleSpotlightUniforms(int program, vec3 lightPositions[3], vec3 lightDirections[3], vec3 lightColors[3], float intensities[3]) {
    glUseProgram(program);

    for (int i = 0; i < 3; i++) {
        string posName = "spotlightPos[" + to_string(i) + "]";
        string dirName = "spotlightDir[" + to_string(i) + "]";
        string colorName = "spotlightColor[" + to_string(i) + "]";
        string intensityName = "spotlightIntensity[" + to_string(i) + "]";
        string cutoffName = "spotlightCutoff[" + to_string(i) + "]";
        string outerCutoffName = "spotlightOuterCutoff[" + to_string(i) + "]";

        GLint posLoc = glGetUniformLocation(program, posName.c_str());
        GLint dirLoc = glGetUniformLocation(program, dirName.c_str());
        GLint colorLoc = glGetUniformLocation(program, colorName.c_str());
        GLint intensityLoc = glGetUniformLocation(program, intensityName.c_str());
        GLint cutoffLoc = glGetUniformLocation(program, cutoffName.c_str());
        GLint outerCutoffLoc = glGetUniformLocation(program, outerCutoffName.c_str());

        if (posLoc >= 0) glUniform3fv(posLoc, 1, &lightPositions[i][0]);
        if (dirLoc >= 0) glUniform3fv(dirLoc, 1, &lightDirections[i][0]);
        if (colorLoc >= 0) glUniform3fv(colorLoc, 1, &lightColors[i][0]);
        if (intensityLoc >= 0) glUniform1f(intensityLoc, intensities[i]);

        // Adjust these angles to make lights cover more area
        float innerCutoff, outerCutoff;

        if (i == 0) {
            // Main center light - very wide coverage
            innerCutoff = cos(radians(8.0f));
            outerCutoff = cos(radians(12.0f));
        }
        else {
            // Orbiting lights - medium wide coverage
            innerCutoff = cos(radians(5.0f));
            outerCutoff = cos(radians(8.0f));
        }

        if (cutoffLoc >= 0) glUniform1f(cutoffLoc, innerCutoff);
        if (outerCutoffLoc >= 0) glUniform1f(outerCutoffLoc, outerCutoff);
    }
}



// Draw gun (FINAL UPDATED)
void drawFirstPersonGun(int shaderProgram, GLuint gunVAO, int gunVertices,
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
    setWorldMatrix(shaderProgram, gunWorldMatrix);
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

    // Load Textures
    GLuint brickTextureID = loadTexture("Textures/brick.jpg");
    GLuint cementTextureID = loadTexture("Textures/cement.jpg");
    GLuint stoneTextureID = loadTexture("Textures/grass2.jpg");
    GLuint graniteTextureID = loadTexture("Textures/granite.jpg");
    GLuint sandTextureID = loadTexture("Textures/soilsand.jpg");
    GLuint woodTextureID = loadTexture("Textures/wood.jpg");

    // Black background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Compile and link shaders here ...
    int shaderProgram = compileAndLinkShaders(getVertexShaderSource(), getFragmentShaderSource());
    // Compile and link shaders here ...
    int colorShaderProgram = compileAndLinkShaders(getVertexShaderSource(), getFragmentShaderSource());
    int texturedShaderProgram = compileAndLinkShaders(getTexturedVertexShaderSource(), getTexturedFragmentShaderSource());

    //int lightShaderProgram = compileAndLinkShaders(getLightVertexShaderSource(), getLightFragmentShaderSource());



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










    // We can set the shader once, since we have only one
    //glUseProgram(shaderProgram);

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


    // Set projection matrix for shader, this won't change
    mat4 projectionMatrix = glm::perspective(90.0f,           // field of view in degrees
        800.0f / 600.0f, // aspect ratio
        0.01f, 100.0f);  // near and far (near > 0)

    GLuint projectionMatrixLocation = glGetUniformLocation(shaderProgram, "projectionMatrix");
    glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, &projectionMatrix[0][0]);

    // Set initial view matrix
    mat4 viewMatrix = lookAt(cameraPosition,                // eye
        cameraPosition + cameraLookAt, // center
        cameraUp);                     // up

    GLuint viewMatrixLocation = glGetUniformLocation(shaderProgram, "viewMatrix");
    glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, &viewMatrix[0][0]);

    // Set View and Projection matrices on both shaders
    setViewMatrix(colorShaderProgram, viewMatrix);
    setViewMatrix(texturedShaderProgram, viewMatrix);


    setProjectionMatrix(colorShaderProgram, projectionMatrix);
    setProjectionMatrix(texturedShaderProgram, projectionMatrix);



    // Define and upload geometry to the GPU here ...
    int texturedPyramidVAO = createTexturedVertexArrayObject(texturedPyramidVertexArray, sizeof(texturedPyramidVertexArray));
    int texturedVaoTetra = createTexturedVertexArrayObject(texturedTetraVertexArray, sizeof(texturedTetraVertexArray));
    int texturedVaoPrism = createTexturedVertexArrayObject(texturedPrism2VertexArray, sizeof(texturedPrism2VertexArray));

    int texturedGround = createTexturedVertexArrayObject(texturedGroundVertexArray, sizeof(texturedGroundVertexArray));

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

    // @TODO 1 - Enable Depth Test
    // ...

    // Container for projectiles to be implemented in tutorial


    // Entering Main Loop
    while (!glfwWindowShouldClose(window))
    {
        // Frame time calculation
        float dt = glfwGetTime() - lastFrameTime;
        lastFrameTime += dt;
        glEnable(GL_DEPTH_TEST);
        // Each frame, reset color of each pixel to glClearColor

        // @TODO 1 - Clear Depth Buffer Bit as well
        // ...
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // // Draw textured geometry
        // glUseProgram(texturedShaderProgram);

        // glActiveTexture(GL_TEXTURE0);
        // GLuint textureLocation = glGetUniformLocation(texturedShaderProgram, "textureSampler");
        // glBindTexture(GL_TEXTURE_2D, cementTextureID);
        glUniform1i(glGetUniformLocation(texturedShaderProgram, "useTexture"), 1);

        // Set view matrix once for both shader programs
        mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraLookAt, cameraUp);
        setViewMatrix(texturedShaderProgram, viewMatrix);
        setViewMatrix(colorShaderProgram, viewMatrix);



        // Draw light source
        //glUseProgram(lightShaderProgram);



       // Replace the spotlight setup section in your main loop with this:

        // Calculate orbiting cube positions first (we need these for light positions)
        float orbitRadius2 = 3.5f;
        spinningCubeAngle += 50.0f * dt;

        // Calculate OrbitingCube1 position
        vec3 orbitingCube1Position = vec3(0.0f, 6.0f, 0.0f) +
            vec3(cos(radians(spinningCubeAngle)) * orbitRadius2, 0.0f, sin(radians(spinningCubeAngle)) * orbitRadius2);

        // Calculate OrbitingCube2 position (orbits around OrbitingCube1)
        float orbitSpeed2 = 2.0f;
        vec3 orbitingCube2Position = orbitingCube1Position +
            vec3(cos(radians(spinningCubeAngle * orbitSpeed2)) * orbitRadius2 * 0.4f, 0.0f,
                sin(radians(spinningCubeAngle * orbitSpeed2)) * orbitRadius2 * 0.4f);

        // Set up multiple spotlights
        vec3 lightPositions[3] = {
            vec3(0.0f, 10.0f, 0.0f),           // Main light at center (raised higher)
            orbitingCube1Position + vec3(0.0f, 2.0f, 0.0f),  // Light above OrbitingCube1
            orbitingCube2Position + vec3(0.0f, 2.0f, 0.0f)   // Light above OrbitingCube2
        };

        // Calculate dynamic light directions that follow movement
        vec3 centerTarget = vec3(0.0f, 0.0f, 0.0f);  // Target point for lights to shine at



        //Downrd facing lights
        vec3 lightDirections[3] = {
            vec3(0.0f, -1.0f, 0.0f),          // Main light points down
            vec3(0.0f, -1.0f, 0.0f),          // OrbitingCube1 light points down
            vec3(0.0f, -1.0f, 0.0f)           // OrbitingCube2 light points down
        };

        vec3 lightColors[3] = {
            vec3(1.0f, 0.0f, 0.0f),           // White light
            vec3(0.0f, 1.0f, 0.0f),           // Green light for OrbitingCube1
            vec3(0.0f, 0.0f, 1.0f)            // Blue light for OrbitingCube2
        };

        float intensities[3] = { 2.0f, 1.5f, 1.5f };   // Different intensities

        // Update spotlight uniforms for BOTH shader programs BEFORE drawing
        setMultipleSpotlightUniforms(colorShaderProgram, lightPositions, lightDirections, lightColors, intensities);
        setMultipleSpotlightUniforms(texturedShaderProgram, lightPositions, lightDirections, lightColors, intensities);

        glUseProgram(texturedShaderProgram);
        glUniform1i(glGetUniformLocation(texturedShaderProgram, "useTexture"), 1);

        // Draw ground
        glUseProgram(texturedShaderProgram);
        glUniform1i(glGetUniformLocation(texturedShaderProgram, "useTexture"), 1);
        glBindTexture(GL_TEXTURE_2D, stoneTextureID);
        glBindVertexArray(texturedGround);
        mat4 groundWorldMatrix = translate(mat4(1.0f), vec3(0.0f, -0.01f, 0.0f)) * scale(mat4(1.0f), vec3(100.0f, 0.02f, 100.0f));  //FINALCHANGE
        GLuint worldMatrixLocation = glGetUniformLocation(texturedShaderProgram, "worldMatrix");
        glUniformMatrix4fv(worldMatrixLocation, 1, GL_FALSE, &groundWorldMatrix[0][0]);
        glDrawArrays(GL_TRIANGLES, 0, 36); // 6 vertices for the ground, not 36
        // 36 vertices, starting at index 0

// Draw textured geometry
        glUseProgram(texturedShaderProgram); // Use textured shader program
        glUniform1i(glGetUniformLocation(texturedShaderProgram, "useTexture"), 1); // Enable texture

        // Draw prism
        glBindVertexArray(texturedVaoPrism);
        glBindTexture(GL_TEXTURE_2D, woodTextureID);
        mat4 prismWorldMatrix = translate(mat4(1.0f), vec3(0.0f, 0.5f, 0.8f)) * scale(mat4(1.0f), vec3(1.0f, 1.0f, 1.0f));
        setWorldMatrix(texturedShaderProgram, prismWorldMatrix);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Draw tetra
        glBindVertexArray(texturedVaoTetra);
        glBindTexture(GL_TEXTURE_2D, graniteTextureID);
        mat4 tetraWorldMatrix = translate(mat4(1.0f), vec3(2.0f, 0.7f, -1.5f)) * scale(mat4(1.0f), vec3(0.7f, 0.7f, 0.7f));
        setWorldMatrix(texturedShaderProgram, tetraWorldMatrix);
        glDrawArrays(GL_TRIANGLES, 0, 12);

        // Draw spinning tetra
        mat4 spinTetraWorldMatrix = glm::mat4(1.0);
        for (int i = 0; i < 4; i++) {
            glBindVertexArray(texturedVaoTetra);
            glBindTexture(GL_TEXTURE_2D, brickTextureID);
            mat4 spinTetraWorldMatrix = glm::rotate(mat4(1.0f), radians(i * 120.f + 0.5f * spinningCubeAngle), vec3(0.0f, 1.0f, 0.0f)) * translate(mat4(1.0f), vec3(2.8f, 2.0f, 0.f)) * scale(mat4(1.0f), vec3(0.3f, 0.3f, 0.3f));
            setWorldMatrix(texturedShaderProgram, spinTetraWorldMatrix);
            glDrawArrays(GL_TRIANGLES, 0, 12);

        }


        // Draw pyramid
        glBindVertexArray(texturedPyramidVAO);
        glBindTexture(GL_TEXTURE_2D, sandTextureID);
        mat4 pyramidWorldMatrix = translate(mat4(1.0f), vec3(-2.0f, 0.5f, -1.f)) * scale(mat4(1.0f), vec3(1.0f, 1.0f, 1.0f));
        setWorldMatrix(texturedShaderProgram, pyramidWorldMatrix);
        glDrawArrays(GL_TRIANGLES, 0, 18);

        glBindTexture(GL_TEXTURE_2D, 0);  // This unbinds any active texture


        // Draw colored geometry
        glUseProgram(colorShaderProgram); // Use color shader program
        glUniform1i(glGetUniformLocation(colorShaderProgram, "useTexture"), 0); // Disable texture usage in color shader



        // Draw spinning model 
        glBindVertexArray(activeVAO); // Make sure cube VAO is bound
        GLuint objectColorLocation = glGetUniformLocation(colorShaderProgram, "objectColor");

        // Update and draw projectiles (FINAL UPDATED)
        glUniform3f(objectColorLocation, 3.0f, 1.8f, 0.6f); // Color projectiles

        for (list<Projectile>::iterator it = projectileList.begin(); it != projectileList.end(); ++it)
        {
            it->Update(dt);
            it->Draw();
        }





        // Draw center cube (red)
        glUniform3f(objectColorLocation, 6.0f, 0.0f, 0.0f);
        mat4 CentreCube = glm::translate(mat4(1.0f), vec3(0.0f, 6.0f, 0.0f)) *
            glm::rotate(mat4(1.0f), radians(spinningCubeAngle), vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(mat4(1.0f), radians(-90.0f), vec3(1.0f, 0.0f, 0.0f)) *
            glm::scale(mat4(1.0f), vec3(0.1f));
        setWorldMatrix(colorShaderProgram, CentreCube);
        glDrawElements(GL_TRIANGLES, activeVAOVertices, GL_UNSIGNED_INT, 0);

        // Draw OrbitingCube1 (Green)
        glUniform3f(objectColorLocation, 0.0f, 6.0f, 0.0f);
        mat4 OrbitingCube1 = glm::translate(mat4(1.0f), orbitingCube1Position) *
            glm::rotate(mat4(1.0f), radians(spinningCubeAngle), vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(mat4(1.0f), radians(-90.0f), vec3(1.0f, 0.0f, 0.0f)) *
            glm::scale(mat4(1.0f), vec3(0.07f));
        setWorldMatrix(colorShaderProgram, OrbitingCube1);
        glDrawElements(GL_TRIANGLES, activeVAOVertices, GL_UNSIGNED_INT, 0);

        // Draw OrbitingCube2 (Blue)
        glUniform3f(objectColorLocation, 0.0f, 0.0f, 6.0f);
        mat4 OrbitingCube2 = glm::translate(mat4(1.0f), orbitingCube2Position) *
            glm::rotate(mat4(1.0f), radians(spinningCubeAngle), vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(mat4(1.0f), radians(-90.0f), vec3(1.0f, 0.0f, 0.0f)) *
            glm::scale(mat4(1.0f), vec3(0.04f));
        setWorldMatrix(colorShaderProgram, OrbitingCube2);
        glDrawElements(GL_TRIANGLES, activeVAOVertices, GL_UNSIGNED_INT, 0);

        // Draw tank
        if (tankVAO != 0 && tankVertices > 0) {
            glUniform3f(objectColorLocation, 0.4f, 0.6f, 0.4f); // Olive green color

            mat4 tankWorldMatrix = translate(mat4(1.0f), tankPosition) *
                rotate(mat4(1.0f), radians(tankRotationY+ 180.0f), vec3(0.0f, 1.0f, 0.0f)) *
                scale(mat4(1.0f), vec3(0.4f, 0.4f, 0.4f)); // Tank size adjustment

            setWorldMatrix(colorShaderProgram, tankWorldMatrix);
            glBindVertexArray(tankVAO);
            glDrawElements(GL_TRIANGLES, tankVertices, GL_UNSIGNED_INT, 0);
        }


        glBindVertexArray(0);





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
            projectileList.push_back(Projectile(spawnPosition, projectileSpeed * projectileDirection, colorShaderProgram));
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
        GLuint viewMatrixLocation = glGetUniformLocation(shaderProgram, "viewMatrix");
        glUniformMatrix4fv(viewMatrixLocation, 1, GL_FALSE, &viewMatrix[0][0]);
    }

    // Shutdown GLFW
    glfwTerminate();

    return 0;
}