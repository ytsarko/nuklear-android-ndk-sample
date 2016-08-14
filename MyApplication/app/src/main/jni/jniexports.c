#include <stdlib.h>
#include <assert.h>
#include <jni.h>
#include <android/log.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>

#define _CHECK_GL_IMPL(line)                        \
    {                                               \
        int ret;                                    \
        while ((ret = glGetError()) != GL_NO_ERROR) \
        {                                           \
            __android_log_print(ANDROID_LOG_DEBUG, "[CHECK_GL]", \
                "%d: glGetError(): %#x\n",          \
                (line), ret);                       \
        }                                           \
    }

#define CHECK_GL _CHECK_GL_IMPL(__LINE__)

static int width;
static int height;

static GLuint vbo, vao, ebo;
static GLuint prog;
static GLuint vert_shdr;
static GLuint frag_shdr;
static GLint attrib_pos;
static GLint attrib_uv;
static GLint attrib_col;
static GLint uniform_tex;
static GLint uniform_proj;

typedef void ( *__GLXextFuncPtr)(void);

#define GL_EXT(name) (pfn##name)gl_ext(#name)

typedef void (*pfnglGenVertexArrays)(GLsizei, GLuint*);
typedef void (*pfnglBindVertexArray)(GLuint);
typedef void (*pfnglDeleteVertexArrays)(GLsizei, const GLuint*);
typedef void (*pfnglGenVertexArraysOES)(GLsizei, GLuint*);
typedef void (*pfnglBindVertexArrayOES)(GLuint);
typedef void (*pfnglDeleteVertexArraysOES)(GLsizei, const GLuint*);

static pfnglGenVertexArrays glGenVertexArrays;
static pfnglBindVertexArray glBindVertexArray;
static pfnglDeleteVertexArrays glDeleteVertexArrays;
static pfnglGenVertexArraysOES glGenVertexArraysOES;
static pfnglBindVertexArrayOES glBindVertexArrayOES;
static pfnglDeleteVertexArraysOES glDeleteVertexArraysOES;

struct vec2 {float x,y;};

struct vec2
vec2(float x, float y)
{
    struct vec2 ret;
    ret.x = x; ret.y = y;
    return ret;
}

static
__GLXextFuncPtr gl_ext(const char *name)
{
    __GLXextFuncPtr func;
    func = eglGetProcAddress(name);
    if (!func)
    {
        __android_log_print(ANDROID_LOG_INFO, "native", "%s:%d: %s %s", __FUNCTION__, __LINE__,
            "failed to load extension ", name);
        return NULL;
    }
    return func;
}

JNIEXPORT void JNICALL Java_com_example_yt_myapplication_LibraryClass_init(JNIEnv *env, jobject thiz) 
{
    __android_log_print(ANDROID_LOG_INFO, "native", "%s:%d", __FUNCTION__, __LINE__);

    glGenVertexArrays = GL_EXT(glGenVertexArraysOES);
    glBindVertexArray = GL_EXT(glBindVertexArrayOES);
    glDeleteVertexArrays = GL_EXT(glDeleteVertexArraysOES);

    assert(glGenVertexArrays && glBindVertexArray && glDeleteVertexArrays);

    GLint status;
    GLuint vertex_index = 0;

    static const GLchar *vertex_shader =
        "uniform mat4 ProjMtx;\n"
        "attribute vec2 Position;\n"
        "attribute vec2 TexCoord;\n"
        "attribute vec4 Color;\n"
        "varying vec4 Frag_Color;\n"
        "void main() {\n"
        "   Frag_Color = Color;\n"
        "   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
        "}\n";
    static const GLchar *fragment_shader =
        "precision mediump float;\n"
        "uniform sampler2D Texture;\n"
        "varying vec4 Frag_Color;\n"
        "void main() {\n"
        "   gl_FragColor = Frag_Color;\n"
        "}\n";

    prog = glCreateProgram(); CHECK_GL;
    vert_shdr = glCreateShader(GL_VERTEX_SHADER);
    frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vert_shdr, 1, &vertex_shader, 0);
    glShaderSource(frag_shdr, 1, &fragment_shader, 0);
    glCompileShader(vert_shdr);
    glCompileShader(frag_shdr);
    glGetShaderiv(vert_shdr, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint logSize = 0;
        glGetShaderiv(vert_shdr, GL_INFO_LOG_LENGTH, &logSize);
        GLchar *log_msg = alloca(logSize);
        glGetShaderInfoLog(vert_shdr, logSize, &logSize, &log_msg[0]);

        __android_log_print(ANDROID_LOG_INFO, "native", "failed to compile vertex shader:\n%s\n", log_msg);
        return;
    }
    glGetShaderiv(frag_shdr, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint logSize = 0;
        glGetShaderiv(frag_shdr, GL_INFO_LOG_LENGTH, &logSize);
        GLchar *log_msg = alloca(logSize);
        glGetShaderInfoLog(frag_shdr, logSize, &logSize, &log_msg[0]);

        __android_log_print(ANDROID_LOG_INFO, "native", "failed to compile fragment shader:\n%s\n", log_msg);
        return;
    }
    glAttachShader(prog, vert_shdr); CHECK_GL;
    glAttachShader(prog, frag_shdr); CHECK_GL;

    /* Assign each attribute a name */

    glBindAttribLocation(prog, vertex_index, "Position"); CHECK_GL;
    attrib_pos = vertex_index;
    vertex_index++;

    glBindAttribLocation(prog, vertex_index, "Color"); CHECK_GL;
    attrib_col = vertex_index;
    vertex_index++;

    glLinkProgram(prog); CHECK_GL;
    glGetProgramiv(prog, GL_LINK_STATUS, &status); CHECK_GL;
    assert(status == GL_TRUE);

    uniform_proj = glGetUniformLocation(prog, "ProjMtx"); CHECK_GL;
    assert(glGetError() == GL_NO_ERROR);
    assert(uniform_proj >= 0);

    {
        /* buffer setup */
        glGenVertexArrays(1, &vao); CHECK_GL;
        glBindVertexArray(vao); CHECK_GL;

        glGenBuffers(1, &vbo); CHECK_GL;
        glBindBuffer(GL_ARRAY_BUFFER, vbo); CHECK_GL;

        glGenBuffers(1, &ebo); CHECK_GL;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo); CHECK_GL;

        struct my_vertex
        {
            struct vec2 pos;
            unsigned int color;
        } vertices[] =
        {

            { vec2(30.50f, 30.50f),   0xff2d2d2d, },
            { vec2(400.50f, 30.50f),  0xff2d2d2d, },
            { vec2(400.50f, 450.50f), 0xff2d2d2d, },
            { vec2(30.50f, 450.50f),  0xff2d2d2d, },

            { vec2(130.50f, 130.50f), 0xff0000ff, },
            { vec2(230.50f, 130.50f), 0xff0000ff, },
            { vec2(230.50f, 230.50f), 0xff0000ff, },
            { vec2(130.50f, 230.50f), 0xff0000ff, },
        };

        unsigned short indices[] =
        {
            0, 1, 2, 0, 2, 3,
            4, 5, 6, 4, 6, 7,
        };

        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
            GL_STATIC_DRAW); CHECK_GL;

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
            GL_STATIC_DRAW); CHECK_GL;

        GLsizei vs = sizeof(struct my_vertex);
        size_t vp = offsetof(struct my_vertex, pos);
        size_t vc = offsetof(struct my_vertex, color);

        glEnableVertexAttribArray((GLuint)attrib_pos); CHECK_GL;
        glEnableVertexAttribArray((GLuint)attrib_col); CHECK_GL;

        glVertexAttribPointer((GLuint)attrib_pos, 2, GL_FLOAT, GL_FALSE, vs, (void*)vp); CHECK_GL;
        glVertexAttribPointer((GLuint)attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, vs, (void*)vc); CHECK_GL;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static
void render(int w, int h)
{
    GLint last_prog, last_tex;
    GLint last_ebo, last_vbo, last_vao;
    GLfloat ortho[4][4] =
    {
        {  2.0f,  0.0f,  0.0f,  0.0f  },
        {  0.0f, -2.0f,  0.0f,  0.0f  },
        {  0.0f,  0.0f, -1.0f,  0.0f  },
        { -1.0f,  1.0f,  0.0f,  1.0f  },
    };
    ortho[0][0] /= (GLfloat)w;
    ortho[1][1] /= (GLfloat)h;

    /* save previous opengl state */
    glGetIntegerv(GL_CURRENT_PROGRAM, &last_prog);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_vbo);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_ebo);
#ifndef GLES
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vao);
#else
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING_OES, &last_vao);
#endif

    /* setup global state */
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); CHECK_GL;
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);

    /* setup program */
    glUseProgram(prog); CHECK_GL;

    glUniformMatrix4fv(uniform_proj, 1, GL_FALSE, &ortho[0][0]); CHECK_GL;

    glBindVertexArray(vao); CHECK_GL;

    glBindBuffer(GL_ARRAY_BUFFER, vbo); CHECK_GL;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo); CHECK_GL;

    /*
     * Variant 1
     */
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0); CHECK_GL;
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (const GLushort*)(0) + 6); CHECK_GL;

    /*
     * Variant 2
     */
    // glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0); CHECK_GL;
    // glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLushort*)(0) + 6); CHECK_GL;

    /*
    * Variant 3
    */
    // glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_SHORT, (const GLushort*)(0)); CHECK_GL;

    /* restore old state */
    glBindTexture(GL_TEXTURE_2D, (GLuint)last_tex);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)last_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)last_ebo);
    glBindVertexArray((GLuint)last_vao);
    glDisable(GL_SCISSOR_TEST);
}

JNIEXPORT void JNICALL Java_com_example_yt_myapplication_LibraryClass_resize(JNIEnv *env, jobject thiz, jint w, jint h)
{
    width = w;
    height = h;

    __android_log_print(ANDROID_LOG_DEBUG, "native", "%s:%d, w:%d, h:%d", __FUNCTION__, __LINE__, width, height);

    glViewport(0, 0, width, height);
}

JNIEXPORT void JNICALL Java_com_example_yt_myapplication_LibraryClass_render(JNIEnv *env, jobject thiz) 
{
    glClearColor(1.0f, 0.5f, 0.0784f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render(width, height);
}
