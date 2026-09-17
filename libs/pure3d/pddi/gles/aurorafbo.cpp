//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

#include <pddi/gles/aurorafbo.hpp>

#include <SDL.h>
#include <string.h>

pglAuroraFBO* pglAuroraFBO::instance = NULL;

static const char* auroraBlitVertexShader =
    "attribute vec2 aPos;\n"
    "attribute vec2 aUV;\n"
    "varying vec2 vUV;\n"
    "uniform mat2 uRot;\n"
    "void main() {\n"
    "    vUV = aUV;\n"
    "    vec2 p = uRot * aPos;\n"
    "    gl_Position = vec4(p.x, p.y, 0.0, 1.0);\n"
    "}\n";

static const char* auroraBlitFragmentShader =
    "precision mediump float;\n"
    "varying vec2 vUV;\n"
    "uniform sampler2D uTex;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(uTex, vUV);\n"
    "}\n";

static const GLfloat auroraBlitQuad[] =
{
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 1.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 0.0f,
};

pglAuroraFBO* pglAuroraFBO::GetInstance()
{
    if(!instance)
        instance = new pglAuroraFBO();
    return instance;
}

void pglAuroraFBO::DestroyInstance()
{
    delete instance;
    instance = NULL;
}

pglAuroraFBO::pglAuroraFBO()
{
    ready = false;
    packedDepthStencil = false;
    scale = 1.0f;
    rotation = AURORA_ROTATION_NORMAL;

    realWidth = 0;
    realHeight = 0;
    fboWidth = 0;
    fboHeight = 0;

    framebuffer = 0;
    colorTexture = 0;
    depthStencilBuffer = 0;

    blitProgram = 0;
    blitBuffer = 0;
    blitAttribPos = -1;
    blitAttribUV = -1;
    blitUniformTex = -1;
    blitUniformRot = -1;

    const float rot[AURORA_ROTATION_COUNT][4] =
    {
        {  1.0f,  0.0f,  0.0f,  1.0f },
        {  0.0f,  1.0f, -1.0f,  0.0f },
        { -1.0f,  0.0f,  0.0f, -1.0f },
        {  0.0f, -1.0f,  1.0f,  0.0f },
    };
    memcpy(rotationMatrices, rot, sizeof(rotationMatrices));
}

pglAuroraFBO::~pglAuroraFBO()
{
    Destroy();
}

bool pglAuroraFBO::Create(int width, int height)
{
    Destroy();

    if(!BuildBlitResources())
        return false;

    return Resize(width, height);
}

void pglAuroraFBO::Destroy()
{
    DestroyTargets();

    if(blitBuffer)
    {
        glDeleteBuffers(1, &blitBuffer);
        blitBuffer = 0;
    }
    if(blitProgram)
    {
        glDeleteProgram(blitProgram);
        blitProgram = 0;
    }

    ready = false;
}

bool pglAuroraFBO::Resize(int width, int height)
{
    int targetWidth = (int)((float)width * scale);
    int targetHeight = (int)((float)height * scale);

    if(targetWidth < 1)
        targetWidth = 1;
    if(targetHeight < 1)
        targetHeight = 1;

    DestroyTargets();

    ready = BuildTargets(targetWidth, targetHeight);

    return ready;
}

void pglAuroraFBO::Bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
}

void pglAuroraFBO::Unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void pglAuroraFBO::Blit()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, realWidth, realHeight);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    glUseProgram(blitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glUniform1i(blitUniformTex, 0);
    glUniformMatrix2fv(blitUniformRot, 1, GL_FALSE, rotationMatrices[rotation]);

    glBindBuffer(GL_ARRAY_BUFFER, blitBuffer);
    glEnableVertexAttribArray((GLuint)blitAttribPos);
    glEnableVertexAttribArray((GLuint)blitAttribUV);
    glVertexAttribPointer((GLuint)blitAttribPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid*)0);
    glVertexAttribPointer((GLuint)blitAttribUV, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid*)(2 * sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray((GLuint)blitAttribPos);
    glDisableVertexAttribArray((GLuint)blitAttribUV);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArrayOES(0);
}

void pglAuroraFBO::SetScale(float s)
{
    if(s <= 0.0f)
        s = 1.0f;
    scale = s;
}

void pglAuroraFBO::SetRotation(AuroraRotation r)
{
    if(r < AURORA_ROTATION_NORMAL || r >= AURORA_ROTATION_COUNT)
        r = AURORA_ROTATION_NORMAL;
    rotation = r;
}

void pglAuroraFBO::SetRealSize(int w, int h)
{
    realWidth = w;
    realHeight = h;
}

bool pglAuroraFBO::BuildBlitResources()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &auroraBlitVertexShader, 0);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &auroraBlitFragmentShader, 0);
    glCompileShader(fragmentShader);

    blitProgram = glCreateProgram();
    glAttachShader(blitProgram, vertexShader);
    glAttachShader(blitProgram, fragmentShader);
    glLinkProgram(blitProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = GL_FALSE;
    glGetProgramiv(blitProgram, GL_LINK_STATUS, &linked);
    if(linked == GL_FALSE)
    {
        SDL_Log("AuroraFBO: blit shader link failed");
        glDeleteProgram(blitProgram);
        blitProgram = 0;
        return false;
    }

    blitAttribPos = glGetAttribLocation(blitProgram, "aPos");
    blitAttribUV = glGetAttribLocation(blitProgram, "aUV");
    blitUniformTex = glGetUniformLocation(blitProgram, "uTex");
    blitUniformRot = glGetUniformLocation(blitProgram, "uRot");

    glGenBuffers(1, &blitBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, blitBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(auroraBlitQuad), auroraBlitQuad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

bool pglAuroraFBO::BuildTargets(int width, int height)
{
    packedDepthStencil = SDL_GL_ExtensionSupported("GL_OES_packed_depth_stencil") != SDL_FALSE;

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

    const char* depthVariant;
    if(packedDepthStencil)
    {
        glGenRenderbuffers(1, &depthStencilBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilBuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilBuffer);
        depthVariant = "GL_DEPTH24_STENCIL8 (packed depth+stencil)";
    }
    else
    {
        glGenRenderbuffers(1, &depthStencilBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilBuffer);
        depthVariant = "GL_DEPTH_COMPONENT16 (depth only)";
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    SDL_Log("AuroraFBO: created %dx%d FBO, depth variant: %s, glCheckFramebufferStatus: 0x%04x (%s)",
        width, height, depthVariant, status,
        status == GL_FRAMEBUFFER_COMPLETE ? "GL_FRAMEBUFFER_COMPLETE" : "INCOMPLETE");

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    fboWidth = width;
    fboHeight = height;

    return status == GL_FRAMEBUFFER_COMPLETE;
}

void pglAuroraFBO::DestroyTargets()
{
    if(depthStencilBuffer)
    {
        glDeleteRenderbuffers(1, &depthStencilBuffer);
        depthStencilBuffer = 0;
    }
    if(colorTexture)
    {
        glDeleteTextures(1, &colorTexture);
        colorTexture = 0;
    }
    if(framebuffer)
    {
        glDeleteFramebuffers(1, &framebuffer);
        framebuffer = 0;
    }

    fboWidth = 0;
    fboHeight = 0;
    ready = false;
}
