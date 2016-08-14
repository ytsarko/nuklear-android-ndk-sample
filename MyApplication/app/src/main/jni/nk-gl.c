#define USE_FULL_GL 0

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <assert.h>
#include <errno.h>
#include <alloca.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>

#include <android/log.h>



static int nk_gl_device_create(struct nk_gl_device *dev);
static void nk_gl_device_destroy(struct nk_gl_device *dev);
static void setup_null_texture(struct nk_gl_device *dev);
static __GLXextFuncPtr nk_gl_ext(const char *name);

/* GL_ARB_vertex_array_object */


int nk_gl_init(struct nk_context *nk_ctx, struct nk_gl **out)
{
    assert(nk_ctx);
    assert(out);

    *out = 0;

    int retval = 0;
    struct nk_gl *nk_gl = 0;
    struct nk_gl_device *nk_gl_dev = 0;    

    nk_gl = calloc(1, sizeof(*nk_gl));
    if (!nk_gl) return ENOMEM;

    nk_gl_dev = calloc(1, sizeof(*nk_gl_dev));
    if (!nk_gl_dev)
    {
        retval = ENOMEM;
        goto err;
    }

    retval = nk_gl_device_create(nk_gl_dev);
    if (retval != 0) goto err;

    nk_gl->ogl = nk_gl_dev;
    nk_gl->ctx = nk_ctx;

    *out = nk_gl;

    return 0;

err:
    if (nk_gl_dev) free(nk_gl_dev);
    if (nk_gl) free(nk_gl);

    return retval;
}

void nk_gl_shutdown(struct nk_gl **ctx)
{
    assert(ctx);

    nk_gl_device_destroy((*ctx)->ogl);
    free(*ctx);

    *ctx = 0;
}

void nk_gl_render(struct nk_gl *nk_gl, enum nk_anti_aliasing aa,
    int w, int h, int max_vertex_buffer, int max_element_buffer)
{
    struct nk_gl_device *dev = nk_gl->ogl;
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
    // glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    /* setup program */
    glUseProgram(dev->prog); CHECK_GL;

    glUniformMatrix4fv(dev->uniform_proj, 1, GL_FALSE, &ortho[0][0]); CHECK_GL;

    glBindVertexArray(dev->vao); CHECK_GL;

    glBindBuffer(GL_ARRAY_BUFFER, dev->vbo); CHECK_GL;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo); CHECK_GL;

    glBindTexture(GL_TEXTURE_2D, (GLuint)dev->null.texture.id); CHECK_GL;
    // glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0); CHECK_GL;
    // glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (const GLushort*)(0) + 6); CHECK_GL;
    glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_SHORT, (const GLushort*)(0)); CHECK_GL;

    /* restore old state */
    glBindTexture(GL_TEXTURE_2D, (GLuint)last_tex);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)last_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)last_ebo);
    glBindVertexArray((GLuint)last_vao);
    glDisable(GL_SCISSOR_TEST);
}

static GLuint FramebufferName;

void nk_gl_blit(struct nk_gl *nk_gl, struct nk_color bg, int w, int h,
    enum nk_anti_aliasing aa)
{
    float background[4];
    nk_color_fv(background, bg);

    // glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName); CHECK_GL;

    glViewport(0, 0, w, h); CHECK_GL;
    glClearColor(background[0], background[1], background[2], background[3]); CHECK_GL;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); CHECK_GL;
    nk_gl_render(nk_gl, aa, w, h, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
}

static
void setup_framebuffer()
{
    glGenFramebuffers(1, &FramebufferName); CHECK_GL;
    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName); CHECK_GL;

    GLuint renderedTexture;
    glGenTextures(1, &renderedTexture); CHECK_GL;
    glBindTexture(GL_TEXTURE_2D, renderedTexture); CHECK_GL;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1280, 720, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0); CHECK_GL;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHECK_GL;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); CHECK_GL;

    // Set "renderedTexture" as our colour attachement #0
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderedTexture, 0); CHECK_GL;

    // Set the list of draw buffers.
    // GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    // glDrawBuffers(1, DrawBuffers); CHECK_GL; // "1" is the size of DrawBuffers

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("failed to configure frame buffer\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0); CHECK_GL;
}

static int nk_gl_device_create(struct nk_gl_device *dev)
{
    assert(glGenVertexArrays);
    assert(glBindVertexArray);
    assert(glDeleteVertexArrays);


    setup_null_texture(dev);

    setup_framebuffer();

    return 0;
}

static void nk_gl_device_destroy(struct nk_gl_device *dev)
{
    glDeleteTextures(1, (GLuint*)&dev->null.texture.id);
    glDetachShader(dev->prog, dev->vert_shdr);
    glDetachShader(dev->prog, dev->frag_shdr);
    glDeleteShader(dev->vert_shdr);
    glDeleteShader(dev->frag_shdr);
    glDeleteProgram(dev->prog);
    glDeleteVertexArrays(1, &dev->vao);
    glDeleteBuffers(1, &dev->vbo);
    glDeleteBuffers(1, &dev->ebo);
    nk_buffer_free(&dev->cmds);
}

static
void setup_null_texture(struct nk_gl_device *dev)
{
    GLuint id;
    glGenTextures(1, &id); CHECK_GL;
    glBindTexture(GL_TEXTURE_2D, id); CHECK_GL;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); CHECK_GL;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); CHECK_GL;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); CHECK_GL;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); CHECK_GL;

    const size_t width = 4;
    const size_t height = 4;
    const size_t size = width * height * sizeof(uint32_t);

    uint32_t *null_texture = malloc(size);
    assert(null_texture);
    // memset(null_texture, 0xff, size);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            uint32_t *pixel = &null_texture[y * width + x];
            // *pixel = (0xff << 24) | (0x00 << 16) | (0x00 << 8) | 0x00;
            *pixel = 0xffffffff;
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
        width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, null_texture); CHECK_GL;

    dev->null.texture = nk_handle_id(id);
    dev->null.uv = nk_vec2(0.5f, 0.5f);
}

#define NK_IMPLEMENTATION
#include "nuklear.h"
