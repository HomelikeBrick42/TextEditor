#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>

const char* VertexShaderSource = R"##(
#version 440 core

layout(location = 0) in vec4 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 v_TexCoord;

uniform mat4 u_ProjectionMatrix = mat4(1.0);
uniform float u_Scale = 1.0;
uniform vec2 u_Position = vec2(0.0);
uniform vec2 u_Offset = vec2(0.0);
uniform int u_Character = 0;

void main() {
	v_TexCoord = a_TexCoord;
	v_TexCoord.y += u_Character;
	v_TexCoord.y /= 256.0;

	vec4 position = a_Position;
	position.xy += u_Position;
	position.xy *= u_Scale;
	position.xy += u_Offset;
	gl_Position = u_ProjectionMatrix * position;
}
)##";

const char* FragmentShaderSource = R"##(
#version 440 core

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main() {
	o_Color = texture(u_Texture, v_TexCoord);
}
)##";

int Width = 600;
int Height = 600;
GLfloat ProjectionMatrix[4 * 4] = {};

constexpr size_t FontSizeX = 8;
constexpr size_t FontSizeY = 16;

size_t CursorPos = 0;
std::vector<char> Message;

int OffsetX = 0;
int OffsetY = 0;

int main(int argc, char** argv) {
	if (!glfwInit()) {
		return -1;
	}

	GLFWwindow* window = glfwCreateWindow(Width, Height, "Text Editor", nullptr, nullptr);
	if (!window) {
		return -1;
	}

	glfwMakeContextCurrent(window);

	if (!gladLoadGL(glfwGetProcAddress)) {
		return -1;
	}

	glfwSwapInterval(1);

	{
		auto SizeCallback = [](GLFWwindow* window, int width, int height) -> void {
			glViewport(0, 0, width, height);

			GLfloat left = 0;
			GLfloat right = (GLfloat)width;
			GLfloat top = 0;
			GLfloat bottom = (GLfloat)height;
			GLfloat near = -1.0f;
			GLfloat far = 1.0f;

			std::memset(ProjectionMatrix, 0, sizeof(ProjectionMatrix));

			ProjectionMatrix[0 * 4 + 0] = 2.0f / (right - left);
			ProjectionMatrix[1 * 4 + 1] = 2.0f / (top - bottom);
			ProjectionMatrix[2 * 4 + 2] = 2.0f / (far - near);
			ProjectionMatrix[3 * 4 + 3] = 1.0f;

			ProjectionMatrix[3 * 4 + 0] = -(right + left) / (right - left);
			ProjectionMatrix[3 * 4 + 1] = -(top + bottom) / (top - bottom);
			ProjectionMatrix[3 * 4 + 2] = -(far + near) / (far - near);

			Width = width;
			Height = height;
		};
		SizeCallback(window, Width, Height);
		glfwSetWindowSizeCallback(window, SizeCallback);
	}

	glfwSetCharCallback(window, [](GLFWwindow* window, unsigned int chr) -> void {
		Message.insert(Message.begin() + CursorPos, (char)chr);
		CursorPos++;
	});

	glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scanCode, int action, int mods) -> void {
		if (action == GLFW_PRESS || action == GLFW_REPEAT) {
			switch (key) {
				case GLFW_KEY_ENTER: {
					Message.insert(Message.begin() + CursorPos, '\n');
					CursorPos++;
				} break;

				case GLFW_KEY_BACKSPACE: {
					if (CursorPos > 0) {
						CursorPos--;
						Message.erase(Message.begin() + CursorPos);
					}
				} break;

				case GLFW_KEY_DELETE: {
					if (CursorPos < Message.size()) {
						Message.erase(Message.begin() + CursorPos);
					}
				} break;

				case GLFW_KEY_LEFT: {
					if (CursorPos > 0) {
						CursorPos--;
					}
				} break;

				case GLFW_KEY_RIGHT: {
					if (CursorPos < Message.size()) {
						CursorPos++;
					}
				} break;

				default: {
				} break;
			}
		}
	});

	glfwSetScrollCallback(window, [](GLFWwindow*, double xOffset, double yOffset) -> void {
		OffsetY += (int)(yOffset * 50.0);
	});

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(
		[](
			GLenum source,
			GLenum type,
			GLuint id,
			GLenum severity,
			GLsizei length,
			const GLchar* message,
			const void* userParam
		) {
			std::cout << "OpenGL Message: " << message << std::endl;
		}, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

	glEnable(GL_DEPTH_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	auto CreateShader = [](const char* vertexSource, const char* fragmentSource) -> GLuint {
		auto CreateShader = [](GLenum type, const char* source) -> GLuint {
			GLuint shader = glCreateShader(type);
			glShaderSource(shader, 1, &source, nullptr);
			glCompileShader(shader);

			// TODO: Errors

			return shader;
		};

		GLuint vertexShader = CreateShader(GL_VERTEX_SHADER, vertexSource);
		GLuint fragmentShader = CreateShader(GL_FRAGMENT_SHADER, fragmentSource);

		GLuint program = glCreateProgram();
		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);
		glLinkProgram(program);

		// TODO: Errors

		glDetachShader(program, vertexShader);
		glDeleteShader(vertexShader);

		glDetachShader(program, fragmentShader);
		glDeleteShader(fragmentShader);

		return program;
	};

	GLuint shader = CreateShader(VertexShaderSource, FragmentShaderSource);

	GLuint quadVertexArray;
	glGenVertexArrays(1, &quadVertexArray);
	glBindVertexArray(quadVertexArray);

	auto CreateBuffer = [](GLenum type, GLenum usage, const void* data, size_t size) -> GLuint {
		GLuint buffer;
		glGenBuffers(1, &buffer);
		glBindBuffer(type, buffer);
		glBufferData(type, size, data, usage);
		return buffer;
	};

	struct Vertex {
		GLint Position[2];
		GLubyte TexCoord[2];
	} quadVertices[] = { // TODO: Replace position with integers
		{ { 0, 16 }, { 0, 1 } },
		{ { 8, 16 }, { 1, 1 } },
		{ { 8, 0  }, { 1, 0 } },
		{ { 0, 0  }, { 0, 0 } },
	};
	GLuint quadVertexBuffer = CreateBuffer(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, quadVertices, sizeof(quadVertices));

	GLuint quadIndices[] = {
		0, 1, 2,
		0, 2, 3,
	};
	GLuint quadIndexBuffer = CreateBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, quadIndices, sizeof(quadIndices));

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_INT, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, Position));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(Vertex), (const void*)offsetof(Vertex, TexCoord));

	GLuint characterTextureAtlas = [](const char* filePath) -> GLuint {
		std::ifstream fontFileStream(filePath, std::ios::binary);
		assert(fontFileStream.is_open());

		std::vector<char> fontFileBytes(std::istreambuf_iterator<char>(fontFileStream), {});

		struct PSF1Font {
			char MagicBytes[2];
			char Mode;
			char CharSize;
			char GlyphBuffer[256 * FontSizeY];
		};

		PSF1Font* font = (PSF1Font*)fontFileBytes.data();

		assert(font->MagicBytes[0] == 0x36);
		assert(font->MagicBytes[1] == 0x04);
		assert(font->CharSize == FontSizeY);
		assert(font->Mode == 2);

		size_t width = FontSizeX;
		size_t height = FontSizeY * 256;

		unsigned char* pixels = new unsigned char[width * height * 4];
		size_t pixelIndex = 0;

		for (size_t i = 0; i < 256; i++) {
			char* chr = &font->GlyphBuffer[i * FontSizeY];
			for (size_t y = 0; y < FontSizeY; y++) {
				for (size_t x = 0; x < FontSizeX; x++) {
					if ((*chr & (0b10000000 >> x)) > 0) {
						pixels[pixelIndex++] = 255; // R
						pixels[pixelIndex++] = 255; // G
						pixels[pixelIndex++] = 255; // B
						pixels[pixelIndex++] = 255; // A
					}
					else {
						pixels[pixelIndex++] = 0; // R
						pixels[pixelIndex++] = 0; // G
						pixels[pixelIndex++] = 0; // B
						pixels[pixelIndex++] = 0; // A
					}
				}
				chr++;
			}
		}

		GLuint texture;
		glCreateTextures(GL_TEXTURE_2D, 1, &texture);
		glTextureStorage2D(texture, 1, GL_RGBA8, (GLsizei)width, (GLsizei)height);

		glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glTextureSubImage2D(texture, 0, 0, 0, (GLsizei)width, (GLsizei)height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		delete[] pixels;
		return texture;
	}("./fonts/zap-vga16.psf");

	glProgramUniform1i(shader, glGetUniformLocation(shader, "u_Texture"), 0);
	glProgramUniform1f(shader, glGetUniformLocation(shader, "u_Scale"), 2.0f);

	while (!glfwWindowShouldClose(window)) {
		glClearColor(0.1f, 0.0f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(shader);
		glBindTextureUnit(0, characterTextureAtlas);
		glProgramUniformMatrix4fv(shader, glGetUniformLocation(shader, "u_ProjectionMatrix"), 1, GL_FALSE, ProjectionMatrix);
		glProgramUniform2f(shader, glGetUniformLocation(shader, "u_Offset"), (GLfloat)OffsetX, (GLfloat)OffsetY);

		glBindVertexArray(quadVertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, quadVertexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIndexBuffer);

		GLint x = 0;
		GLint y = 0;
		for (auto& chr : Message) {
			switch (chr) {
				case '\n': {
					x = 0;
					y += FontSizeY;
				} break;

				case '\r': {
				} break;

				case '\t': {
					glProgramUniform1i(shader, glGetUniformLocation(shader, "u_Character"), ' ');
					for (size_t i = 0; i < 4; i++) {
						glProgramUniform2f(shader, glGetUniformLocation(shader, "u_Position"), (GLfloat)x, (GLfloat)y);
						glDrawElements(GL_TRIANGLES, sizeof(quadIndices) / sizeof(GLuint), GL_UNSIGNED_INT, nullptr);
						x += FontSizeX;
					}
				} break;

				default: {
					glProgramUniform1i(shader, glGetUniformLocation(shader, "u_Character"), chr);
					glProgramUniform2f(shader, glGetUniformLocation(shader, "u_Position"), (GLfloat)x, (GLfloat)y);
					glDrawElements(GL_TRIANGLES, sizeof(quadIndices) / sizeof(GLuint), GL_UNSIGNED_INT, nullptr);
					x += FontSizeX;
				} break;
			}
		}

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteTextures(1, &characterTextureAtlas);
	glDeleteVertexArrays(1, &quadVertexArray);
	glDeleteBuffers(1, &quadVertexBuffer);
	glDeleteBuffers(1, &quadIndexBuffer);
	glDeleteProgram(shader);

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
