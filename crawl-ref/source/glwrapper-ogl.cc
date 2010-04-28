#include "AppHdr.h"

#ifdef USE_TILE
#ifdef USE_GL

#include "glwrapper-ogl.h"

// How do we get access to the GL calls?
// If other UI types use the -ogl wrapper they should
// include more conditional includes here.
#ifdef USE_SDL
#include <SDL_opengl.h>
#endif

#include "debug.h"

/////////////////////////////////////////////////////////////////////////////
// Static functions from GLStateManager

GLStateManager *glmanager = NULL;

void GLStateManager::init()
{
    if (glmanager)
        return;

    glmanager = new OGLStateManager();
}

void GLStateManager::shutdown()
{
    delete glmanager;
    glmanager = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// OGLStateManager

OGLStateManager::OGLStateManager()
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0, 0.0, 0.0, 1.0f);
    glDepthFunc(GL_LEQUAL);
}

void OGLStateManager::set(const GLState& state)
{
    if (state.array_vertex)
        glEnableClientState(GL_VERTEX_ARRAY);
    else
        glDisableClientState(GL_VERTEX_ARRAY);

    if (state.array_texcoord)
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    else
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    if (state.array_colour)
    {
        glEnableClientState(GL_COLOR_ARRAY);
    }
    else
    {
        glDisableClientState(GL_COLOR_ARRAY);

        // [enne] This should *not* be necessary, but the Linux OpenGL
        // driver that I'm using sets this to the last colour of the
        // colour array.  So, we need to unset it here.
        glColor3f(1.0f, 1.0f, 1.0f);
    }

    if (state.texture)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (state.blend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (state.depthtest)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (state.alphatest)
    {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_NOTEQUAL, state.alpharef);
    }
    else
        glDisable(GL_ALPHA_TEST);
}

void OGLStateManager::set_transform(const GLW_3VF *trans, const GLW_3VF *scale)
{
    glLoadIdentity();
    if (trans)
        glTranslatef(trans->x, trans->y, trans->z);
    if (scale)
        glScalef(scale->x, scale->y, scale->z);
}

void OGLStateManager::reset_view_for_resize(coord_def &m_windowsz)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // For ease, vertex positions are pixel positions.
    glOrtho(0, m_windowsz.x, m_windowsz.y, 0, -1000, 1000);
}

void OGLStateManager::reset_transform()
{
    glLoadIdentity();
    glTranslatef(0,0,0);
    glScalef(1,1,1);
}

void OGLStateManager::pixelstore_unpack_alignment(unsigned int bpp)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, bpp);
}

void OGLStateManager::draw_primitive(const GLPrimitive &prim)
{
    // Handle errors
    if (!prim.vert_pointer || prim.count < 1 || prim.size < 1)
        return;
    ASSERT(GLStateManager::_valid(prim.count, prim.mode));

    // Set pointers
    glVertexPointer(prim.vert_size, GL_FLOAT, prim.size, prim.vert_pointer);
    if (prim.texture_pointer)
        glTexCoordPointer(2, GL_FLOAT, prim.size, prim.texture_pointer);
    if (prim.colour_pointer)
        glColorPointer(4, GL_UNSIGNED_BYTE, prim.size, prim.colour_pointer);

    // Handle pre-render matrix manipulations
    if (prim.pretranslate || prim.prescale)
    {
        glPushMatrix();
        if (prim.pretranslate)
        {
            glTranslatef(prim.pretranslate->x,
                         prim.pretranslate->y,
                         prim.pretranslate->z);
        }
        if (prim.prescale)
            glScalef(prim.prescale->x, prim.prescale->y, prim.prescale->z);
    }

    // Draw!
    switch (prim.mode)
    {
    case GLW_QUADS:
        glDrawArrays(GL_QUADS, 0, prim.count);
        break;
    case GLW_LINES:
        glDrawArrays(GL_LINES, 0, prim.count);
        break;
    default:
        break;
    }

    // Clean up
    if (prim.pretranslate || prim.prescale)
    {
        glPopMatrix();
    }
}

void OGLStateManager::delete_textures(size_t count, unsigned int *textures)
{
    glDeleteTextures(count, (GLuint*)textures);
}

void OGLStateManager::generate_textures(size_t count, unsigned int *textures)
{
    glGenTextures(count, (GLuint*)textures);
}

void OGLStateManager::bind_texture(unsigned int texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
}

void OGLStateManager::load_texture(unsigned char *pixels, unsigned int width,
                                   unsigned int height, MipMapOptions mip_opt)
{
    // Assumptions...
    const unsigned int bpp = 4;
    const GLenum texture_format = GL_RGBA;
    const GLenum format = GL_UNSIGNED_BYTE;
    // Also assume that the texture is already bound using bind_texture

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    if (mip_opt == MIPMAP_CREATE)
    {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gluBuild2DMipmaps(GL_TEXTURE_2D, bpp, width, height,
                          texture_format, format, pixels);
    }
    else
    {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, bpp, width, height, 0,
                     texture_format, format, pixels);
    }
}

void OGLStateManager::reset_view_for_redraw(float x, float y)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(x, y , 1.0f);
}

void OGLStateManager::set_current_color(GLW_3VF &color)
{
    glColor3f(color.r, color.g, color.b);
}

void OGLStateManager::set_current_color(GLW_4VF &color)
{
    glColor4f(color.r, color.g, color.b, color.a);
}

#endif // USE_GL
#endif // USE_TILE