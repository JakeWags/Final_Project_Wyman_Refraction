// Having some issues with undefined references to these GL anisotropic filtering constants
// Solution from: https://gamedev.stackexchange.com/questions/70829/why-is-gl-texture-max-anisotropy-ext-undefined
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

// honestly not sure why this is a thing, but it was required to get stbi to load
#define STB_IMAGE_IMPLEMENTATION
#include "gl/stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "gl/cyCore.h"
#include "gl/cyVector.h"
#include "gl/cyTriMesh.h"
#include "gl/cyGL.h"

#include "Camera.h"

/*
Jake Wagoner
u6048387
Final project: Wyman Image-Space Refraction

This project is an implementation of Wyman (2005)'s method for image-space refraction.

*NOTE* currently only supports an unmoving background with a central object. The current refraction is based only on the face surface normals.


There is potentially an issue with the direction of the refractions, but I expect that this is mostly due to the fact that this is single-surface refraction as of now.

The camera can be rotated around the center object using left click and drag.
The distance to the object can be changed using right click and drag.

CTRL + Mouse Click and Drag can be used to rotate the light around the object.
*/

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void loadTexture(const char* path, unsigned int uniform_index=-1);
glm::mat4 getProjection(float zoom = 45.0f);
unsigned int initPlane();
void initPlaneShader();
void renderPlane(unsigned int planeVAO);
void RenderScene(GLuint VAO, GLuint planeVAO, cy::TriMesh mesh);

// settings
unsigned int SCR_WIDTH = 1500;
unsigned int SCR_HEIGHT = 1500;

float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;

// first mouse flag
bool firstMouse = true;

// mouse clicks
bool leftMouse = false;
bool rightMouse = false;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool recompilingShaders = false;
bool swappingProjection = false;
bool holdingCTRL = false;
bool holdingALT = false;
bool perspectiveProjection = true;

// Distance to object
float distance = 40.0f;

Camera textureCamera(glm::vec3(0.0f, 0.0f, distance), -90.0f, 0.0f);

// Euler Angles
float pitch = 45.0f;
float yaw = 20.0f;
float texturePitch = 0.0f;
float textureYaw = -90.0f;
float rotationSpeed = 0.3f;

// lighting values
// by setting the light dir equal to the camera position, we get a directional light where specular should be max on a surface directly facing the camera
glm::vec3 lightDir(1.0f, 1.0f, 1.0f);
glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

float lightIntensity = 1.0f;

// material values
glm::vec3 specularColor(0.5f);
float shininess = 11.5f;
float refractiveIndex = 1.5f; // glass

// Shader initialization
cy::GLSLProgram shaderProgram;
cy::GLSLProgram planeShaderProgram;

std::vector<GLuint> textures;

float aniso = 8.0f; // not sure what a good value for this is...

// Modify the main function to use the render buffer
int main(int argc, char* argv[]) {
    std::cout << "loading .obj file: " << argv[1] << std::endl;

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Set GLFW context version to 3.3 and use core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create a windowed mode window and its OpenGL context
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Final Project: Wyman Refraction", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Load all OpenGL function pointers using GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Set the viewport
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    // Register the callback function
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);

    ////////////////////////////////////////////
    // Load the shaders
    ////////////////////////////////////////////
    if (!shaderProgram.BuildFiles("shaders/vertex_shader.glsl", "shaders/fragment_shader.glsl")) {
       std::cerr << "Failed to compile shaders" << std::endl;
       return -1;
    }
    shaderProgram.Bind();

    shaderProgram.RegisterUniform(0, "mv");
    shaderProgram.RegisterUniform(1, "mvNormal");
    shaderProgram.RegisterUniform(2, "p");
    shaderProgram.RegisterUniform(3, "model");

    shaderProgram.RegisterUniform(4, "lightColor");
    shaderProgram.RegisterUniform(5, "lightDirection");
    shaderProgram.RegisterUniform(6, "lightIntensity");
    shaderProgram.RegisterUniform(7, "specularColor");
    shaderProgram.RegisterUniform(8, "shininess");
	shaderProgram.RegisterUniform(9, "background");
    shaderProgram.RegisterUniform(10, "refractiveIndex");

    ////////////////////////////////////////////
    // Load the model using cyTriMesh
    ////////////////////////////////////////////
    cy::TriMesh mesh;
    std::string filePath = "assets/" + std::string(argv[1]);
    if (!mesh.LoadFromFileObj(filePath.c_str(), true)) {
        std::cout << "Failed to load model" << std::endl;
    }

    // generate and bind a VAO, VBO
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);

    // bind VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // get all vertices from mesh
    // list of vertices
    // this should probably be a smarter implementation like triangle strip...
    float* vertices = new float[mesh.NF() * 3 * 3];
    float* normals = new float[mesh.NF() * 3 * 3];
    float* texCoords = new float[mesh.NF() * 3 * 2];

    mesh.ComputeNormals();

    ///////////////////////////////////////////////////////////
    // Get all the vertex data, normal data, texture data, etc
    ///////////////////////////////////////////////////////////
    for (int i = 0; i < mesh.NF(); i++) {
        // get the vertices of the face
        vertices[i * 9 + 0] = mesh.V(mesh.F(i).v[0]).x;
        vertices[i * 9 + 1] = mesh.V(mesh.F(i).v[0]).y;
        vertices[i * 9 + 2] = mesh.V(mesh.F(i).v[0]).z;

        vertices[i * 9 + 3] = mesh.V(mesh.F(i).v[1]).x;
        vertices[i * 9 + 4] = mesh.V(mesh.F(i).v[1]).y;
        vertices[i * 9 + 5] = mesh.V(mesh.F(i).v[1]).z;

        vertices[i * 9 + 6] = mesh.V(mesh.F(i).v[2]).x;
        vertices[i * 9 + 7] = mesh.V(mesh.F(i).v[2]).y;
        vertices[i * 9 + 8] = mesh.V(mesh.F(i).v[2]).z;

        // get the normals of the vertices
        normals[i * 9 + 0] = mesh.VN(mesh.FN(i).v[0]).x;
        normals[i * 9 + 1] = mesh.VN(mesh.FN(i).v[0]).y;
        normals[i * 9 + 2] = mesh.VN(mesh.FN(i).v[0]).z;

        normals[i * 9 + 3] = mesh.VN(mesh.FN(i).v[1]).x;
        normals[i * 9 + 4] = mesh.VN(mesh.FN(i).v[1]).y;
        normals[i * 9 + 5] = mesh.VN(mesh.FN(i).v[1]).z;

        normals[i * 9 + 6] = mesh.VN(mesh.FN(i).v[2]).x;
        normals[i * 9 + 7] = mesh.VN(mesh.FN(i).v[2]).y;
        normals[i * 9 + 8] = mesh.VN(mesh.FN(i).v[2]).z;
    }

    ////////////////////////////////////////////
    // Create and Bind VBOs, VAO
    ////////////////////////////////////////////

    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh.NF() * 3 * 3, vertices, GL_STATIC_DRAW);

    // attribute data
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // normals
    unsigned int normalVBO;
    glGenBuffers(1, &normalVBO);
    glBindBuffer(GL_ARRAY_BUFFER, normalVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh.NF() * 3 * 3, normals, GL_STATIC_DRAW);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    // texture coordinates
    unsigned int texCoordVBO;
    glGenBuffers(1, &texCoordVBO);
    glBindBuffer(GL_ARRAY_BUFFER, texCoordVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh.NF() * 3 * 2, texCoords, GL_STATIC_DRAW);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);

    // unbind the VBO
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // unbind the VAO
    glBindVertexArray(0);
    
    // set lighting attributes
    shaderProgram.SetUniform("lightColor", lightColor.x, lightColor.y, lightColor.z);
    shaderProgram.SetUniform("lightDirection", lightDir.x, lightDir.y, lightDir.z);
    shaderProgram.SetUniform("lightIntensity", lightIntensity);
    shaderProgram.SetUniform("specularColor", specularColor.x, specularColor.y, specularColor.z);
    shaderProgram.SetUniform("shininess", shininess);
	shaderProgram.SetUniform("refractiveIndex", refractiveIndex);

    // Compute the bounding box to get the center points
    mesh.ComputeBoundingBox();

    GLuint frameBuffer = 0;
    glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);


    // Front and Back face textures
    GLuint frontDepthTexture, backDepthTexture;
    glGenTextures(1, &frontDepthTexture);
    glGenTextures(1, &backDepthTexture);

    glBindTexture(GL_TEXTURE_2D, frontDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, backDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Create front-face depth buffer
    GLuint frontDepthBuffer;
    glGenRenderbuffers(1, &frontDepthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, frontDepthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);

    // Create back-face depth buffer
    GLuint backDepthBuffer;
    glGenRenderbuffers(1, &backDepthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, backDepthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);

	// Attach the textures to the framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frontDepthTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, backDepthTexture, 0);

	// Attach the depth renderbuffers to the framebuffer
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, frontDepthBuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, backDepthBuffer);

	// set the draw buffers

    GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "Framebuffer is not complete!" << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (argc < 3) {
		std::cerr << "Please provide a background image" << std::endl;
		return -1;
	}
    loadTexture(("assets/" + std::string(argv[2])).c_str());
    //loadTexture("assets/buildings.png");

    unsigned int planeVAO = initPlane();
    initPlaneShader();

    planeShaderProgram.Bind();
	planeShaderProgram.SetUniform("background", 0);

	// set uniform for the background
    shaderProgram.Bind();
	shaderProgram.SetUniform("background", 0);

    ////////////////////////////////////////////
    // Render Loop
    ////////////////////////////////////////////
    while (!glfwWindowShouldClose(window)) {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		RenderScene(VAO, planeVAO, mesh);

        glfwSwapBuffers(window);

        // Poll for and process events
        glfwPollEvents();
    }

    // Clean up and exit
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ----------------------------------
void processInput(GLFWwindow* window)
{
    // if the user presses F6, recompile the shaders
    if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS && !recompilingShaders)
    {
        recompilingShaders = true;
        std::cout << "Recompiling shaders" << std::endl;
        shaderProgram.BuildFiles("shaders/vertex_shader.glsl", "shaders/fragment_shader.glsl");
		planeShaderProgram.BuildFiles("shaders/plane_vertex_shader.glsl", "shaders/plane_fragment_shader.glsl");
    }

    if (glfwGetKey(window, GLFW_KEY_F6) == GLFW_RELEASE)
    {
        recompilingShaders = false;
    }

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS && !swappingProjection)
    {
        swappingProjection = true;
        std::cout << "Swapping projection" << std::endl;
        perspectiveProjection = !perspectiveProjection;
    }

    // prevent multiple swaps per keypress
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE)
    {
        swappingProjection = false;
    }

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
    {
        holdingCTRL = true;
    }

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_RELEASE)
    {
        holdingCTRL = false;
    }
	if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS)
	{
		holdingALT = true;
	}

	if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_RELEASE)
	{
		holdingALT = false;
	}

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // if up arrow, increase index of refraction
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
	{
		refractiveIndex += 0.01f;
		shaderProgram.SetUniform("refractiveIndex", refractiveIndex);
	}
	// if down arrow, decrease index of refraction
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
	{
		refractiveIndex -= 0.01f;
		shaderProgram.SetUniform("refractiveIndex", refractiveIndex);
	}
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// -----------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
	SCR_WIDTH = width;
	SCR_HEIGHT = height;
    glViewport(0, 0, width, height);
}


// glfw: whenever the mouse moves, this callback is called
// This function computes the x and y offset and feeds it to the camera object, where the Euler angles are updated
// ------------------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    float rotationSpeed = 0.01f;

	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {

        // if ctrl and left mouse clicked
        if (holdingCTRL) {
            // rotate the light direction
            pitch += -yoffset * rotationSpeed;
            yaw += -xoffset * rotationSpeed;
        }
        else {
            // add to textureYaw and texturePitch
            textureYaw += xoffset * rotationSpeed;
            texturePitch += yoffset * rotationSpeed;
        }
	}

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS) {
        textureCamera.ProcessMouseMovement(GLFW_MOUSE_BUTTON_2, xoffset, yoffset, distance / 1.2, distance * 1.5);
    }
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    textureCamera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// get the projection matrix
// --------------------------------
glm::mat4 getProjection(float zoom)
{
    if (perspectiveProjection)
    {
        return glm::perspective(glm::radians(zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    }
    else
    {
        // use one over camera distance as a uniform scale factor
        return glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f) * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f / distance));
    }
}

// load and create a texture
// -----------------------------------------------------------
void loadTexture(const char* path, unsigned int uniform_index)
{
	unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    // set the texture wrapping parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// load image, create texture and generate mipmaps
	int width, height, nrChannels;
	unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 4);
	if (data)
	{
		GLenum format = GL_RGBA;
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
	else
	{
		std::cerr << "Failed to load texture" << std::endl;
	}

	stbi_image_free(data);

	textures.push_back(texture);
}

// Initialize the plane shader program
void initPlaneShader() {
    if (!planeShaderProgram.BuildFiles("shaders/plane_vertex_shader.glsl", "shaders/plane_fragment_shader.glsl")) {
        std::cerr << "Failed to compile plane shaders" << std::endl;
        exit(-1);
    }
    planeShaderProgram.Bind();
    planeShaderProgram.RegisterUniform(0, "mvp");
    planeShaderProgram.RegisterUniform(1, "background");
}

// Create the plane VAO and VBO
unsigned int initPlane() {
    float planeVertices[] = {
        // positions         // normals         // texture coords
        -1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, -1.0f,
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f,
         1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  -1.0f, 0.0f,

        -1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, -1.0f,
         1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  -1.0f, -1.0f
    };

    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // normals
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // texture coords
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    return planeVAO;
}

// Render the plane
void renderPlane(unsigned int planeVAO) {
    planeShaderProgram.Bind();

    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Add a small constant to the color of the plane to separate it from the background color
    glClearColor(0.01f, 0.01f, 0.01f, 1.0f);
}

void RenderScene(GLuint VAO, GLuint planeVAO, cy::TriMesh mesh) {
    // render the background plane
    glm::mat4 view = textureCamera.GetViewMatrix();

    // apply rotations to the camera
    view = glm::rotate(view, glm::radians(texturePitch), glm::vec3(1.0f, 0.0f, 0.0f));
    view = glm::rotate(view, glm::radians(textureYaw), glm::vec3(0.0f, 1.0f, 0.0f));

    // get the view direction after applying rotations
    glm::vec3 viewDir = glm::vec3(view[2][0], view[2][1], view[2][2]);

    // PLANE RENDERING

    // disable writing to the depth buffer
    glDepthMask(GL_FALSE);

    planeShaderProgram.Bind();

    glm::mat4 planeModel = glm::mat4(1.0f);

    planeShaderProgram.SetUniformMatrix4("mvp", glm::value_ptr(planeModel));

    // bind texture for background
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    planeShaderProgram.SetUniform("background", textures[0]);

    renderPlane(planeVAO);

    // enable writing to the depth buffer
    glDepthMask(GL_TRUE);

    // Get the projection matrix
    glm::mat4 projection = getProjection(textureCamera.Zoom);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(0.25f));
    glm::vec3 objectCenter = glm::vec3(0.0f, 0.0f, 0.0f);
    if (mesh.IsBoundBoxReady()) {
        // get the bounding box of the object
        cy::Vec3f boundMin = mesh.GetBoundMin();
        cy::Vec3f boundMax = mesh.GetBoundMax();

        // spatial object center with respect to the origin
        objectCenter = glm::vec3(
            (boundMin.x + boundMax.x) / 2.0f,
            (boundMin.y + boundMax.y) / 2.0f,
            (boundMin.z + boundMax.z) / 2.0f
        );

        // translate it such that the center is at the origin
        model = glm::translate(model, -objectCenter);
    }

    glm::mat4 mv = view * model;

    glm::mat3 mv3 = glm::mat3(mv);
    glm::mat3 mvNormal = glm::transpose(glm::inverse(mv3));

    shaderProgram.Bind();

    shaderProgram.SetUniformMatrix4("model", glm::value_ptr(model));
    shaderProgram.SetUniformMatrix4("mv", glm::value_ptr(mv));
    shaderProgram.SetUniformMatrix3("mvNormal", glm::value_ptr(mvNormal));
    shaderProgram.SetUniformMatrix4("p", glm::value_ptr(projection));

    lightDir = glm::vec3(
        cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
        sin(glm::radians(pitch)),
        sin(glm::radians(yaw)) * cos(glm::radians(pitch))
    );

    shaderProgram.SetUniform("lightDirection", lightDir.x, lightDir.y, lightDir.z);
    shaderProgram.SetUniform("envMapViewDir", viewDir.x, viewDir.y, viewDir.z);

    // bind texture for background
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);

    shaderProgram.SetUniform("background", textures[0]);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, mesh.NF() * 3);
}