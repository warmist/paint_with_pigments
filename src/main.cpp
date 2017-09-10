#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "imgui_impl_glfw_gl3.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error %d: %s\n", error, description);
}
const int max_nm = 730;
const int min_nm = 380;
const int count_color_samples = (max_nm - min_nm) / 10+1;
void load_color_samples(GLuint texture,int count_colors)
{
	std::ifstream f("colors.txt");
	
	std::vector<float> buffer(count_color_samples);
	for(int j=0;j<count_colors;j++)
	{
		std::cout << "COLOR:" << j << std::endl;
		for (int i = 0; i < count_color_samples; i++)
		{
			float v;
			f >> v;
			if (v > 1)v = 1;
			if (v < 0)v = 0;
			buffer[i] = v;
			std::cout << v << " ";
		}
		std::cout << std::endl;
		glTexSubImage2D(GL_TEXTURE_1D_ARRAY, 0,
			0, j, 
			count_color_samples, 1,
			GL_RED, GL_FLOAT,
			buffer.data());
	}
}
struct pixel_buffer
{

	GLuint texture;
	GLsizeiptr screen_size = 0;

	int index = 0;
	int next_index = 1;
	const int count_colors = 5;
	std::vector<float> buffer;
	int w, h;

	GLuint texture_colors;//samples of paint reflectance

	pixel_buffer()
	{
		glGenTextures(1, &texture);
		glGenTextures(1, &texture_colors);

		
		glBindTexture(GL_TEXTURE_1D_ARRAY, texture_colors);
		glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		//380-730 every 10 nm
		
		glTexImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_R32F, count_color_samples, count_colors, 0, GL_RED, GL_FLOAT, NULL);
		load_color_samples(texture_colors,count_colors);
	}
	void update_buffer_size(ImGuiIO& io)
	{
		if (io.DisplaySize.x < 0)
			return;
		w = io.DisplaySize.x;
		h = io.DisplaySize.y;
		if (screen_size == w*h)
			return;
		screen_size = GLsizeiptr(w*h);

		buffer.resize(screen_size*count_colors);

		glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R32F, w, h, count_colors, 0, GL_RED, GL_FLOAT, NULL);
	}
	void set_pixel(int x, int y, int d,float v)
	{
		buffer[x + (h-y)*w+d*w*h] = v;
	}
	float& pixel(int x, int y, int d)
	{
		return buffer[x + (h - y)*w + d*w*h];
	}
	void push_data()
	{
		glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
		//for(int i=0;i<count_colors;i++)
		//{
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
				0, 0, 0,
				w, h, count_colors,
				GL_RED, GL_FLOAT,
				buffer.data());
		//}
		
	}
};
void print_prog_status(GLuint id,GLint stat,const std::vector<char>& log)
{
	if (stat == GL_TRUE)
		std::cout << id << " is ok!"<<std::endl;
	else
	{
		if (log.size() > 0)
			std::cout << "Log:" << log.data() << std::endl;
	}
}
void get_shader_log(GLuint id)
{
	GLint status;
	glGetShaderiv(id, GL_COMPILE_STATUS, &status);
	int InfoLogLength;
	glGetShaderiv(id, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> log;
	log.resize(InfoLogLength + 1);

	glGetShaderInfoLog(id, InfoLogLength, NULL, &log[0]);
	print_prog_status(id, status, log);
}
GLuint make_shader(std::string text, GLuint type)
{
	GLuint ret = glCreateShader(type);

	const char *p = text.c_str();
	const GLint l = text.length();

	glShaderSource(ret, 1, &p, &l);
	glCompileShader(ret);

	get_shader_log(ret);
	return ret;
}
GLuint init_shader(GLuint& tex_uniform,GLuint& pigments_uniform,GLuint& power_uniform)
{
	GLuint prog_id = glCreateProgram();
	
	GLuint s_vert = make_shader(
	R"(
	//!program main vertex
	#version 330

	layout(location = 0) in vec3 vertexPosition_modelspace;

	varying vec3 pos;
	void main()
	{
		gl_Position.xyz = vertexPosition_modelspace;
		gl_Position.w = 1.0;
		pos=vertexPosition_modelspace;
	}
	)", GL_VERTEX_SHADER);

	std::ifstream shade_f("shader.glsl");
	std::string shade_text;
	std::getline(shade_f, shade_text, '\0');
	GLuint s_frag = make_shader(shade_text, GL_FRAGMENT_SHADER);
	// Check Shader
	//glGetShaderiv(s_id, GL_COMPILE_STATUS, &tmp_s.status.result);
	//get_shader_log(tmp_s, ctx);
	
	glAttachShader(prog_id, s_vert);
	glAttachShader(prog_id, s_frag);
	glLinkProgram(prog_id);

	tex_uniform = glGetUniformLocation(prog_id, "color_layers");
	pigments_uniform = glGetUniformLocation(prog_id, "pigments");
	power_uniform = glGetUniformLocation(prog_id, "power");

	return prog_id;
}
void APIENTRY dgb_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, GLvoid *userParam)
{
	if(severity== GL_DEBUG_SEVERITY_MEDIUM || severity== GL_DEBUG_SEVERITY_HIGH)
	{
		//__debugbreak();
		std::cout << message << std::endl;
	}
}
int main(int, char**)
{
    // Setup window
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Painting", NULL, NULL);
    glfwMakeContextCurrent(window);
    
    gl3wInit();
#ifdef DEBUG_GL
	glDebugMessageCallback(&dgb_callback, nullptr);
	glEnable(GL_DEBUG_OUTPUT);
#endif
    // Setup ImGui binding
    ImGui_ImplGlfwGL3_Init(window, true);

    ImVec4 clear_color = ImColor(114, 144, 154);
	pixel_buffer pbuf;

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);
	static const GLfloat g_vertex_buffer_data[] = {

		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
	};
	// This will identify our vertex buffer
	GLuint vertexbuffer;

	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);
	GLuint texture_loc;
	GLuint pigments_loc;
	GLuint power_loc;
	auto prog = init_shader(texture_loc, pigments_loc,power_loc);
	
	float power = 0.529;
	int color_id = 0;
	const int rad = 10;
    // Main loop
	while (!glfwWindowShouldClose(window))
	{

		ImGuiIO& io = ImGui::GetIO();

		glfwPollEvents();

		ImGui_ImplGlfwGL3_NewFrame();

		ImGui::Begin("Settings");
		ImGui::SliderFloat("Power", &power, 0, 1);
		ImGui::SliderInt("Color", &color_id, 0, pbuf.count_colors-1);
		ImGui::End();
		pbuf.update_buffer_size(io);

		if (io.MouseWheel)
		{
			color_id += io.MouseWheel;
			if (color_id >= pbuf.count_colors)
				color_id = 0;
			if (color_id < 0)
				color_id = pbuf.count_colors - 1;
		}
		if( (io.MouseDown[0] && !io.MouseDownOwned[0]) || (io.MouseDown[1] && !io.MouseDownOwned[1]))
		{
			float dir = 1;
			if (io.MouseDown[1])
				dir = -1;

			for(int y=-rad;y<rad;y++)
			for (int x = -rad; x < rad; x++)
			{
				if(x*x+y*y<rad*rad)
				{
					int tx = io.MousePos.x + x;
					int ty = io.MousePos.y + y;
					if (tx >= 0 && ty >= 0 && tx < pbuf.w && ty < pbuf.h)
					{
						auto& f = pbuf.pixel(tx, ty, color_id);
						f += 0.1*dir;
						if (f < 0)f = 0;
					}
				}
			}
		}
		pbuf.push_data();
        // Rendering
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glBindVertexArray(VertexArrayID);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(
			0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);
		glUseProgram(prog);
		glActiveTexture(GL_TEXTURE0 + 0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, pbuf.texture);
		glUniform1i(texture_loc, 0);
		glActiveTexture(GL_TEXTURE0 + 1);
		glBindTexture(GL_TEXTURE_1D_ARRAY, pbuf.texture_colors);
		glUniform1i(pigments_loc, 1);
		glUniform1f(power_loc, power);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		
		glDisableVertexAttribArray(0);
		glUseProgram(0);
        ImGui::Render();


        glfwSwapBuffers(window);
        
    }

    // Cleanup
    ImGui_ImplGlfwGL3_Shutdown();
    glfwTerminate();

    return 0;
}
