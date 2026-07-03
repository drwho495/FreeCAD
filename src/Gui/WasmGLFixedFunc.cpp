/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * WebAssembly build only. Coin3D still emits a handful of fixed-function
 * OpenGL 1.x entry points that do not exist in WebGL2/GLES3 (the context Qt
 * hands us). Emscripten's libGL provides the ES2/WebGL subset but not these
 * legacy calls, so the link fails with undefined symbols.
 *
 * This shim provides no-op (or minimal) definitions so the GUI links and
 * boots with a *non-rendering* 3D viewport. It is the seam that Stage 4's
 * real GLES2/WebGL2 Coin backend replaces with a shader-based fixed-function
 * emulation (client-side matrix stack, lighting, texgen, …). Until then the
 * viewport is expected to be blank/incorrect; the rest of the GUI is live.
 *
 * See research/R3-graphics-coin-webgl.md.
 */
#include <stddef.h>

extern "C" {

typedef unsigned int GLenum;
typedef int GLint;
typedef float GLfloat;
typedef double GLdouble;
typedef unsigned char GLboolean;

/* Matrix stack — Stage 4 replaces with a client-side stack feeding uniforms. */
void glPushMatrix(void) {}
void glPopMatrix(void) {}
void glMultMatrixf(const GLfloat* m) { (void)m; }
void glTranslatef(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }
void glScalef(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }

/* Fixed-function material / texgen — emulated in shaders in Stage 4. */
void glColorMaterial(GLenum face, GLenum mode) { (void)face; (void)mode; }
void glTexGeni(GLenum coord, GLenum pname, GLint param) { (void)coord; (void)pname; (void)param; }
void glTexGenfv(GLenum coord, GLenum pname, const GLfloat* params)
{
    (void)coord; (void)pname; (void)params;
}

/* Accumulation buffer — never existed in ES; genuinely unsupported. */
void glAccum(GLenum op, GLfloat value) { (void)op; (void)value; }

/* Legacy state query. Zero-fill so callers see a defined (identity-ish) state
 * rather than reading uninitialized memory. */
void glGetDoublev(GLenum pname, GLdouble* params)
{
    (void)pname;
    if (params) {
        params[0] = 0.0;
    }
}

/* Immediate mode + legacy fixed-function used by SoNaviCube and Coin overlays.
 * All no-ops until the Stage 4 backend batches these into vertex buffers. */
void glLoadMatrixd(const GLdouble* m) { (void)m; }
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
    (void)l; (void)r; (void)b; (void)t; (void)n; (void)f;
}
void glAlphaFunc(GLenum func, GLfloat ref) { (void)func; (void)ref; }
void glTexEnvi(GLenum target, GLenum pname, GLint param) { (void)target; (void)pname; (void)param; }
void glColor3f(GLfloat r, GLfloat g, GLfloat b) { (void)r; (void)g; (void)b; }
void glVertex2f(GLfloat x, GLfloat y) { (void)x; (void)y; }
void glTexCoord2f(GLfloat s, GLfloat t) { (void)s; (void)t; }

/* Immediate-mode begin/end and the vertex-attribute variants Coin emits. */
typedef unsigned char GLubyte;
void glBegin(GLenum mode) { (void)mode; }
void glEnd(void) {}
void glVertex2fv(const GLfloat* v) { (void)v; }
void glVertex3f(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }
void glVertex3fv(const GLfloat* v) { (void)v; }
void glVertex4fv(const GLfloat* v) { (void)v; }
void glNormal3f(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }
void glNormal3fv(const GLfloat* v) { (void)v; }
void glColor3fv(const GLfloat* v) { (void)v; }
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { (void)r; (void)g; (void)b; (void)a; }
void glColor4fv(const GLfloat* v) { (void)v; }
void glColor3ubv(const GLubyte* v) { (void)v; }
void glColor4ubv(const GLubyte* v) { (void)v; }
void glTexCoord2fv(const GLfloat* v) { (void)v; }
void glTexCoord3fv(const GLfloat* v) { (void)v; }
void glTexCoord4fv(const GLfloat* v) { (void)v; }
void glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    (void)s; (void)t; (void)r; (void)q;
}
void glRasterPos3fv(const GLfloat* v) { (void)v; }
void glRasterPos2f(GLfloat x, GLfloat y) { (void)x; (void)y; }

/* Fog, clip planes, light model, polygon mode, texture env, color index —
 * all fixed-function state with no WebGL2 equivalent (Stage 4 shader work). */
void glClipPlane(GLenum plane, const GLdouble* eqn) { (void)plane; (void)eqn; }
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) { (void)r; (void)g; (void)b; (void)a; }
void glFogf(GLenum pname, GLfloat param) { (void)pname; (void)param; }
void glFogfv(GLenum pname, const GLfloat* params) { (void)pname; (void)params; }
void glFogi(GLenum pname, GLint param) { (void)pname; (void)param; }
void glIndexi(GLint c) { (void)c; }
void glLightModelfv(GLenum pname, const GLfloat* params) { (void)pname; (void)params; }
void glLightModeli(GLenum pname, GLint param) { (void)pname; (void)param; }
void glPolygonMode(GLenum face, GLenum mode) { (void)face; (void)mode; }
void glTexEnvf(GLenum target, GLenum pname, GLfloat param) { (void)target; (void)pname; (void)param; }
void glTexEnvfv(GLenum target, GLenum pname, const GLfloat* params)
{
    (void)target; (void)pname; (void)params;
}
void glLightf(GLenum light, GLenum pname, GLfloat param) { (void)light; (void)pname; (void)param; }
void glLightfv(GLenum light, GLenum pname, const GLfloat* params)
{
    (void)light; (void)pname; (void)params;
}
void glLighti(GLenum light, GLenum pname, GLint param) { (void)light; (void)pname; (void)param; }
void glMaterialf(GLenum face, GLenum pname, GLfloat param) { (void)face; (void)pname; (void)param; }
void glMaterialfv(GLenum face, GLenum pname, const GLfloat* params)
{
    (void)face; (void)pname; (void)params;
}
void glMateriali(GLenum face, GLenum pname, GLint param) { (void)face; (void)pname; (void)param; }
void glShadeModel(GLenum mode) { (void)mode; }
void glLoadMatrixf(const GLfloat* m) { (void)m; }
void glMultMatrixd(const GLdouble* m) { (void)m; }
void glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z) { (void)a; (void)x; (void)y; (void)z; }
void glRotated(GLdouble a, GLdouble x, GLdouble y, GLdouble z)
{
    (void)a; (void)x; (void)y; (void)z;
}
void glMatrixMode(GLenum mode) { (void)mode; }
void glLoadIdentity(void) {}
void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
    (void)l; (void)r; (void)b; (void)t; (void)n; (void)f;
}
void glPushAttrib(GLenum mask) { (void)mask; }
void glPopAttrib(void) {}
void glPushClientAttrib(GLenum mask) { (void)mask; }
void glPopClientAttrib(void) {}
void glLineStipple(GLint factor, unsigned short pattern) { (void)factor; (void)pattern; }
void glPolygonStipple(const GLubyte* mask) { (void)mask; }
void glTexGend(GLenum coord, GLenum pname, GLdouble param) { (void)coord; (void)pname; (void)param; }
void glTexGendv(GLenum coord, GLenum pname, const GLdouble* params)
{
    (void)coord; (void)pname; (void)params;
}
void glRasterPos3f(GLfloat x, GLfloat y, GLfloat z) { (void)x; (void)y; (void)z; }
void glBitmap(GLint w, GLint h, GLfloat x0, GLfloat y0, GLfloat xm, GLfloat ym, const GLubyte* bm)
{
    (void)w; (void)h; (void)x0; (void)y0; (void)xm; (void)ym; (void)bm;
}
void glDrawPixels(GLint w, GLint h, GLenum fmt, GLenum type, const void* px)
{
    (void)w; (void)h; (void)fmt; (void)type; (void)px;
}
void glGetTexGeniv(GLenum coord, GLenum pname, GLint* params)
{
    (void)coord; (void)pname;
    if (params) { params[0] = 0; }
}

/* Display lists — no equivalent in GLES; Coin also has a VBO path we rely on.
 * glGenLists returns 0 ("no list") so callers treat lists as unavailable. */
typedef unsigned int GLuint;
typedef int GLsizei;
GLuint glGenLists(GLsizei range) { (void)range; return 0; }
void glNewList(GLuint list, GLenum mode) { (void)list; (void)mode; }
void glEndList(void) {}
void glCallList(GLuint list) { (void)list; }
void glDeleteLists(GLuint list, GLsizei range) { (void)list; (void)range; }
void glListBase(GLuint base) { (void)base; }
void glCallLists(GLsizei n, GLenum type, const void* lists) { (void)n; (void)type; (void)lists; }

/* Misc remaining fixed-function entry points. */
void glPointSize(GLfloat size) { (void)size; }
void glVertex2s(short x, short y) { (void)x; (void)y; }
void glVertex2i(GLint x, GLint y) { (void)x; (void)y; }
void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params)
{
    (void)target; (void)level; (void)pname;
    if (params) { params[0] = 0; }
}
void glColor3ub(GLubyte r, GLubyte g, GLubyte b) { (void)r; (void)g; (void)b; }
void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r) { (void)s; (void)t; (void)r; }
void glTexGenf(GLenum coord, GLenum pname, GLfloat param) { (void)coord; (void)pname; (void)param; }
void glClearIndex(GLfloat c) { (void)c; }
void glDrawBuffer(GLenum buf) { (void)buf; }
void glPixelMapfv(GLenum map, GLint size, const GLfloat* values) { (void)map; (void)size; (void)values; }
void glPixelMapuiv(GLenum map, GLint size, const unsigned int* values)
{
    (void)map; (void)size; (void)values;
}
void glPixelTransferf(GLenum pname, GLfloat param) { (void)pname; (void)param; }
void glPixelTransferi(GLenum pname, GLint param) { (void)pname; (void)param; }
void glPixelZoom(GLfloat xfactor, GLfloat yfactor) { (void)xfactor; (void)yfactor; }

/* EGL platform-extension entry points referenced by Coin's context glue
 * (src/glue/gl_egl.cpp). Qt owns the GL context on wasm, so Coin never drives
 * EGL; these exist only to satisfy the linker and return "no display/surface". */
typedef void* EGLDisplay;
typedef void* EGLSurface;
typedef void* EGLConfig;
typedef void* EGLNativeWindowType;
typedef void* EGLNativePixmapType;
typedef int EGLenum;
typedef int EGLint;
typedef unsigned int EGLBoolean;
EGLDisplay eglGetPlatformDisplay(EGLenum p, void* d, const EGLint* a)
{
    (void)p; (void)d; (void)a; return (EGLDisplay)0;
}
EGLSurface eglCreatePlatformWindowSurface(EGLDisplay d, EGLConfig c, void* w, const EGLint* a)
{
    (void)d; (void)c; (void)w; (void)a; return (EGLSurface)0;
}
EGLSurface eglCreatePlatformPixmapSurface(EGLDisplay d, EGLConfig c, void* p, const EGLint* a)
{
    (void)d; (void)c; (void)p; (void)a; return (EGLSurface)0;
}
EGLBoolean eglBindTexImage(EGLDisplay d, EGLSurface s, EGLint b)
{
    (void)d; (void)s; (void)b; return 0;
}
EGLBoolean eglReleaseTexImage(EGLDisplay d, EGLSurface s, EGLint b)
{
    (void)d; (void)s; (void)b; return 0;
}

}  // extern "C"
