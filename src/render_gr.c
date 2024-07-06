#include <stdio.h>

#include "config.h"

#include "GL/ctor.h"
#include "GL/core.h"
#include "GL/utility.h"

#include "render_grh.h"
#include "render_gr.h"


extern GLuint sh;
extern GLint attrVertex, attrColor;
#ifdef SHTIME
extern GLint uniTime;
#endif
#ifdef HDR
extern GLint uniLightAlpha;
extern GLint uniLightColor;
#ifdef TRIPPY
extern GLint attrNotemix;
#endif
#endif

void grCompileStandardShader(void)
{
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
    GLuint psh = glCreateShader(GL_FRAGMENT_SHADER);
    
    
    const char* shadera =
        "#version 330 core\n"
        "out vec2 trigpos_v;\n"
        "flat out vec2 trigpos;\n"
    #ifdef SHTIME
        "uniform float intime;\n"
    #endif
        "in vec4 incolor;\n"
        "in vec2 inpos;\n"
    #ifndef PFAKEY
        "flat "
    #endif
        "out vec4 pcolor;\n"
    #ifdef TRANSFORM
        "out float zbuf;"
    #endif
    #if defined(ROUNDEDGE) || defined(GLOWEDGE)
        "out vec2 ppos;\n"
        "flat out vec2 fpos;\n"
    #endif
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
    #ifdef TRANSFORM
        "   vec2 rawpos = vec2(inpos.x, clamp(inpos.y, -1.25F, 1.0F));\n"
    #else
        "   vec2 rawpos = vec2(inpos.x, min(inpos.y, 1.0F));\n"
    #endif
    #ifdef WIDEMIDI
        "   rawpos.x = rawpos.x * 0.5F;\n"
    #endif
    #ifdef GLOW
        "   float ill_a = "
    #if defined(TRIPPY) && defined(TRANSFORM)
        "   clamp((-rawpos.y - 0.95F) * 8.0F, 0.0F, 6.0F);\n"
    #elif defined(TRANSFORM)
        "   clamp((-rawpos.y - 0.95F) * 32.0F, 1.0F, 6.0F);\n"
    #elif defined(TRIPPY) && !defined(HDR)
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
    #ifdef TRANSFORM
        "   const float PI2 = 3.1415926535897932384626433832795 / 2.0;\n"
        "   float z = min((1.0F - pos.y) * 0.5F, 1.0F);\n"
        //"   vec2 vtxpos = vec2(pos.x * ((3.0F - pos.y) * 0.25F), (-z*z*2.0F) + 1.0F);\n"
        //"   vec2 vtxpos = vec2(pos.x * ((3.0F - max(pos.y, -1.0F)) * 0.25F), (-pos.y*pos.y*2.0F) + 1.0F);\n"
        "   vec2 vtxpos = vec2(pos.x * ((3.0F - max(pos.y, -1.0F)) * 0.25F), ((cos(pos.y)-0.52F) * 4.0F) - 1.16F);\n"
        "   zbuf = z;\n"
    #else
        "   vec2 vtxpos = pos;\n"
    #endif
    #if defined(ROUNDEDGE) || defined(GLOWEDGE)
        "   ppos = vtxpos;\n"
        "   fpos = vtxpos;\n"
    #endif
    #ifdef KEYBOARD
    #ifdef ROTAT
        "   vtxpos.y = max((vtxpos.y * 0.8F) + 0.2F, -0.7F);\n"
    #else
        "   vtxpos.y = (vtxpos.y * 0.8F) + 0.2F;\n"
    #endif
        //"   vtxpos.y = pow(vtxpos.y + 1.0F, 1.4F) - 1.0F;"
    #endif
    #ifdef ROTAT
        "   const float PI = 3.1415926535897932384626433832795;\n"
        "   vtxpos = vec2(sin((vtxpos.x + 1.0F) * PI), cos((vtxpos.x + 1.0F) * PI) * (16.0F / 9.0F)) * (((vtxpos.y + 1.0F) * 0.75F) - 0.05F);\n"
    #endif
    #ifdef WOBBLE
        "   vec2 interm = vec2(vtxpos.x + cos(intime*4.0F + vtxpos.x * 8.0F)*0.025F + sin(vtxpos.y + cos(intime)*0.25F * 4.0F)*0.125F, vtxpos.y + cos((sin(intime * 0.8F) + vtxpos.y) * 2.0F)*0.25F + sin(vtxpos.x * 4.0F)*0.125F + sin(vtxpos.y*4.0F + intime*2.769F)*0.025F);\n"
    #ifdef WOBBLE_INTERP
        "   float interp_amt = pos.y + 0.02F;\n"
        "   vtxpos.xy = mix(interm, vtxpos, min(-interp_amt * interp_amt * interp_amt, 1.0F));\n"
    #else
        "   vtxpos.xy = interm;\n"
    #endif
    #endif
    #ifdef HDR
        "   npos = vtxpos.xy;\n"
        #if 0 && defined(TRIPPY)
            #ifdef GLOW
            "    illum = clamp(((-rawpos.y - 0.95F) * 32.0F), 0.0F, 5.6F);\n"
            #endif
        #endif
    #endif
    #ifdef TRANSFORM
        "   gl_Position = vec4(vtxpos.xy, -z, 1.0F);\n"
    #else
        "   gl_Position = vec4(vtxpos.xy, 0.0F, 1.0F);\n"
    #endif
        "   trigpos_v = vtxpos.xy;\n"
        "   trigpos = vtxpos.xy;\n"
        "}\n"
        ;

    #if !defined(HDR) || defined(GLOWEDGE) || defined(ROUNDEDGE)
    const char* shaderb =
    #else
    const char* shaderb[] =
    {
    #endif
        "#version 330 core\n"
        "in vec2 trigpos_v;\n"
        "flat in vec2 trigpos;\n"
    #ifdef SHTIME
        "uniform float intime;\n"
    #endif
    #ifndef PFAKEY
        "flat "
    #endif
        "in vec4 pcolor;\n"
    #ifdef TRANSFORM
        "in float zbuf;\n"
    #endif
    #if defined(ROUNDEDGE) || defined(GLOWEDGE)
        "in vec2 ppos;\n"
        "flat in vec2 fpos;\n"
        "vec3 saturate(vec3 c, float amt)\n"
        "{\n"
        "    vec3 gray = vec3(dot(vec3(0.2126F,0.7152F,0.0722F), c));\n"
        "    return vec3(mix(gray, c, amt));\n"
        "}\n"
    #endif
    #ifdef NOISEOVERLAY
"\
vec3 permute(vec3 x) { return mod(((x*34.0)+1.0)*x, 289.0); }\
\
float snoise(vec2 v){\
  const vec4 C = vec4(0.211324865405187, 0.366025403784439,\
           -0.577350269189626, 0.024390243902439);\
  vec2 i  = floor(v + dot(v, C.yy) );\
  vec2 x0 = v -   i + dot(i, C.xx);\
  vec2 i1;\
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);\
  vec4 x12 = x0.xyxy + C.xxzz;\
  x12.xy -= i1;\
  i = mod(i, 289.0);\
  vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))\
  + i.x + vec3(0.0, i1.x, 1.0 ));\
  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy),\
    dot(x12.zw,x12.zw)), 0.0);\
  m = m*m ;\
  m = m*m ;\
  vec3 x = 2.0 * fract(p * C.www) - 1.0;\
  vec3 h = abs(x) - 0.5;\
  vec3 ox = floor(x + 0.5);\
  vec3 a0 = x - ox;\
  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );\
  vec3 g;\
  g.x  = a0.x  * x0.x  + h.x  * x0.y;\
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;\
  return 130.0 * dot(m, g);\
}\
"
"\
vec2 rotate(vec2 v, float a)\
{\
	float s = sin(a);\
	float c = cos(a);\
	mat2 m = mat2(c, -s, s, c);\
	return m * v;\
}\
"
"\
const mat3 rgb2yiq = mat3(0.299, 0.596, 0.211,\
                    0.587, -0.274, -0.523,\
                    0.114, -0.322, 0.312);\
const mat3 yiq2rgb = mat3(1, 1, 1,\
                    0.956, -0.272, -1.106,\
                    0.621, -0.647, 1.703);\
vec3 rotat(vec3 col, float rota)\
{\
    vec3 org = rgb2yiq * col;\
    vec3 rot = vec3(org.x, rotate(org.yz, rota));\
    vec3 res = yiq2rgb * rot;\
    return res;\
}\
vec3 rotat_yuv(vec3 col, float y, float uv, float rota)\
{\
    vec3 org = rgb2yiq * col;\
    vec3 rot = vec3(org.x * y, rotate(org.yz, rota) * uv);\
    vec3 res = yiq2rgb * rot;\
    return res;\
}\
"
    #endif
    #ifdef HDR
        "in vec2 npos;\n"
        #ifdef TRIPPY
        "in float illum;\n"
        "uniform float notemix;\n"
        #endif
        #ifdef WIDEMIDI
        "uniform float lighta[256];\n"
        "uniform vec4  lightc[256];\n"
        #else
        "uniform float lighta[128];\n"
        "uniform vec4  lightc[128];\n"
        #endif
        "uniform sampler2D blurtex;\n"
        "vec2 texres = vec2(1.0F / 1280.0F, 1.0F / 720.0F);\n"
        "vec4 blur(sampler2D tex, vec2 uv)\n"
        "{\n"
        "    vec4 color = vec4(0.0F);\n"
        "    vec2 blurk[3];\n"
        "    blurk[0] = 1.4117647058823530F * texres;\n"
        "    blurk[1] = 3.2941176470588234F * texres;\n"
        "    blurk[2] = 5.1764705882352940F * texres;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y - blurk[2].y)) * 0.015302F;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x - blurk[0].x, uv.y - blurk[1].y)) * 0.024972F;\n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y - blurk[1].y)) * 0.028224F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[0].x, uv.y - blurk[1].y)) * 0.024972F;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x - blurk[1].x, uv.y - blurk[0].y)) * 0.024972F;\n"
        "    color += texture2D(tex, vec2(uv.x - blurk[0].x, uv.y - blurk[0].y)) * 0.036054F;\n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y - blurk[0].y)) * 0.040749F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[0].x, uv.y - blurk[0].y)) * 0.036054F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[1].x, uv.y - blurk[0].y)) * 0.024972F;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x - blurk[2].x, uv.y             )) * 0.015302F;\n"
        "    color += texture2D(tex, vec2(uv.x - blurk[1].x, uv.y             )) * 0.028224F;\n"
        "    color += texture2D(tex, vec2(uv.x - blurk[0].x, uv.y             )) * 0.040749F;\n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y             )) * 0.046056F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[0].x, uv.y             )) * 0.040749F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[1].x, uv.y             )) * 0.028224F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[2].x, uv.y             )) * 0.015302F;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x - blurk[1].x, uv.y + blurk[0].y)) * 0.024972F;\n"
        "    color += texture2D(tex, vec2(uv.x - blurk[0].x, uv.y + blurk[0].y)) * 0.036054F;\n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y + blurk[0].y)) * 0.040749F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[0].x, uv.y + blurk[0].y)) * 0.036054F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[1].x, uv.y + blurk[0].y)) * 0.024972F;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x - blurk[0].x, uv.y + blurk[1].y)) * 0.024972F;\n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y + blurk[1].y)) * 0.028224F;\n"
        "    color += texture2D(tex, vec2(uv.x + blurk[0].x, uv.y + blurk[1].y)) * 0.024972F;\n"
        "    \n"
        "    color += texture2D(tex, vec2(uv.x,              uv.y + blurk[2].y)) * 0.015302F;\n"
        "    \n"
        "    return color;\n"
        "}\n"
    #endif
        "out vec4 outcolor;\n"
        "void main()\n"
        "{\n"
    #if defined(ROUNDEDGE)
        "   vec2 rpos = ppos - fpos;\n"
        "   float af = (64.0F * abs(rpos.x*rpos.y)) + ((rpos.x*rpos.x)+(rpos.y*rpos.y));\n"
        "   float ua = af * 1024.0F;\n"
        "   float a = clamp(ua, 0.0F, 1.0F);\n"
        "   outcolor = vec4(pcolor.xyz, pcolor.w * a)"
        #ifdef TRANSFORM
            " * (0.2F + zbuf)"
        #endif
        ";\n"
    #elif defined(GLOWEDGE)
        "   vec2 rpos = abs(ppos - fpos);\n"
        "   float af = rpos.x * rpos.y;\n"
        "   float ua = af * 1024.0F;\n"
        "   float a = clamp(1.0F - (ua * ua), 0.0F, 1.0F);\n"
        "   outcolor = vec4(saturate(pcolor.xyz, ((a * a)) + 1.0F), pcolor.w)"
        #ifdef TRANSFORM
            " * (0.2F + zbuf)"
        #endif
        ";\n"
    #elif defined(HDR)
        "   outcolor = pcolor;//blur(blurtex, gl_FragCoord.xy * texres) + pcolor;\n"
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
        #ifdef ROTAT
        "       const float PI = 3.1415926535897932384626433832795;\n"
        "       float xpos = (((i - 128) / 64.0F) + 1.0F + (1.0F / 151.0F)) * PI;\n"
        "       vec2 posdiff = npos - vec2(sin(xpos) * -0.25F, cos(xpos) * -0.25F * (16.0F / 9.0F));\n"
        #else
        "       vec2 posdiff = npos - vec2(((i - 128) / 64.0F) + 1.0F + (1.0F / 151.0F), -0.6F);\n"
        #endif
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
        "   float sosi = dot(lightcolor.xyz, pcolor.www) * 16.0F;\n"
        //"   float sosi = max(1.0F, illum);\n"
        //"   sosi = clamp(sosi, 0.08F, 2.4F);\n"
        "   sosi = clamp(sosi, 0.08F, 1.0F);\n"
        "   outcolor = vec4("
                //"(lightcolor.xyz * notemix) +"
                "(pcolor.xyz * vec3(mix(sosi, 1.0F, notea)))"
                " * (((("
                        "pow(lightcolor.xyz, vec3(1.8F))"
                        " * vec3(32.0F))"
                    " + vec3((1.0F / 64.0F))"
                    ") * vec3(notea)) + vec3(notemix))"
            ", outcolor.w);\n"
            #ifdef TRANSFORM
            "   if(notemix != 0.0F)\n"
            "   {\n"
            "       outcolor.xyz *= (0.0078125F + (zbuf*zbuf*zbuf));\n"
            "   }\n" 
            #endif
        
        /*
        "   outcolor = vec4("
        //"(lightcolor.xyz * notemix) +"
        "(pcolor.xyz * vec3((notemix * sosi) + 1.0F)) * (((pow(lightcolor.xyz, vec3((notea * -0.25F) + 1.0F)) * ((notea * 0.4F) + 1.0F)) * vec3(notea)) + ((1.0F / 32.0F))), outcolor.w);\n"
        */
        #elif defined(TRANSFORM)
        "   outcolor = vec4(outcolor.xyz * (0.2F + (zbuf*zbuf)), outcolor.w);\n"
        #else
        "   outcolor += lightcolor;\n"
        #endif
        #ifdef NOKEYBOARD
            #if !defined(ROTAT)
            "   outcolor = vec4(mix(outcolor.xyz, lightcolor.xyz, clamp((-npos.y - 0.58F) * 32.0F, 0.0F, 1.0F)), outcolor.w);\n"
            //#elif !(defined(HDR) && defined(TRIPPY))
            #else
            "   vec2 corrpos = vec2(npos.x, npos.y * (9.0F / 16.0F));\n"
            "   outcolor = vec4(mix(outcolor.xyz, lightcolor.xyz, clamp((0.09F - dot(corrpos, corrpos)) * 32.0F, 0.0F, 1.0F)), outcolor.w);\n"
            #endif
        #endif
        ,
        "",
    #else
        "   outcolor = vec4(pcolor.xyz"
        #ifdef TRANSFORM
            " * (0.2F + (zbuf*zbuf))"
        #endif
        ", pcolor.w);\n"
    #endif
    #ifdef NOISEOVERLAY
        //"   float coy = ((gl_FragCoord.y / 720.0F) - 0.5F) * 2.0F;\n"
        "   float coy = trigpos_v.y;\n"
        "   float cox = trigpos_v.x;\n"
        #ifdef SHTIME
        "   coy += intime;\n"
        "   cox += sin(intime * 2.0F) * (1.0F / 32.0F);\n"
        "   cox += sin(coy * 0.8F) * (1.0F / 16.0F);\n"
        "   float cys = coy - (trigpos.y * 1.02F);\n"
        #else
        "   float cys = coy - (trigpos.y * 1.1F);\n"
        #endif
        "   vec2 colorpos = vec2((cox * 2.0F) + ((cos(coy * 2.6F) + sin(cys * 1.1F)) * (1.0F / 16.0F)), cys + ((sin(coy * 1.4F) + 1.1F) * 0.0625));\n"
        //"   float nr = snoise(colorpos * vec2(4.0F));"
        "   float nr = clamp(snoise(colorpos * vec2(16.0F)) + snoise(vec2(0.341F, 0.897F) + colorpos * vec2(7.6F)), 0.0F, 1.0F);"
        "   outcolor = vec4(rotat(outcolor.xyz, nr), outcolor.w);\n"
        //"   outcolor = vec4(rotat_yuv(outcolor.xyz, (1.0F + (nr * 0.5F)), 1.0F, 0.0F), outcolor.w);\n"
        //"   outcolor = pcolor * length(colorpos);\n"
    #endif
        "}\n"
    #if defined(HDR) && !defined(GLOWEDGE) && !defined(ROUNDEDGE)
    }
    #endif
    ;
    GLint stat = 0;
    
    glShaderSource(vsh, 1, &shadera, 0);
    glCompileShader(vsh);
    glGetShaderiv(vsh, GL_COMPILE_STATUS, &stat);
    //if(stat != 1)
        grhPrintShaderInfo(vsh);
    
    #if defined(HDR) && !defined(GLOWEDGE) && !defined(ROUNDEDGE)
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
    glShaderSource(psh, 3, shaderb, 0);
    #else
    glShaderSource(psh, 1, &shaderb, 0);
    #endif
    glCompileShader(psh);
    glGetShaderiv(psh, GL_COMPILE_STATUS, &stat);
    //if(stat != 1)
        grhPrintShaderInfo(psh);
    
    sh = glCreateProgram();
    
    glAttachShader(sh, vsh);
    glAttachShader(sh, psh);
    
    glLinkProgram(sh);
    
    glGetProgramiv(sh, GL_LINK_STATUS, &stat);
    //if(stat != 1)
    {
        glGetProgramiv(sh, GL_INFO_LOG_LENGTH, &stat);
        if(stat > 0)
        {
            char ilog[256];
            GLsizei outloglen = 0;
            glGetProgramInfoLog(sh, 256, &outloglen, ilog);
            printf("%*.*s\n", outloglen, outloglen, ilog);
        }
        else
        {
            //puts("Unknown shader program error");
        }
        
    }
    
    attrVertex = glGetAttribLocation(sh, "inpos");
    attrColor = glGetAttribLocation(sh, "incolor");
    
    if(attrVertex < 0)
        puts("inpos not found");
    if(attrColor < 0)
        puts("incolor not found");
    
    while(attrVertex < 0 || attrColor < 0)
        ;
    
    #ifdef SHTIME
    uniTime = glGetUniformLocation(sh, "intime");
    #endif
    
    #ifdef HDR
    uniLightAlpha = glGetUniformLocation(sh, "lighta");
    uniLightColor = glGetUniformLocation(sh, "lightc");
        #ifdef TRIPPY
        attrNotemix = glGetUniformLocation(sh, "notemix");
        #endif
    #endif
    
    glDetachShader(sh, psh);
    glDetachShader(sh, vsh);
    
    glDeleteShader(vsh);
    glDeleteShader(psh);
}
