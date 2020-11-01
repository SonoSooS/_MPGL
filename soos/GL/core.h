#ifndef GLDEFINE
#include <GL/glcorearb.h>
#define GLDEFINE(proc, name) GLAPI proc name
#endif

GLDEFINE(PFNGLCULLFACEPROC, glCullFace);
GLDEFINE(PFNGLTEXPARAMETERIPROC, glTexParameteri);
GLDEFINE(PFNGLTEXIMAGE2DPROC, glTexImage2D);
GLDEFINE(PFNGLCLEARPROC, glClear);
GLDEFINE(PFNGLCLEARCOLORPROC, glClearColor);
GLDEFINE(PFNGLCLEARDEPTHPROC, glClearDepth);
GLDEFINE(PFNGLDISABLEPROC, glDisable);
GLDEFINE(PFNGLENABLEPROC, glEnable);
GLDEFINE(PFNGLFINISHPROC, glFinish);
GLDEFINE(PFNGLFLUSHPROC, glFlush);
GLDEFINE(PFNGLBLENDFUNCPROC, glBlendFunc);
GLDEFINE(PFNGLDEPTHFUNCPROC, glDepthFunc);
GLDEFINE(PFNGLGETSTRINGPROC, glGetString);
GLDEFINE(PFNGLVIEWPORTPROC, glViewport);
GLDEFINE(PFNGLDRAWARRAYSPROC, glDrawArrays);
GLDEFINE(PFNGLDRAWELEMENTSPROC, glDrawElements);
GLDEFINE(PFNGLBINDTEXTUREPROC, glBindTexture);
GLDEFINE(PFNGLDELETETEXTURESPROC, glDeleteTextures);
GLDEFINE(PFNGLGENTEXTURESPROC, glGenTextures);
GLDEFINE(PFNGLACTIVETEXTUREPROC, glActiveTexture);
GLDEFINE(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate);
GLDEFINE(PFNGLBLENDCOLORPROC, glBlendColor);
GLDEFINE(PFNGLBLENDEQUATIONPROC, glBlendEquation);
GLDEFINE(PFNGLBINDBUFFERPROC, glBindBuffer);
GLDEFINE(PFNGLDELETEBUFFERSPROC, glDeleteBuffers);
GLDEFINE(PFNGLGENBUFFERSPROC, glGenBuffers);
GLDEFINE(PFNGLBUFFERDATAPROC, glBufferData);
GLDEFINE(PFNGLBUFFERSUBDATAPROC, glBufferSubData);
GLDEFINE(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate);
GLDEFINE(PFNGLDRAWBUFFERSPROC, glDrawBuffers);
GLDEFINE(PFNGLATTACHSHADERPROC, glAttachShader);
GLDEFINE(PFNGLCOMPILESHADERPROC, glCompileShader);
GLDEFINE(PFNGLCREATEPROGRAMPROC, glCreateProgram);
GLDEFINE(PFNGLCREATESHADERPROC, glCreateShader);
GLDEFINE(PFNGLDELETEPROGRAMPROC, glDeleteProgram);
GLDEFINE(PFNGLDELETESHADERPROC, glDeleteShader);
GLDEFINE(PFNGLDETACHSHADERPROC, glDetachShader);
GLDEFINE(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray);
GLDEFINE(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray);
GLDEFINE(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation);
GLDEFINE(PFNGLGETATTRIBLOCATIONPROC, glGetAttribLocation);
GLDEFINE(PFNGLGETPROGRAMIVPROC, glGetProgramiv);
GLDEFINE(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog);
GLDEFINE(PFNGLGETSHADERIVPROC, glGetShaderiv);
GLDEFINE(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog);
GLDEFINE(PFNGLLINKPROGRAMPROC, glLinkProgram);
GLDEFINE(PFNGLSHADERSOURCEPROC, glShaderSource);
GLDEFINE(PFNGLUSEPROGRAMPROC, glUseProgram);
GLDEFINE(PFNGLUNIFORM1FPROC, glUniform1f);
GLDEFINE(PFNGLUNIFORM1FVPROC, glUniform1fv);
GLDEFINE(PFNGLUNIFORM4FVPROC, glUniform4fv);
GLDEFINE(PFNGLUNIFORM1IPROC, glUniform1i);
GLDEFINE(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer);
GLDEFINE(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray);
GLDEFINE(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays);
GLDEFINE(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays);
GLDEFINE(PFNGLDEBUGMESSAGECALLBACKARBPROC, glDebugMessageCallbackARB);
