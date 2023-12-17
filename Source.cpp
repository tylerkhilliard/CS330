#include <iostream>         // cout, cerr
#include <algorithm>
#include <cstdlib>          // EXIT_FAILURE
#include <GL/glew.h>        // GLEW library
#include <GLFW/glfw3.h>     // GLFW library

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "meshes.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std; // Standard namespace

/*Shader program Macro*/
#ifndef GLSL
#define GLSL(Version, Source) "#version " #Version " core \n" #Source
#endif
// Global camera variables
glm::vec3 cameraPosition = glm::vec3(0.0f, 0.0f, 12.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float lastX = 400.0f;
float lastY = 300.0f;
bool firstMouse = true;

float yaw = -90.0f;
float pitch = 0.0f;
float fov = 45.0f;

float movementSpeed = 0.05f;
bool isOrthographic = false;

GLuint gTexture1Id;
GLuint gTexture2Id;
GLuint gTexture3Id;
GLuint gTexture4Id;
GLuint gTexture5Id;

// Function to create a texture from an image file using stb_image
bool UCreateTexture(const char* filename, GLuint& textureId)
{
	int width, height, channels;
	unsigned char* image = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);

	if (!image)
	{
		std::cerr << "Error loading texture: " << filename << std::endl;
		return false;
	}

	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(image);
	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}
// Function prototypes
void UUpdateCamera();
void UProcessInput(GLFWwindow* window);
void UMouseCallback(GLFWwindow* window, double xpos, double ypos);
void URender();
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
void UDestroyShaderProgram(GLuint programId);
// Global variable for the location of the view matrix in the shader program
GLint viewLoc;

void UUpdateCamera();
void UProcessInput(GLFWwindow* window);
void UMouseCallback(GLFWwindow* window, double xpos, double ypos);
void UScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	// Adjust the movement speed based on the scroll wheel input
	movementSpeed += 0.01f * static_cast<float>(yoffset);

	// Ensure that the movement speed doesn't go below a minimum value
	movementSpeed = std::max(movementSpeed, 0.01f);
}
// Unnamed namespace
namespace
{
	const char* const WINDOW_TITLE = "Module 7"; // Macro for window title

	// Variables for window width and height
	const int WINDOW_WIDTH = 800;
	const int WINDOW_HEIGHT = 600;

	// Stores the GL data relative to a given mesh
	struct GLMesh
	{
		GLuint vao;         // Handle for the vertex array object
		GLuint vbos[2];     // Handles for the vertex buffer objects
		GLuint nIndices;    // Number of indices of the mesh
	};

	// Main GLFW window
	GLFWwindow* gWindow = nullptr;
	// Triangle mesh data
	//GLMesh gMesh;
	// Shader program
	GLuint gProgramId;

	//Shape Meshes from Professor Brian
	Meshes meshes;
}

/* User-defined Function prototypes to:
 * initialize the program, set the window size,
 * redraw graphics on the window when resized,
 * and render graphics on the screen
 */
bool UInitialize(int, char* [], GLFWwindow** window);
void UResizeWindow(GLFWwindow* window, int width, int height);
void UProcessInput(GLFWwindow* window);
void URender();
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
void UDestroyShaderProgram(GLuint programId);

////////////////////////////////////////////////////////////////////////////////////////
/* Surface Vertex Shader Source Code*/
const GLchar* vertexShaderSource = GLSL(440,

	layout(location = 0) in vec3 vertexPosition; // VAP position 0 for vertex position data
layout(location = 1) in vec3 vertexNormal; // VAP position 1 for normals
layout(location = 2) in vec2 textureCoordinate;

out vec3 vertexFragmentNormal; // For outgoing normals to fragment shader
out vec3 vertexFragmentPos; // For outgoing color / pixels to fragment shader
out vec2 vertexTextureCoordinate;

//Uniform / Global variables for the  transform matrices
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(vertexPosition, 1.0f); // Transforms vertices into clip coordinates

	vertexFragmentPos = vec3(model * vec4(vertexPosition, 1.0f)); // Gets fragment / pixel position in world space only (exclude view and projection)

	vertexFragmentNormal = mat3(transpose(inverse(model))) * vertexNormal; // get normal vectors in world space only and exclude normal translation properties
	vertexTextureCoordinate = textureCoordinate;
}
);
////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Surface Fragment Shader Source Code*/
const GLchar* fragmentShaderSource = GLSL(440,

	in vec3 vertexFragmentNormal; // For incoming normals
in vec3 vertexFragmentPos; // For incoming fragment position
in vec2 vertexTextureCoordinate;

out vec4 fragmentColor; // For outgoing cube color to the GPU

// Uniform / Global variables for object color, light color, light position, and camera/view position
uniform vec4 objectColor;
uniform vec3 ambientColor;
uniform vec3 light1Color;
uniform vec3 light1Position;
uniform vec3 light2Color;
uniform vec3 light2Position;
uniform vec3 viewPosition;
uniform sampler2D uTexture; // Useful when working with multiple textures
uniform bool ubHasTexture;
uniform float ambientStrength = 0.1f; // Set ambient or global lighting strength
uniform float specularIntensity1 = 0.8f;
uniform float highlightSize1 = 16.0f;
uniform float specularIntensity2 = 0.8f;
uniform float highlightSize2 = 16.0f;

void main()
{
	/*Phong lighting model calculations to generate ambient, diffuse, and specular components*/

	//Calculate Ambient lighting
	vec3 ambient = ambientStrength * ambientColor; // Generate ambient light color

	//**Calculate Diffuse lighting**
	vec3 norm = normalize(vertexFragmentNormal); // Normalize vectors to 1 unit
	vec3 light1Direction = normalize(light1Position - vertexFragmentPos); // Calculate distance (light direction) between light source and fragments/pixels on cube
	float impact1 = max(dot(norm, light1Direction), 0.0);// Calculate diffuse impact by generating dot product of normal and light
	vec3 diffuse1 = impact1 * light1Color; // Generate diffuse light color
	vec3 light2Direction = normalize(light2Position - vertexFragmentPos); // Calculate distance (light direction) between light source and fragments/pixels on cube
	float impact2 = max(dot(norm, light2Direction), 0.0);// Calculate diffuse impact by generating dot product of normal and light
	vec3 diffuse2 = impact2 * light2Color; // Generate diffuse light color

	//**Calculate Specular lighting**
	vec3 viewDir = normalize(viewPosition - vertexFragmentPos); // Calculate view direction
	vec3 reflectDir1 = reflect(-light1Direction, norm);// Calculate reflection vector
	//Calculate specular component
	float specularComponent1 = pow(max(dot(viewDir, reflectDir1), 0.0), highlightSize1);
	vec3 specular1 = specularIntensity1 * specularComponent1 * light1Color;
	vec3 reflectDir2 = reflect(-light2Direction, norm);// Calculate reflection vector
	//Calculate specular component
	float specularComponent2 = pow(max(dot(viewDir, reflectDir2), 0.0), highlightSize2);
	vec3 specular2 = specularIntensity2 * specularComponent2 * light2Color;

	//**Calculate phong result**
	//Texture holds the color to be used for all three components
	vec4 textureColor = texture(uTexture, vertexTextureCoordinate);
	vec3 phong1;
	vec3 phong2;

	if (ubHasTexture == true)
	{
		phong1 = (ambient + diffuse1 + specular1) * textureColor.xyz;
		phong2 = (ambient + diffuse2 + specular2) * textureColor.xyz;
	}
	else
	{
		phong1 = (ambient + diffuse1 + specular1) * objectColor.xyz;
		phong2 = (ambient + diffuse2 + specular2) * objectColor.xyz;
	}

	fragmentColor = vec4(phong1 + phong2, 1.0); // Send lighting results to GPU
	//fragmentColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
}
);
///////////////////////////////////////////////////////////////////////////////////////

/* User-defined Function prototypes to:
 * initialize the program, set the window size,
 * redraw graphics on the window when resized,
 * and render graphics on the screen
 */
bool UInitialize(int, char* [], GLFWwindow** window);
void UResizeWindow(GLFWwindow* window, int width, int height);
void UProcessInput(GLFWwindow* window);
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos);
void URender();
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
void UDestroyShaderProgram(GLuint programId);
bool UCreateTexture(const char* filename, GLuint& textureId);
void UDestroyTexture(GLuint textureId);
int main(int argc, char* argv[])
{
	if (!UInitialize(argc, argv, &gWindow))
		return EXIT_FAILURE;

	//Error checking
	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
		return EXIT_FAILURE;
	}
	// Create the mesh
	//UCreateMesh(gMesh); // Calls the function to create the Vertex Buffer Object
	meshes.CreateMeshes();

	// Create the shader program
	if (!UCreateShaderProgram(vertexShaderSource, fragmentShaderSource, gProgramId))
		return EXIT_FAILURE;

	glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(gWindow, UMouseCallback);

	// Register scroll callback
	glfwSetScrollCallback(gWindow, UScrollCallback);

	// Sets the background color of the window to black (it will be implicitely used by glClear)
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	// Load textures
	const char* texFilename = "checker.jpg";
	if (!UCreateTexture(texFilename, gTexture1Id))
	{
		cout << "Failed to load texture " << texFilename << endl;
		return EXIT_FAILURE;
	}

	texFilename = "metal.jpg";  // Use the metal texture
	if (!UCreateTexture(texFilename, gTexture2Id))
	{
		cout << "Failed to load texture " << texFilename << endl;
		return EXIT_FAILURE;
	}

	texFilename = "grid.png";  // Use the grid texture
	if (!UCreateTexture(texFilename, gTexture3Id))
	{
		cout << "Failed to load texture " << texFilename << endl;
		return EXIT_FAILURE;
	}

	texFilename = "sun.png"; // Use the sun texture
	if (!UCreateTexture(texFilename, gTexture4Id))
	{
		cout << "Failed to load texture " << texFilename << endl;
		return EXIT_FAILURE;
	}

	texFilename = "multi.png"; // Use the sun texture
	if (!UCreateTexture(texFilename, gTexture5Id))
	{
		cout << "Failed to load texture " << texFilename << endl;
		return EXIT_FAILURE;
	}

	// bind textures on corresponding texture units
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gTexture1Id);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gTexture2Id);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gTexture3Id);
	
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, gTexture4Id);

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, gTexture5Id);
	// render loop
	// -----------
	while (!glfwWindowShouldClose(gWindow)) {
		UProcessInput(gWindow);
		UUpdateCamera();
		URender();
		glfwPollEvents();
	}
	// Release mesh data
	//UDestroyMesh(gMesh);
	meshes.DestroyMeshes();

	// Release shader program
	UDestroyShaderProgram(gProgramId);
	glfwTerminate(); // Terminates GLFW before exiting
	exit(EXIT_SUCCESS); // Terminates the program successfully
}
void UMouseCallback(GLFWwindow* window, double xpos, double ypos) {
	if (firstMouse) {
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	if (!isOrthographic) {
		float xoffset = xpos - lastX;
		float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

		const float sensitivity = 0.1f;
		xoffset *= sensitivity;
		yoffset *= sensitivity;

		yaw += xoffset;
		pitch += yoffset;

		// Constrain pitch to avoid flipping
		if (pitch > 89.0f) pitch = 89.0f;
		if (pitch < -89.0f) pitch = -89.0f;

		glm::vec3 front;
		front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		front.y = sin(glm::radians(pitch));
		front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		cameraFront = glm::normalize(front);

		lastX = xpos;
		lastY = ypos;
	}
}

// Initialize GLFW, GLEW, and create a window
bool UInitialize(int argc, char* argv[], GLFWwindow** window)
{
	// GLFW: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// GLFW: window creation
	// ---------------------
	* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
	if (*window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(*window);
	glfwSetFramebufferSizeCallback(*window, UResizeWindow);

	// Enable capturing key events
	glfwSetInputMode(*window, GLFW_STICKY_KEYS, GL_TRUE);

	// GLEW: initialize
	// ----------------
	// Note: if using GLEW version 1.13 or earlier
	glewExperimental = GL_TRUE;
	GLenum GlewInitResult = glewInit();

	if (GLEW_OK != GlewInitResult)
	{
		std::cerr << glewGetErrorString(GlewInitResult) << std::endl;
		return false;
	}

	// Displays GPU OpenGL version
	cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << endl;

	return true;
}


// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void UProcessInput(GLFWwindow* window) {
	// Escape key: close the application
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	// Check if the view is orthographic
	if (!isOrthographic) {
		// WASD keys: move the camera
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			cameraPosition += movementSpeed * cameraFront;
			cout << "W key pressed - New Camera Position: " << cameraPosition.x << ", " << cameraPosition.y << ", " << cameraPosition.z << endl;
		}
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			cameraPosition -= movementSpeed * cameraFront;
			cout << "S key pressed - New Camera Position: " << cameraPosition.x << ", " << cameraPosition.y << ", " << cameraPosition.z << endl;
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			cameraPosition -= glm::normalize(glm::cross(cameraFront, cameraUp)) * movementSpeed;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			cameraPosition += glm::normalize(glm::cross(cameraFront, cameraUp)) * movementSpeed;

		// Arrow keys: move the camera
		if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
			cameraPosition += movementSpeed * cameraFront;
		if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
			cameraPosition -= movementSpeed * cameraFront;
		if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
			cameraPosition -= glm::normalize(glm::cross(cameraFront, cameraUp)) * movementSpeed;
		if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
			cameraPosition += glm::normalize(glm::cross(cameraFront, cameraUp)) * movementSpeed;

		// Q key: move camera up
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
			cameraPosition += movementSpeed * cameraUp;
		// E key: move camera down
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
			cameraPosition -= movementSpeed * cameraUp;

		// Page Up: move camera up
		if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)
			cameraPosition += movementSpeed * cameraUp;
		// Page Down: move camera down
		if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS)
			cameraPosition -= movementSpeed * cameraUp;
		// R key: reset camera to origin
		if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
			cameraPosition = glm::vec3(0.0f, 0.0f, 12.0f); // Reset to origin
			cout << "R key pressed - Camera Reset to Origin" << endl;
		}

	}
	// Toggle between orthographic and perspective views
	if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
		isOrthographic = false;
		cout << "Switched to Perspective View" << endl;
	}
	// O key: toggle to orthographic projection
	if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
		isOrthographic = true;

		// Set camera position for orthographic view
		cameraPosition = glm::vec3(0.0f, 0.0f, 12.0f);
		cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
		cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

	}

	// Update the view matrix
	UUpdateCamera();
}



// glfw: whenever the window size changed (by OS or user resize) this callback function executes
void UResizeWindow(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}


// Functioned called to render a frame
void URender()
{
	// Update the view matrix
	GLint modelLoc;
	GLint viewLoc;
	GLint projLoc;
	GLint viewPosLoc;
	GLint ambStrLoc;
	GLint ambColLoc;
	GLint light1ColLoc;
	GLint light1PosLoc;
	GLint light2ColLoc;
	GLint light2PosLoc;
	GLint objColLoc;
	GLint specInt1Loc;
	GLint highlghtSz1Loc;
	GLint specInt2Loc;
	GLint highlghtSz2Loc;
	GLint uHasTextureLoc;
	bool ubHasTextureVal;
	glm::mat4 scale;
	glm::mat4 rotation;
	glm::mat4 rotation1;
	glm::mat4 rotation2;
	glm::mat4 translation;
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
	// Enable z-depth
	glEnable(GL_DEPTH_TEST);

	// Clear the frame and z buffers
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	// Set the shader to be used
	glUseProgram(gProgramId);

	// Retrieves and passes transform matrices to the Shader program
	modelLoc = glGetUniformLocation(gProgramId, "model");
	viewLoc = glGetUniformLocation(gProgramId, "view");
	projLoc = glGetUniformLocation(gProgramId, "projection");
	viewPosLoc = glGetUniformLocation(gProgramId, "viewPosition");
	ambStrLoc = glGetUniformLocation(gProgramId, "ambientStrength");
	ambColLoc = glGetUniformLocation(gProgramId, "ambientColor");
	light1ColLoc = glGetUniformLocation(gProgramId, "light1Color");
	light1PosLoc = glGetUniformLocation(gProgramId, "light1Position");
	light2ColLoc = glGetUniformLocation(gProgramId, "light2Color");
	light2PosLoc = glGetUniformLocation(gProgramId, "light2Position");
	objColLoc = glGetUniformLocation(gProgramId, "objectColor");
	specInt1Loc = glGetUniformLocation(gProgramId, "specularIntensity1");
	highlghtSz1Loc = glGetUniformLocation(gProgramId, "highlightSize1");
	specInt2Loc = glGetUniformLocation(gProgramId, "specularIntensity2");
	highlghtSz2Loc = glGetUniformLocation(gProgramId, "highlightSize2");
	uHasTextureLoc = glGetUniformLocation(gProgramId, "ubHasTexture");

	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));


	//set ambient lighting strength
	glUniform1f(ambStrLoc, 0.4f);
	//set ambient color
	glUniform3f(ambColLoc, 0.2f, 0.2f, 0.2f);
	glUniform3f(light1ColLoc, 1.0f, 0.5f, 0.1f);
	glUniform3f(light1PosLoc, 8.0f, 3.0f, 2.0f);
	glUniform3f(light2ColLoc, 1.0f, 1.0f, 1.0f);
	glUniform3f(light2PosLoc, -2.0f, 3.0f, 2.0f);
	//set specular intensity
	glUniform1f(specInt1Loc, .6f);
	glUniform1f(specInt2Loc, .6f);
	//set specular highlight size
	glUniform1f(highlghtSz1Loc, 12.0f);
	glUniform1f(highlghtSz2Loc, 12.0f);
	ubHasTextureVal = true;
	glUniform1i(uHasTextureLoc, ubHasTextureVal);


	if (isOrthographic) {
		projection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 100.0f);
	}
	else {
		projection = glm::perspective(glm::radians(45.0f), (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);
	}

	projLoc = glGetUniformLocation(gProgramId, "projection");
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
	// Enable z-depth
	glEnable(GL_DEPTH_TEST);

	// Clear the frame and z buffers
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Transforms the camera
	view = glm::lookAt(cameraPosition, cameraPosition + cameraFront, cameraUp);

	// Creates a orthographic projection
	//projection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 100.0f);
	projection = glm::perspective(glm::radians(45.0f), (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);

	// Creates either orthographic or perspective projection
	if (isOrthographic) {
		projection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 100.0f);
	}
	else {
		projection = glm::perspective(glm::radians(45.0f), (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);
	}

	projLoc = glGetUniformLocation(gProgramId, "projection");
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gTexture3Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 2);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gPlaneMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(6.0f, 1.0f, 6.0f));
	// 2. Rotate the object
	rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(0.0f, 0.0f, 0.0f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	glProgramUniform4f(gProgramId, objColLoc, 1.0f, 0.0f, 0.0f, 1.0f);
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 2);
	// Draws the triangles
	glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);

	// Activate the VBOs contained within the mesh's VAO
	if (!isOrthographic) {
		// Rendering code for the plane
		glBindVertexArray(meshes.gPlaneMesh.vao);

		// 1. Scales the object
		scale = glm::scale(glm::vec3(6.0f, 1.0f, 6.0f));
		// 2. Rotate the object
		rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
		// 3. Position the object
		translation = glm::translate(glm::vec3(0.0f, 0.0f, 0.0f));
		// Model matrix: transformations are applied right-to-left order
		model = translation * rotation * scale;
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

		glProgramUniform4f(gProgramId, objColLoc, 1.0f, 0.0f, 0.0f, 1.0f);

		// Draws the triangles
		glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

		// Deactivate the Vertex Array Object
		glBindVertexArray(0);
	}

	// Activate the VBOs contained within the mesh's VAO
	//glBindVertexArray(meshes.gPyramid3Mesh.vao);

	// 1. Scales the object
	//scale = glm::scale(glm::vec3(1.0f, 1.0f, 1.0f));
	// 2. Rotate the object
	//rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
	// 3. Position the object
	//translation = glm::translate(glm::vec3(-2.0f, 1.0f, -3.0f));
	// Model matrix: transformations are applied right-to-left order
	//model = translation * rotation * scale;
	//glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	//glProgramUniform4f(gProgramId, objectColorLoc, 0.0f, 0.5f, 0.5f, 1.0f);

	// Draws the triangles
	//glDrawArrays(GL_TRIANGLE_STRIP, 0, meshes.gPyramid3Mesh.nVertices);

	// Deactivate the Vertex Array Object
	
	
	///////////////////////////////////////////////////////////////This is for the main cylinder
	
	
	//glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gTexture1Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gCylinderMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.5f, 2.0f, 0.5f));
	//(Width,Height,
	//original scale = glm::scale(glm::vec3(1.0f, 1.0f, 1.0f));
	// 2. Rotate the object
	rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(0.0f, 0.0f, 0.0f));
	//(x,y,z)
	// original translation was	translation = glm::translate(glm::vec3(-3.5f, 1.0f, 5.5f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	glProgramUniform4f(gProgramId, objColLoc, 1.0f, 1.0f, 0.0f, 1.0f);
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);
	// Draws the triangles
	glDrawArrays(GL_TRIANGLE_FAN, 0, 36);		//bottom
	glDrawArrays(GL_TRIANGLE_FAN, 36, 36);		//top
	glDrawArrays(GL_TRIANGLE_STRIP, 72, 146);	//sides

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);


	////////////////////////////////////////////////////////This is for the angled box


	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gTexture1Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);
// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gBoxMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(3.0f, 0.35f, 0.6f));
	// 2. Rotate the object
	rotation = glm::rotate(glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(1.7f, 1.2f, 0.0f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	glProgramUniform4f(gProgramId, objColLoc, 0.5f, 0.5f, 0.0f, 1.0f);

	// Draws the triangles
	glDrawElements(GL_TRIANGLES, meshes.gBoxMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);


	//////////////////////////////////////////////////////////////This is the Washer


	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gTexture1Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gCylinderMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.5f, 0.1f, 0.5f));
	//(Width,Height,
	//original scale = glm::scale(glm::vec3(1.0f, 1.0f, 1.0f));
	// 2. Rotate the object
	rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(-3.0f, 0.0f, 4.0f));
	//(x,y,z)
	// original translation was	translation = glm::translate(glm::vec3(-3.5f, 1.0f, 5.5f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	glProgramUniform4f(gProgramId, objColLoc, 1.0f, 1.0f, 0.0f, 1.0f);
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);
	// Draws the triangles
	glDrawArrays(GL_TRIANGLE_FAN, 0, 36);		//bottom
	glDrawArrays(GL_TRIANGLE_FAN, 36, 36);		//top
	glDrawArrays(GL_TRIANGLE_STRIP, 72, 146);	//sides

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);



	//////////////////////////////////////////////////////////////////This is the middle of the washer


// Activate the texture unit and bind the texture for the Torus
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gTexture2Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 1);
	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gTorusMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.2f, 0.2f, 0.5f));
	// 2. Rotate the object
	//rotation = glm::rotate(1.0f, glm::vec3(1.0, 1.0f, 1.0f));
	rotation = glm::rotate(glm::pi<float>() / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(-3.0f, 0.1f, 4.0f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	scale = glm::scale(glm::vec3(1.0f, 1.0f, 0.5f));

	glProgramUniform4f(gProgramId, objColLoc, 0.0f, 0.0f, 1.0f, 1.0f);
	// Draws the triangles
	glDrawArrays(GL_TRIANGLES, 0, meshes.gTorusMesh.nVertices);

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);





	///////////////////////////////////////////////////////////////This is for the first of the small buttons
	
	
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, gTexture5Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 4);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gCylinderMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.3f, 0.1f, 0.3f));
	//(Width,Height,
	//original scale = glm::scale(glm::vec3(1.0f, 1.0f, 1.0f));
	// 2. Rotate the object
	rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(0.0f, 0.0f, 1.5f));
	//(x,y,z)
	// original translation was	translation = glm::translate(glm::vec3(-3.5f, 1.0f, 5.5f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	glProgramUniform4f(gProgramId, objColLoc, 1.0f, 1.0f, 0.0f, 1.0f);
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 4);
	// Draws the triangles
	glDrawArrays(GL_TRIANGLE_FAN, 0, 36);		//bottom
	glDrawArrays(GL_TRIANGLE_FAN, 36, 36);		//top
	glDrawArrays(GL_TRIANGLE_STRIP, 72, 146);	//sides

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);


	///////////////////////////////////////////////////////////////This is for the second of the small buttons


	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, gTexture5Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 4);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gCylinderMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.3f, 0.1f, 0.3f));
	//(Width,Height,
	// 2. Rotate the object
	rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(-2.0f, 0.0f, -1.5f));
	//(x,y,z)
	// original translation was	translation = glm::translate(glm::vec3(-3.5f, 1.0f, 5.5f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	glProgramUniform4f(gProgramId, objColLoc, 1.0f, 1.0f, 0.0f, 1.0f);
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 4);
	// Draws the triangles
	glDrawArrays(GL_TRIANGLE_FAN, 0, 36);		//bottom
	glDrawArrays(GL_TRIANGLE_FAN, 36, 36);		//top
	glDrawArrays(GL_TRIANGLE_STRIP, 72, 146);	//sides

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);

	///////////////////////////////////////////////////////////////This is for the top of the angled screw


//glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gTexture1Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gCylinderMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.1f, -0.57f, 0.1f));
	//(Width,Height,
	// 2. Rotate the object
	rotation = glm::rotate(glm::radians(-60.0f), glm::vec3(-13.0f, 0.0f, 1.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(-2.0f, 0.37f, -1.7f));
	//(x,y,z)
	// original translation was	translation = glm::translate(glm::vec3(-3.5f, 1.0f, 5.5f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	glProgramUniform4f(gProgramId, objColLoc, 1.0f, 1.0f, 0.0f, 1.0f);
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);
	// Draws the triangles
	glDrawArrays(GL_TRIANGLE_FAN, 0, 36);		//bottom
	glDrawArrays(GL_TRIANGLE_FAN, 36, 36);		//top
	glDrawArrays(GL_TRIANGLE_STRIP, 72, 146);	//sides

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);


	///////////////////////////////////////////////////////////////This is for the angled of the screws


//glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gTexture1Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gCylinderMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.3f, 0.1f, 0.3f));
	//(Width,Height,
	// 2. Rotate the object
	rotation = glm::rotate(glm::radians(-60.0f), glm::vec3(-13.0f, 0.0f, 1.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(-2.0f, 0.37f, -1.7f));
	//(x,y,z)
	// original translation was	translation = glm::translate(glm::vec3(-3.5f, 1.0f, 5.5f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	glProgramUniform4f(gProgramId, objColLoc, 1.0f, 1.0f, 0.0f, 1.0f);
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);
	// Draws the triangles
	glDrawArrays(GL_TRIANGLE_FAN, 0, 36);		//bottom
	glDrawArrays(GL_TRIANGLE_FAN, 36, 36);		//top
	glDrawArrays(GL_TRIANGLE_STRIP, 72, 146);	//sides

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);


	//////////////////////////////////////////////////////////////////This is for the top of the cylinder


// Activate the texture unit and bind the texture for the Torus
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gTexture2Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 1);
	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gTorusMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.5f, 0.5f, 1.0f));
	// 2. Rotate the object
	//rotation = glm::rotate(1.0f, glm::vec3(1.0, 1.0f, 1.0f));
	rotation = glm::rotate(glm::pi<float>() / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(0.0f, 2.0f, 0.0f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	scale = glm::scale(glm::vec3(1.0f, 1.0f, 0.5f));

	glProgramUniform4f(gProgramId, objColLoc, 0.0f, 0.0f, 1.0f, 1.0f);
	// Draws the triangles
	glDrawArrays(GL_TRIANGLES, 0, meshes.gTorusMesh.nVertices);

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);

	///////////////////////////////////////////////////////////////This is for the bottom of the straight screw


//glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gTexture1Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gCylinderMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.3f, 0.1f, 0.3f));
	//(Width,Height,
	// 2. Rotate the object
	rotation = glm::rotate(glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 1.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(-2.8f, 0.0f, -3.7f));
	//(x,y,z)
	// original translation was	translation = glm::translate(glm::vec3(-3.5f, 1.0f, 5.5f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	glProgramUniform4f(gProgramId, objColLoc, 1.0f, 1.0f, 0.0f, 1.0f);
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);
	// Draws the triangles
	glDrawArrays(GL_TRIANGLE_FAN, 0, 36);		//bottom
	glDrawArrays(GL_TRIANGLE_FAN, 36, 36);		//top
	glDrawArrays(GL_TRIANGLE_STRIP, 72, 146);	//sides

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);


	///////////////////////////////////////////////////////////////This is for the straight screw


//glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gTexture1Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);

	// Activate the VBOs contained within the mesh's VAO
	glBindVertexArray(meshes.gCylinderMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(0.1f, -0.65f, 0.1f));
	//(Width,Height,
	// 2. Rotate the object
	rotation = glm::rotate(glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 1.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(-2.8f, 0.7f, -3.7f));
	//(x,y,z)
	// original translation was	translation = glm::translate(glm::vec3(-3.5f, 1.0f, 5.5f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	glProgramUniform4f(gProgramId, objColLoc, 1.0f, 1.0f, 0.0f, 1.0f);
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);
	// Draws the triangles
	glDrawArrays(GL_TRIANGLE_FAN, 0, 36);		//bottom
	glDrawArrays(GL_TRIANGLE_FAN, 36, 36);		//top
	glDrawArrays(GL_TRIANGLE_STRIP, 72, 146);	//sides

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);


	///////////////////////////////////////////////////////This is for the sun/light


	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, gTexture4Id);

	// Set the texture unit uniform in the shader
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 3);

	// Activate the VBOs contained within the mesh's VAO
	//glBindVertexArray(meshes.gSphereMesh.vao);

	// 1. Scales the object
	scale = glm::scale(glm::vec3(1.0f, 1.0f, 1.0f));
	// 2. Rotate the object
	rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
	// 3. Position the object
	translation = glm::translate(glm::vec3(10.0f, 6.0f, -4.0f));
	// Model matrix: transformations are applied right-to-left order
	model = translation * rotation * scale;
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

	glProgramUniform4f(gProgramId, objColLoc, 0.0f, 1.0f, 0.0f, 1.0f);

	// Draws the triangles
	glDrawElements(GL_TRIANGLES, meshes.gSphereMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

	// Deactivate the Vertex Array Object
	glBindVertexArray(0);

	// Unbind the texture
	glBindTexture(GL_TEXTURE_2D, 0);

	// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
	glfwSwapBuffers(gWindow);    // Flips the the back buffer with the front buffer every frame.
}

void UUpdateCamera() {
	glm::mat4 view = glm::lookAt(cameraPosition, cameraPosition + cameraFront, cameraUp);
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
}

// Implements the UCreateShaders function
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId)
{
	// Compilation and linkage error reporting
	int success = 0;
	char infoLog[512];

	// Create a Shader program object.
	programId = glCreateProgram();

	// Create the vertex and fragment shader objects
	GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

	// Retrive the shader source
	glShaderSource(vertexShaderId, 1, &vtxShaderSource, NULL);
	glShaderSource(fragmentShaderId, 1, &fragShaderSource, NULL);

	// Compile the vertex shader, and print compilation errors (if any)
	glCompileShader(vertexShaderId); // compile the vertex shader
	// check for shader compile errors
	glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShaderId, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;

		return false;
	}

	glCompileShader(fragmentShaderId); // compile the fragment shader
	// check for shader compile errors
	glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShaderId, sizeof(infoLog), NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;

		return false;
	}

	// Attached compiled shaders to the shader program
	glAttachShader(programId, vertexShaderId);
	glAttachShader(programId, fragmentShaderId);

	glLinkProgram(programId);   // links the shader program
	// Retrieve uniform locations after linking
	viewLoc = glGetUniformLocation(programId, "view");
	// check for linking errors
	glGetProgramiv(programId, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(programId, sizeof(infoLog), NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;

		return false;
	}

	glUseProgram(programId);    // Uses the shader program

	return true;
}


void UDestroyShaderProgram(GLuint programId)
{
	glDeleteProgram(programId);
}