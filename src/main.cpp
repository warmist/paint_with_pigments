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

void make_gui()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Begin("Window");
    ImGui::Text("Hello world");
    ImGui::End();
}
const int max_nm = 720;
const int min_nm = 380;
const int count_color_samples = (max_nm - min_nm) / 10;
void load_color_samples(GLuint texture,int count_colors)
{
	std::ifstream f("colors.txt");
	
	std::vector<float> buffer(count_color_samples);
	for(int j=0;j<count_colors;j++)
	{
		for (int i = 0; i < count_color_samples; i++)
		{
			f >> buffer[i];
			if (buffer[i] > 1)buffer[i] = 1;
			if (buffer[i] < 0)buffer[i] = 0;
		}
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
	const int count_colors = 3;
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

		//380-720 every 10 nm
		
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
		const unsigned pixel_size = sizeof(float);

		buffer.resize(screen_size*pixel_size);

		glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		//glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, (int)io.DisplaySize.x, (int)io.DisplaySize.y );
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R32F, w, h, count_colors, 0, GL_RED, GL_FLOAT, NULL);

		//GL_INVALID_ENUM
		// Because we're also using this tex as an image (in order to write to it),
		// we bind it to an image unit as well
		//glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

		/*
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

		glBindTexture(GL_TEXTURE_2D, framebuffer_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (int)io.DisplaySize.x, (int)io.DisplaySize.y, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer_tex, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		*/
	}
	void push_data()
	{
		glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
			0, 0, 0,
			w, h, count_colors,
			GL_RED, GL_FLOAT,
			buffer.data());
		
	}
};
void print_prog_status(GLuint id,GLint stat,const std::vector<char>& log)
{
	if (stat == GL_TRUE)
		std::cout << id << " is ok!";
	else
	{
		if (log.size() > 0)
			std::cout << "Log:" << log.data();
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
GLuint init_shader(GLuint& tex_uniform,GLuint& pigments_uniform)
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

	GLuint s_frag = make_shader(R"(
	//!program main fragment
	#version 330


float xFit_1931(  float wave )
{
	 float t1 = (wave-442.0f)*((wave<442.0f)?0.0624f:0.0374f);
	 float t2 = (wave-599.8f)*((wave<599.8f)?0.0264f:0.0323f);
	 float t3 = (wave-501.1f)*((wave<501.1f)?0.0490f:0.0382f);
	return 0.362f*exp(-0.5f*t1*t1) + 1.056f*exp(-0.5f*t2*t2)
	- 0.065f*exp(-0.5f*t3*t3);
}
float yFit_1931(  float wave )
{
	 float t1 = (wave-568.8f)*((wave<568.8f)?0.0213f:0.0247f);
	 float t2 = (wave-530.9f)*((wave<530.9f)?0.0613f:0.0322f);
	return 0.821f*exp(-0.5f*t1*t1) + 0.286f*exp(-0.5f*t2*t2);
}
float zFit_1931(  float wave )
{
	 float t1 = (wave-437.0f)*((wave<437.0f)?0.0845f:0.0278f);
	 float t2 = (wave-459.0f)*((wave<459.0f)?0.0385f:0.0725f);
	return 1.217f*exp(-0.5f*t1*t1) + 0.681f*exp(-0.5f*t2*t2);
}
vec3 fit_1931( float w)
{
	return vec3(xFit_1931(w),yFit_1931(w),zFit_1931(w));
}
float black_body_spectrum( float l, float temperature )
{
	/*float h=6.626070040e-34; //Planck constant
	float c=299792458; //Speed of light
	float k=1.38064852e-23; //Boltzmann constant
	*/
	 float const_1=5.955215e-17;//h*c*c
	 float const_2=0.0143878;//(h*c)/k
	 float top=(2*const_1);
	 float bottom=(exp((const_2)/(temperature*l))-1)*l*l*l*l*l;
	return top/bottom;
	//return (2*h*freq*freq*freq)/(c*c)*(1/(math.exp((h*freq)/(k*temperature))-1))
}

	out vec4 color;
	uniform sampler2DArray color_layers;
	uniform sampler1DArray pigments;
	varying vec3 pos;
	void main(){
		//color = vec4(texture(color_layers, vec3(pos.xy, 0)));
		color = vec4(texture(pigments, pos.xy*3));
	}
	)", GL_FRAGMENT_SHADER);
	// Check Shader
	//glGetShaderiv(s_id, GL_COMPILE_STATUS, &tmp_s.status.result);
	//get_shader_log(tmp_s, ctx);
	
	glAttachShader(prog_id, s_vert);
	glAttachShader(prog_id, s_frag);
	glLinkProgram(prog_id);

	tex_uniform = glGetUniformLocation(prog_id, "color_layers");
	pigments_uniform = glGetUniformLocation(prog_id, "pigments");

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

	glDebugMessageCallback(&dgb_callback, nullptr);
	glEnable(GL_DEBUG_OUTPUT);

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
	
	auto prog = init_shader(texture_loc, pigments_loc);
	
    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        
        ImGuiIO& io = ImGui::GetIO();
       
        glfwPollEvents();
       
        ImGui_ImplGlfwGL3_NewFrame();

        make_gui();
		pbuf.update_buffer_size(io);
		for (int i = 0; i < pbuf.buffer.size(); i++)
			pbuf.buffer[i] = 0.5;
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
