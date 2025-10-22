#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#include "types.h"

#include "GL/ctor.h"
#include "GL/core.h"
#include "GL/utility.h"

#include "render_grh.h"
#include "render_gr.h"


GLuint shGrShader;
GLint attrGrVertex;
GLint attrGrColor;
GLint attrGrNotemix;
GLint uniGrLightAlpha;
GLint uniGrLightColor;
GLint uniGrTime;


void grInstallShader(void)
{
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
    GLuint psh = glCreateShader(GL_FRAGMENT_SHADER);
    
#pragma region Shader VSH
    
    const char* shadera =
        "#version 330 core\n"
        "out vec2 trigpos_v;\n"
        "flat out vec2 trigpos;\n"
        "in vec4 incolor;\n"
        "in vec2 inpos;\n"
    #ifndef PFAKEY
        "flat "
    #endif
        "out vec4 pcolor;\n"
    #ifdef HDR
        "out vec2 npos;\n"
        #ifdef TRIPPY
            "out float illum;\n"
        #endif
    #endif
    #ifdef PIANOKEYS
        "const float divider = 8.0F;\n"
    #endif
        "void main()\n"
        "{\n"
        "   vec2 rawpos = vec2(inpos.x, min(inpos.y, 1.0F));\n"
    #ifdef WIDEMIDI
        "   rawpos.x = rawpos.x * 0.5F;\n"
    #endif
    #ifdef GLOW
        "   float ill_a = "
    #if defined(TRIPPY) && !defined(HDR)
        "   clamp((-rawpos.y - 0.5F) * 4.0F, 0.0F, 5.6F);\n"
    #else
        "   clamp((-rawpos.y - 0.865F) * 16.0F, 1.0F, 5.6F);\n"
    #endif
        #if 0 && defined(HDR) && defined(TRIPPY)
        "   pcolor = incolor;\n"
        #else
        "   pcolor = vec4(incolor.xyz * ill_a, incolor.w);\n"
        #endif
    #else
        "   pcolor = incolor;\n"
    #endif
    #ifdef PIANOKEYS
        "   vec2 pos = vec2((rawpos.x * (4.0F / (150.0F * divider))) - 1.0F, rawpos.y);\n"
    #else
        "   vec2 pos = vec2((rawpos.x * (2.0F / 128.0F)) - 1.0F, rawpos.y);\n"
    #endif
        "   vec2 vtxpos = pos;\n"
    #ifdef KEYBOARD
        "   vtxpos.y = (vtxpos.y * 0.8F) + 0.2F;\n"
        //"   vtxpos.y = pow(vtxpos.y + 1.0F, 1.4F) - 1.0F;"
    #endif
    #ifdef HDR
        "   npos = vtxpos.xy;\n"
        #if defined(TRIPPY)
            #ifdef GLOW
            "    illum = clamp(((-rawpos.y - 0.95F) * 32.0F), 0.0F, 5.6F);\n"
            #endif
        #endif
    #endif
        "   gl_Position = vec4(vtxpos.xy, 0.0F, 1.0F);\n"
        "   trigpos_v = vtxpos.xy;\n"
        "   trigpos = vtxpos.xy;\n"
        "}\n"
        ;
        
#pragma endregion
    
#pragma region Shader FSH

    #if !defined(HDR)
    const char* shaderb =
    #else
    const char* shaderb[] =
    {
    #endif
        "#version 330 core\n"
        "in vec2 trigpos_v;\n"
        "flat in vec2 trigpos;\n"
    #ifndef PFAKEY
        "flat "
    #endif
        "in vec4 pcolor;\n"
    #ifdef HDR
        "in vec2 npos;\n"
        #ifdef TRIPPY
        "in float illum;\n"
        #endif
        "uniform float notemix;\n"
        #ifdef WIDEMIDI
        "uniform float lighta[256];\n"
        "uniform vec4  lightc[256];\n"
        #else
        "uniform float lighta[128];\n"
        "uniform vec4  lightc[128];\n"
        #endif
    #endif
        "out vec4 outcolor;\n"
        "void main()\n"
        "{\n"
    #if defined(HDR)
        "   outcolor = pcolor;\n"
        "   vec4 lightcolor = vec4(0.0F, 0.0F, 0.0F, 1.0F);\n"
        #ifdef FASTHDR
        "   if(notemix == 0.0F){\n"
        #endif
        #ifdef WIDEMIDI
        "   for(int i = 0; i < 256; i++)\n"
        #else
        "   for(int i = 0; i < 128; i++)\n"
        #endif
        "   {\n"
        "       vec2 posdiff = npos - vec2(((i - 128) / 64.0F) + 1.0F + (1.0F / 151.0F), -0.6F);\n"
        #if !defined(TRIPPY)
        //"       vec2 asposdif = posdiff;\n"
        "       vec2 asposdif = vec2(posdiff.x, posdiff.y * (9.0F / 16.0F));\n"
        "       float posdist = dot(asposdif, asposdif);\n"
        "       lightcolor += vec4((lightc[i].xyz * lighta[i]) * min(pow(1.0F / posdist, 1.0F) * (1.0F / 8192.0F), 4.0F), 0.0F);\n"
        #else
        "       vec2 asposdif = vec2(posdiff.x, posdiff.y * (1.0F / 4.0F));\n"
        "       float posdist = dot(asposdif, asposdif);\n"
        "       lightcolor += vec4((lightc[i].xyz * pow(lighta[i], 1.0F)) * min(pow((0.5F / posdist), 1.0F) * (1.0F / 4096.0F), 4.0F), 0.0F);\n"
        #endif
        "   }\n"
        #ifdef FASTHDR
        "   }\n"
        #endif
        #if defined(TRIPPY)
        "   float notea = 1.0F;// - notemix;\n"
        //"   float sosi = dot(lightcolor.xyz, pcolor.www) * 16.0F;\n"
        "   float sosi = max(1.0F, illum);\n"
        //"   sosi = clamp(sosi, 0.08F, 2.4F);\n"
        "   sosi = clamp(sosi, 0.08F, 1.0F);\n"
        "   outcolor = vec4("
                "(lightcolor.xyz * notemix) +"
                "(pcolor.xyz * vec3(mix(sosi, 1.0F, notea)))"
                " * (((("
                        "pow(lightcolor.xyz, vec3(1.8F))"
                        " * vec3(32.0F))"
                    " + vec3((1.0F / 64.0F))"
                    ") * vec3(notea)) + vec3(notemix))"
            ", outcolor.w);\n"
        
        /*
        "   outcolor = vec4("
        //"(lightcolor.xyz * notemix) +"
        "(pcolor.xyz * vec3((notemix * sosi) + 1.0F)) * (((pow(lightcolor.xyz, vec3((notea * -0.25F) + 1.0F)) * ((notea * 0.4F) + 1.0F)) * vec3(notea)) + ((1.0F / 32.0F))), outcolor.w);\n"
        */
        #else
        "   outcolor += lightcolor;\n"
        #endif
        #ifdef NOKEYBOARD
            "   outcolor = vec4(mix(outcolor.xyz, lightcolor.xyz, clamp((-npos.y - 0.58F) * 32.0F, 0.0F, 1.0F)), outcolor.w);\n"
        #endif
        ,
        "",
    #else
        "   outcolor = vec4(pcolor.xyz, pcolor.w);\n"
    #endif
        "}\n"
    #if defined(HDR)
    }
    #endif
    ;
    
#pragma endregion
    
    
    GLint stat = 0;
    
    char* sh_vsh = NULL;
    char* sh_fsh = NULL;
    size_t size_vsh = 0;
    size_t size_fsh = 0;
    
    FILE* file_in_vsh = fopen("MPGL_GR_VSH.vsh", "rb");
    FILE* file_in_fsh = fopen("MPGL_GR_FSH.fsh", "rb");
    
    if(file_in_fsh && file_in_vsh)
    {
        fseek(file_in_vsh, 0, SEEK_END);
        fseek(file_in_fsh, 0, SEEK_END);
        
        size_vsh = ftell(file_in_vsh);
        size_fsh = ftell(file_in_fsh);
        
        fseek(file_in_vsh, 0, SEEK_SET);
        fseek(file_in_fsh, 0, SEEK_SET);
        
        // Zero-size malloc is impl-defined, so
        //  don't play with fire, over-allocate
        sh_vsh = malloc(size_vsh + 1);
        sh_fsh = malloc(size_fsh + 1);
        
        if(sh_vsh && sh_fsh)
        {
            sh_vsh[size_vsh] = '\0';
            sh_fsh[size_fsh] = '\0';
            
            if(fread(sh_vsh, 1, size_vsh, file_in_vsh) != size_vsh)
            {
                free(sh_vsh);
                sh_vsh = NULL;
            }
            else if(fread(sh_fsh, 1, size_fsh, file_in_fsh) != size_fsh)
            {
                free(sh_fsh);
                sh_fsh = NULL;
            }
            else
            {
                puts("Successfully loaded custom shaders");
            }
        }
        else
        {
            puts("Failed to load custom shaders: memory allocation failure");
        }
    }
    else if(file_in_fsh || file_in_vsh)
    {
        puts("Found custom shaders, but failed to open all");
    }
    
    if(file_in_vsh)
    {
        fclose(file_in_vsh);
        file_in_vsh = NULL;
    }
    
    if(file_in_fsh)
    {
        fclose(file_in_fsh);
        file_in_fsh = NULL;
    }
    
    if(sh_vsh && sh_fsh)
    {
        const char sh_header[] =
        "#version 130 core\r\n" // GLSL 3.0
        "#line 0 1\r\n"         // Enable 2nd file file numbers
        ;
        
        // char[] = literal; should include the null-terminator in the array,
        //  so subtract one to exclude it
        GLint strlen_header = sizeof(sh_header) - 1;
        
        const char* sh_source_vsh[2] = {sh_header, sh_vsh};
        const char* sh_source_fsh[2] = {sh_header, sh_fsh};
        GLint sh_length_vsh[2] = {-1, size_vsh};
        GLint sh_length_fsh[2] = {-1, size_fsh};
        
        glShaderSource(vsh, 2, sh_source_vsh, sh_length_vsh);
        glShaderSource(psh, 2, sh_source_fsh, sh_length_fsh);
        
        free(sh_vsh);
        free(sh_fsh);
    }
    else
    {
        glShaderSource(vsh, 1, &shadera, NULL);
        
        #if defined(HDR)
        #ifndef TIMI_CAPTURE
        if(!uglSupportsExt("WGL_EXT_framebuffer_sRGB"))
        #endif
        {
            shaderb[1] =
            //"   outcolor = vec4(pow(outcolor.xyz / (outcolor.xyz + vec3(1.0F)), vec3(1.1F)), outcolor.w);\n"
            "   outcolor = vec4(pow(outcolor.xyz, vec3(0.5F)), outcolor.w);\n"
            //"\n"
            ;
        }
        glShaderSource(psh, 3, shaderb, NULL);
        #else
        glShaderSource(psh, 1, &shaderb, NULL);
        #endif
    }
    
    glCompileShader(vsh);
    glGetShaderiv(vsh, GL_COMPILE_STATUS, &stat);
    //if(stat != GL_TRUE)
        grhPrintShaderInfoLog(vsh);
    
    glCompileShader(psh);
    glGetShaderiv(psh, GL_COMPILE_STATUS, &stat);
    //if(stat != 1)
        grhPrintShaderInfoLog(psh);
    
    shGrShader = glCreateProgram();
    
    glAttachShader(shGrShader, vsh);
    glAttachShader(shGrShader, psh);
    
    glLinkProgram(shGrShader);
    
    glGetProgramiv(shGrShader, GL_LINK_STATUS, &stat);
    //if(stat != GL_TRUE)
        grhPrintProgramInfoLog(shGrShader);
    
    attrGrVertex = glGetAttribLocation(shGrShader, "inpos");
    attrGrColor = glGetAttribLocation(shGrShader, "incolor");
    
    if(SH_INVALID(attrGrVertex))
        puts("inpos not found");
    if(SH_INVALID(attrGrColor))
        puts("incolor not found");
    
    if(attrGrVertex < 0 || attrGrColor < 0)
        __builtin_trap();
    
    uniGrTime = glGetUniformLocation(shGrShader, "intime");
    
    uniGrLightAlpha = glGetUniformLocation(shGrShader, "lighta");
    uniGrLightColor = glGetUniformLocation(shGrShader, "lightc");
    attrGrNotemix = glGetUniformLocation(shGrShader, "notemix");
    
    
    glDetachShader(shGrShader, psh);
    glDetachShader(shGrShader, vsh);
    
    glDeleteShader(vsh);
    glDeleteShader(psh);
}
