//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#ifndef _AURORAFBO_HPP_
#define _AURORAFBO_HPP_

#include <pddi/gles/gl.hpp>

class pglAuroraFBO
{
public:
    // values match wl_output_transform
    enum AuroraRotation
    {
        AURORA_ROTATION_NORMAL = 0,
        AURORA_ROTATION_90     = 1,
        AURORA_ROTATION_180    = 2,
        AURORA_ROTATION_270    = 3,
        AURORA_ROTATION_COUNT  = 4
    };

    static pglAuroraFBO* GetInstance();
    static void DestroyInstance();

    bool Create(int width, int height);
    void Destroy();
    bool Resize(int width, int height);

    void Bind();
    void Unbind();
    void Blit();

    bool IsReady() const { return ready; }

    GLuint GetTexture() const { return colorTexture; }
    int GetWidth() const { return fboWidth; }
    int GetHeight() const { return fboHeight; }

    void SetScale(float s);
    float GetScale() const { return scale; }

    void SetRotation(AuroraRotation r);
    AuroraRotation GetRotation() const { return rotation; }

    void SetRealSize(int w, int h);
    int GetRealWidth() const { return realWidth; }
    int GetRealHeight() const { return realHeight; }

private:
    pglAuroraFBO();
    ~pglAuroraFBO();

    bool BuildBlitResources();
    bool BuildTargets(int width, int height);
    void DestroyTargets();

    static pglAuroraFBO* instance;

    bool ready;
    bool packedDepthStencil;
    float scale;
    AuroraRotation rotation;

    int realWidth;
    int realHeight;
    int fboWidth;
    int fboHeight;

    GLuint framebuffer;
    GLuint colorTexture;
    GLuint depthStencilBuffer;

    GLuint blitProgram;
    GLuint blitBuffer;
    GLint blitAttribPos;
    GLint blitAttribUV;
    GLint blitUniformTex;
    GLint blitUniformRot;
    float rotationMatrices[AURORA_ROTATION_COUNT][4];
};

#endif
