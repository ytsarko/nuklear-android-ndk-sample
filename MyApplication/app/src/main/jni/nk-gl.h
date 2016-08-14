#ifndef NK_GL_H
#define NK_GL_H

#pragma once

#include "nuklear.h"

struct nk_gl
{
    struct nk_gl_device    *ogl;
    struct nk_context      *ctx;
};

int nk_gl_init(struct nk_context *nk_ctx, struct nk_gl **out);
void nk_gl_shutdown(struct nk_gl **ctx);

void nk_gl_render(struct nk_gl *nk_gl, enum nk_anti_aliasing aa,
    int w, int h, int max_vertex_buffer, int max_element_buffer);

void nk_gl_blit(struct nk_gl *nk_gl, struct nk_color bg, int w, int h,
    enum nk_anti_aliasing aa);

#endif /* NK_GL_H */
