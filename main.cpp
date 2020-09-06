#define GL_GLEXT_PROTOTYPES 1
#define GL3_PROTOTYPES      1

#include <GLFW/glfw3.h>
#include <array>
#include <string>
#include <sstream>
#include <fstream>

#define WIDTH 640
#define HEIGHT 480

static const char* quad_shader_vs = R"(
#version 460

layout(location = 0) in vec2 vPos;

out vec2 uv;

void main()
{
    gl_Position = vec4(vPos, 0, 1);
    uv = (vPos + vec2(1)) * 0.5f;
}
)";

static const char* quad_shader_fs = R"(
#version 460

layout(location = 0) uniform sampler2D tex;
layout(location = 1) uniform float time;
in vec2 uv;
out vec4 color;

vec3 fromColorTemperature(float kelvin) {
    float temp = kelvin * 0.01f;
    float r,g,b;
    if (temp <= 66) {
        r = 255;
        g = 99.4708025861 * log(temp) - 161.1195681661;
        b = temp <= 19 ? 0 : 138.5177312231 * log(temp-10) - 305.0447927307;
    }
    else {
        r = 329.698727446 * pow(temp-60, -0.1332047592);
        g = 288.1221695283 * pow(temp-60, -0.0755148492);
        b = 255;
    }

    return vec3(
            clamp(r/255.0f, 0, 1),
            clamp(g/255.0f, 0, 1),
            clamp(b/255.0f, 0, 1)
    );
}

void main()
{
    color = texture(tex, uv);
    color = vec4(fromColorTemperature(pow(color.r,6)*7000),0) * color.r;
}
)";

static const char* cs_fire = R"(
#version 460

layout(local_size_x = 32, local_size_y = 32) in;

layout(location = 0) uniform float time;
layout(location = 1) uniform ivec2 size;

layout(binding = 0, rgba32f) uniform image2D sourceTex;
layout(binding = 1, rgba32f) uniform image2D destTex;

//	Classic Perlin 3D Noise
//	by Stefan Gustavson
//
vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}
vec3 fade(vec3 t) {return t*t*t*(t*(t*6.0-15.0)+10.0);}

float cnoise(vec3 P){
  vec3 Pi0 = floor(P); // Integer part for indexing
  vec3 Pi1 = Pi0 + vec3(1.0); // Integer part + 1
  Pi0 = mod(Pi0, 289.0);
  Pi1 = mod(Pi1, 289.0);
  vec3 Pf0 = fract(P); // Fractional part for interpolation
  vec3 Pf1 = Pf0 - vec3(1.0); // Fractional part - 1.0
  vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
  vec4 iy = vec4(Pi0.yy, Pi1.yy);
  vec4 iz0 = Pi0.zzzz;
  vec4 iz1 = Pi1.zzzz;

  vec4 ixy = permute(permute(ix) + iy);
  vec4 ixy0 = permute(ixy + iz0);
  vec4 ixy1 = permute(ixy + iz1);

  vec4 gx0 = ixy0 / 7.0;
  vec4 gy0 = fract(floor(gx0) / 7.0) - 0.5;
  gx0 = fract(gx0);
  vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
  vec4 sz0 = step(gz0, vec4(0.0));
  gx0 -= sz0 * (step(0.0, gx0) - 0.5);
  gy0 -= sz0 * (step(0.0, gy0) - 0.5);

  vec4 gx1 = ixy1 / 7.0;
  vec4 gy1 = fract(floor(gx1) / 7.0) - 0.5;
  gx1 = fract(gx1);
  vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
  vec4 sz1 = step(gz1, vec4(0.0));
  gx1 -= sz1 * (step(0.0, gx1) - 0.5);
  gy1 -= sz1 * (step(0.0, gy1) - 0.5);

  vec3 g000 = vec3(gx0.x,gy0.x,gz0.x);
  vec3 g100 = vec3(gx0.y,gy0.y,gz0.y);
  vec3 g010 = vec3(gx0.z,gy0.z,gz0.z);
  vec3 g110 = vec3(gx0.w,gy0.w,gz0.w);
  vec3 g001 = vec3(gx1.x,gy1.x,gz1.x);
  vec3 g101 = vec3(gx1.y,gy1.y,gz1.y);
  vec3 g011 = vec3(gx1.z,gy1.z,gz1.z);
  vec3 g111 = vec3(gx1.w,gy1.w,gz1.w);

  vec4 norm0 = taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
  g000 *= norm0.x;
  g010 *= norm0.y;
  g100 *= norm0.z;
  g110 *= norm0.w;
  vec4 norm1 = taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
  g001 *= norm1.x;
  g011 *= norm1.y;
  g101 *= norm1.z;
  g111 *= norm1.w;

  float n000 = dot(g000, Pf0);
  float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));
  float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
  float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
  float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
  float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
  float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
  float n111 = dot(g111, Pf1);

  vec3 fade_xyz = fade(Pf0);
  vec4 n_z = mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_xyz.z);
  vec2 n_yz = mix(n_z.xy, n_z.zw, fade_xyz.y);
  float n_xyz = mix(n_yz.x, n_yz.y, fade_xyz.x);
  return 2.2 * n_xyz;
}


void main()
{
    ivec2 storePos = ivec2(gl_GlobalInvocationID.xy);
    vec2 storePosf = vec2(storePos) / vec2(size);
    if (storePos.x < 1 || storePos.x >= size.x - 1) return;
    if (storePos.y < 1 || storePos.y >= size.y - 1) return;


    vec4 c0 = imageLoad(sourceTex, storePos + ivec2(1, 0));
    vec4 c1 = imageLoad(sourceTex, storePos + ivec2(-1,0));
    vec4 c2 = imageLoad(sourceTex, storePos + ivec2(0, 1));
    vec4 c3 = imageLoad(sourceTex, storePos + ivec2(0,-1));
    vec4 newCol = (c0+c1+c2+c3) * 0.25;
    vec3 noisePoint = vec3(storePosf*15 - vec2(0, 8*time), time);
    float stride = cnoise(noisePoint+vec3(time));
    float cooling = 0.4 * abs(cnoise(noisePoint)) * storePosf.y;
    newCol -= vec4(cooling);
    if (storePos.y < 8) newCol = vec4(1);
    imageStore(destTex, storePos + ivec2(stride*3,2), newCol);
}

)";

inline static GLuint CompileShader(GLint type, const char* source)
{
  // Preprocess macro's
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  // Check the compilation of the shader 
  GLint success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

  if (success == GL_FALSE)
  {
    GLint maxLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

    GLchar* errorLog = (GLchar*)malloc(maxLength);
    glGetShaderInfoLog(shader, maxLength, &maxLength, errorLog);

    printf("Error in shader: %s", errorLog);

    free(errorLog);
    return -1;
  }

  return shader;
};

inline static GLuint GenerateProgram(GLuint cs)
{
  GLuint program = glCreateProgram();
  glAttachShader(program, cs);
  glLinkProgram(program);
  GLint isLinked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
  if (!isLinked)
  {
    GLint maxLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
    GLchar* errorLog = (GLchar*)malloc(maxLength);
    glGetProgramInfoLog(program, maxLength, &maxLength, errorLog);

    printf("Shader linker error: %s", errorLog);

    glDeleteProgram(program);
    exit(5);
  }
  return program;
}

inline static GLuint GenerateProgram(GLuint vs, GLuint fs)
{
  GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  GLint isLinked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
  if (!isLinked)
  {
    GLint maxLength = 0;  
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
    GLchar* errorLog = (GLchar*)malloc(maxLength);
    glGetProgramInfoLog(program, maxLength, &maxLength, errorLog);

    printf("Shader linker error: %s", errorLog);

    glDeleteProgram(program);
    exit(5);
  }
  return program;
}

void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

int main(int argc, char** argv) {
    if (!glfwInit()) return -2;

    glfwSetErrorCallback(error_callback);
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "fire", nullptr, nullptr);
    if (!window) return -3;

    glfwMakeContextCurrent(window);
    glDisable(GL_DEPTH_TEST);
    glfwSwapInterval(0);

    GLuint quad_vs = CompileShader(GL_VERTEX_SHADER, quad_shader_vs);
    GLuint quad_fs = CompileShader(GL_FRAGMENT_SHADER, quad_shader_fs);
    GLuint quad_shader = GenerateProgram(quad_vs, quad_fs);
    GLuint fire_program = GenerateProgram(CompileShader(GL_COMPUTE_SHADER, cs_fire));

    glUseProgram(quad_shader);

    float quad_data[12] = {
            -1.0, -1.0,
            1.0, -1.0,
            -1.0, 1.0,

            1.0, 1.0,
            -1.0, 1.0,
            1.0, -1.0,
    };

    GLuint quad_vao, quad_vbo;
    glGenVertexArrays(1, &quad_vao);
    glGenBuffers(1, &quad_vbo);
    glBindVertexArray(quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_data), quad_data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, false, 0, nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);


    GLuint buf[2];
    for(unsigned int & i : buf)
    {
        glGenTextures(1, &i);
        glBindTexture(GL_TEXTURE_2D, i);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTextureParameteri(i, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(i, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }


    int tick = 0;
    while(!glfwWindowShouldClose(window))
    {
        double ping = glfwGetTime();
        glUseProgram(fire_program);
        glUniform1f(0, glfwGetTime());
        glUniform2i(1, WIDTH, HEIGHT);
        glBindImageTexture(0, buf[tick%2], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
        glBindImageTexture(1, buf[(tick+1)%2], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glDispatchCompute(WIDTH/32+1, HEIGHT/32+1, 1);

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(quad_shader);
        glUniform1f(1, glfwGetTime());
        glBindTexture(GL_TEXTURE_2D, buf[tick%2]);
        glBindVertexArray(quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glfwPollEvents();
        glfwSwapBuffers(window);
        tick++;
        while(glfwGetTime() - ping < 1.0 / 60) {}
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
