/* Copyright (c) 1996-2014 Clickteam
*
* This source code is part of the iOS exporter for Clickteam Multimedia Fusion 2
* and Clickteam Fusion 2.5.
*
* Permission is hereby granted to any person obtaining a legal copy
* of Clickteam Multimedia Fusion 2 or Clickteam Fusion 2.5 to use or modify this source
* code for debugging, optimizing, or customizing applications created with
* Clickteam Multimedia Fusion 2 and/or Clickteam Fusion 2.5.
* Any other use of this source code is prohibited.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/
//
//  CRenderer.cpp
//  RuntimeIPhone
//
//  Created by Anders Riggelsen on 8/20/13.
//  Copyright (c) 2013 Clickteam. All rights reserved.
//


#import "CRenderer.h"

#import "CoreMath.h"
#import "CServices.h"
#import "CShader.h"
#import "CRunView.h"
#import "CRunApp.h"
#import "CRun.h"
#import "CArrayList.h"
#import "CRunFrame.h"
#import "CBitmap.h"
#import "CLayer.h"

static CRenderer* ssRenderer;

#define CHECK_GL_ERROR() ({ GLenum __error = glGetError(); if(__error) printf("OpenGL error 0x%04X in %s\n", __error, __FUNCTION__); (__error ? NO : YES); })



CRenderer::CRenderer(CRunView* runView)
{
    lock = [[NSObject alloc] init];
    currentShader = NULL;
    defaultShader = NULL;
    gradientShader = NULL;
    currentLayer = NULL;
    
    effectShader = NULL;
    perspectiveShader = NULL;
    sinewaveShader = NULL;

    iShader = -1;
    
    //Do set opengl3 or 2 with apple suggested code
    
    
    context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    bOpenGL2 = false;
    if(context == nil)
    {
        context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
        bOpenGL2 = true;
    }
    
    if (!context)
        return;
    if(![EAGLContext setCurrentContext:context])
        return;

    view = runView;
    windowSize = CGSizeMake(view->appRect.size.width, view->appRect.size.height);
    topLeft = CGPointMake(0, 0);
    texturesToRemove = [[CArrayList alloc] init];

    ssRenderer = this;
      
    // Create default framebuffer object. The backing will be allocated for the current layer in -resizeFromLayer
    
    glGenFramebuffers(1, &defaultFramebuffer);
    glGenRenderbuffers(1, &colorRenderbuffer);

    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRenderbuffer);

    glGenRenderbuffers(1, &stencilbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, stencilbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, (int)windowSize.width, (int)windowSize.height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencilbuffer);
    glEnable(GL_BLEND);
    usesBlending = YES;
    usesScissor = NO;

    usedTextures = [[NSMutableSet alloc] init];
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

    bool flipProjection = false;
    currentRenderState.framebuffer = defaultFramebuffer;
    currentRenderState.viewport = Viewport(0, 0, view->appRect.size.width, view->appRect.size.height);
    currentRenderState.transform = currentTransform = Mat3f::identity();
    currentRenderState.projection = Mat3f::orthogonalProjectionMatrix(0, 0, view->appRect.size.width, view->appRect.size.height, flipProjection);
    currentRenderState.actProjstate = flipProjection;
    currentRenderState.oldProjstate = flipProjection;
    
    screenViewport = Viewport(0, 0, view->appRect.size.width, view->appRect.size.height);
    // Adding new Rtt
    renderScene.x = 0;
    renderScene.y = 0;
    renderScene.width = view->appRect.size.width;
    renderScene.height= view->appRect.size.height;
    
    renderScene.frameRtt = [[CRenderToTexture alloc] initWithWidth:renderScene.width Height:renderScene.height withRenderer:this andSwapMode:NO andSwapProjection:YES];
    renderScene.layerRtt = [[CRenderToTexture alloc] initWithWidth:renderScene.width Height:renderScene.height withRenderer:this andSwapMode:NO andSwapProjection:YES];
    renderScene.back1Rtt = [[CRenderToTexture alloc] initWithWidth:renderScene.width Height:renderScene.height withRenderer:this andSwapMode:NO andSwapProjection:YES];

    //
    //Vertices:
    
    GLfloat vertices[8] = {
        0.0f,    0.0f,
        1.0f,    0.0f,
        0.0f,    1.0f,
        1.0f,    1.0f
    };
  
    
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);
    
    defaultShader = new CShader(this);
    gradientShader = new CShader(this);
    defaultEllipseShader = new CShader(this);
    gradientEllipseShader = new CShader(this);
    solidColorShader = new CShader(this);
    
    defaultShader->loadShader(@"default", true, false);
    gradientShader->loadShader(@"gradient", false, true);
    defaultEllipseShader->loadShader(@"defaultEllipse", true, false);
    gradientEllipseShader->loadShader(@"gradientEllipse", false, true);
    solidColorShader->loadShader(@"solidColor", true, false);
    
    backingProgram = -1;
    
    originX = originY = 0;
    scaleX = scaleY = 1.0;
    downScaling.scale = 1;
    downScaling.active= false;
    
    supportedExtensions = [[NSMutableSet alloc] init];
    NSString* extensionString = [NSString stringWithUTF8String:(char *)glGetString(GL_EXTENSIONS)];
    NSArray* extensions = [extensionString componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    for(NSString* oneExtension in extensions)
    {
        if(![oneExtension isEqualToString:@""])
            [supportedExtensions addObject:oneExtension];
    }

    printf("OpenGL version is %s.\n", (char *)glGetString(GL_VERSION));
    printf("GLSL version is %s.\n", (char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
    
    setBlendEquation(GL_FUNC_ADD);
    forgetCachedState();
    checkForError();
}

CRenderer* CRenderer::getRenderer()
{
    return ssRenderer;
}

CRenderer::~CRenderer()
{
    [lock release];
    destroyFrameBuffers();
    delete defaultShader;
    delete gradientShader;
    delete defaultEllipseShader;
    delete gradientEllipseShader;
    delete solidColorShader;
    [texturesToRemove release];

    delete perspectiveShader;
    delete sinewaveShader;
       
    // Tear down context
    if ([EAGLContext currentContext] == context)
        [EAGLContext setCurrentContext:nil];

    [context release];
    context = nil;
}

void CRenderer::pushRenderingState(Vec2f offset)
{
    renderStateStack.push_back(currentRenderState);
    Mat3f offsetMatrix = Mat3f::translationMatrix(offset.x, offset.y);
    setTransformMatrix(Mat3f::multiply(currentRenderState.transform, offsetMatrix));
}

void CRenderer::pushRenderingState()
{
    renderStateStack.push_back(currentRenderState);
}

void CRenderer::popRenderingState()
{
    currentRenderState = renderStateStack.back();
    renderStateStack.pop_back();
    glBindFramebuffer(GL_FRAMEBUFFER, currentRenderState.framebuffer);
    setProjectionMatrix(topLeft.x, topLeft.y, currentRenderState.contentSize.x, currentRenderState.contentSize.y, currentRenderState.actProjstate);
    forgetCachedState();

}

void CRenderer::destroyFrameBuffers()
{
    if (defaultFramebuffer)
    {
        glDeleteFramebuffers(1, &defaultFramebuffer);
        defaultFramebuffer = 0;
    }
    if (colorRenderbuffer)
    {
        glDeleteRenderbuffers(1, &colorRenderbuffer);
        colorRenderbuffer = 0;
    }
    glDeleteVertexArrays(1, &vao);
}

void CRenderer::beginFrame(CRunApp* app) {
    renderScene.onLayer = false;
    renderScene.onAlpha = false;
    renderScene.onScreen= false;
    renderScene.onFrame = true;
    bPreMultiply = app->bPremultiply;
    renderScene.bEffLayers = app->frame->hasLayerEffects;
    renderScene.bEffFrame  = app->frame->hasFrameEffect;
    if(view->pRunApp->frame != nil)
        renderScene.color = BGRtoARGB(view->pRunApp->frame->leBackground);
    else
        renderScene.color= BGRtoARGB(view->pRunApp->gaBorderColour);
    renderScene.alpha = 255;
    currentProgram = 0;
    
    [renderScene.frameRtt bindFrameBuffer];
    [renderScene.frameRtt setResampling:NO];
    [renderScene.frameRtt fillWithColor:renderScene.color andAlpha:renderScene.alpha];

    
    if(bPreMultiply)
        setBlendFunction(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    else
        setBlendFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void CRenderer::beginLayer(int inkEffect, bool needBackground)
{
    if(renderScene.layerRtt != nullptr) {
        renderScene.onLayer = true;
        renderScene.onAlpha = false;
        renderScene.onFrame = false;
        renderScene.needBck = needBackground;
        if(needBackground)
            copyRttToRtt(renderScene.frameRtt, renderScene.back1Rtt, BOP_BLEND_REPLACETRANSP, -1, false);
        
        [renderScene.layerRtt bindFrameBuffer];
        [renderScene.layerRtt clearWithAlphaDontBind:0.0f];
        
        if((inkEffect & BOP_MASK) == BOP_EFFECTEX) {
            renderScene.onAlpha = true;
            setBlendFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }
}

void CRenderer::layerToFrame(int shaderIndex, int inkEffect, int inkEffectParam)
{
    if(renderScene.layerRtt != nullptr) {
        [renderScene.layerRtt unbindFrameBuffer];
        
        renderScene.onLayer = false;
        renderScene.onAlpha = false;
        renderScene.onFrame = true;

        int width = renderScene.width;
        int height= renderScene.height;

        CRenderToTexture *pRtt = renderScene.layerRtt;

        setEffectShaderByIndex(shaderIndex);
        pushAndResetTransform();
        setInkBlendEffect(inkEffect, inkEffectParam, effectShader);
        
        if(currentShader->hasPixelSize)
        {
            currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (pRtt->width));
            currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (pRtt->height));
        }
        currentShader->setPreMultiply(false);
        if(currentShader->hasBackground)
            currentShader->getBackground(0, 0, width, height);
        
        currentShader->setTexture(pRtt);
        currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(0,0), Vec2f(width,height), Vec2fZero));
        draw();
        if(currentShader->hasBackground)
            currentShader->releaseBackground();
        if(effectShader != nullptr)
            removeEffectShader();
        popTransform();
    }
}

void CRenderer::renderAScene(CRunFrame* frame)
{
    if(renderScene.frameRtt != nullptr) {

        [renderScene.frameRtt unbindFrameBuffer];

        CRenderToTexture* pRtt = renderScene.frameRtt;
        int x = renderScene.x;
        int y = renderScene.y;
        int width = renderScene.width;
        int height= renderScene.height;

        renderColor(x, y, width, height, frame->leBackground);
        setEffectShaderByIndex(frame->effectShader);
        setInkBlendEffect(frame->effect, frame->effectParam, effectShader);
        if(currentShader->hasPixelSize)
        {
            currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (pRtt->width));
            currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (pRtt->height));
        }
        currentShader->setPreMultiply(false);
        if(currentShader->hasBackground)
            currentShader->getBackground(x, y, pRtt->width, pRtt->height);
        currentShader->setTexture(pRtt);
        currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(pRtt->width, pRtt->height), Vec2fZero));
        draw();
        if(currentShader->hasBackground)
            currentShader->releaseBackground();
        if(effectShader != nullptr)
            removeEffectShader();
    }
}

void CRenderer::renderToScreen(bool bResampling, int bckcolor, int shaderIndex, int inkEffect, int inkEffectParam)
{
    if(renderScene.frameRtt != nullptr) {
        [renderScene.frameRtt unbindFrameBuffer];
        
        renderScene.onScreen = true;
        renderScene.onFrame  = false;
        CRenderToTexture* pRtt = renderScene.frameRtt;
        
        int x = renderScene.x;
        int y = renderScene.y;
        int width = renderScene.width;
        int height= renderScene.height;


        clear();
        renderColor(x,y,width,height,renderScene.color);
        setEffectShaderByIndex(shaderIndex);
        setInkBlendEffect(inkEffect, inkEffectParam, effectShader);
        if(currentShader->hasPixelSize)
        {
            currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (pRtt->width));
            currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (pRtt->height));
        }
        currentShader->setPreMultiply(false);
        if(currentShader->hasBackground)
            currentShader->getBackground(x, y, width, height);
        currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(width,height), Vec2fZero));
        currentShader->setTexture(pRtt);
        [pRtt setResampling:bResampling];
        draw();
        if(currentShader->hasBackground)
            currentShader->releaseBackground();
    }
    if(effectShader != nullptr)
        removeEffectShader();
    renderScene.reset();
    currentShader = NULL;

}

void CRenderer::pushScene(int x, int y, int w, int h) {
    sceneStack.push_back(renderScene);
    renderScene.index = getOrCreatePoolSubScene(w, h);
    renderScene.x = x;
    renderScene.y = y;
    renderScene.width = w;
    renderScene.height= h;
    renderScene.subScene = true;

    renderScene.frameRtt = subScenePool[renderScene.index].frameRtt;
    renderScene.layerRtt = subScenePool[renderScene.index].layerRtt;
    renderScene.back1Rtt = subScenePool[renderScene.index].back1Rtt;
    
    GLint sc[4];
    glGetIntegerv(GL_SCISSOR_BOX, sc);
    clipLeft   = sc[0];
    clipBottom = sc[1];
    clipWidth  = sc[2];
    clipHeight = sc[3];
}

void CRenderer::popScene() {
    int index = renderScene.index;
    renderScene = sceneStack.back();
    frameColor = renderScene.color;
    sceneStack.pop_back();
    releasePoolSubScene(index);
}

// Clear the frame, ready for rendering
void CRenderer::clear(float red, float green, float blue)
{
    glClearColor(red, green, blue, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}

void CRenderer::clear()
{
    float r,g,b;
    int background;
    if(view->pRunApp->frame != nil)
        background = BGRtoARGB(view->pRunApp->frame->leBackground);
    else
        background = BGRtoARGB(view->pRunApp->gaBorderColour);

    r = ((background >> 16) & 0xFF)/255.0f;
    g = ((background >> 8) & 0xFF)/255.0f;
    b = (background & 0xFF)/255.0f;
    clear(r,g,b);
}

void CRenderer::clearWithRunApp(CRunApp* app)
{
    float r,g,b;
    int background;
    if(app->frame != nil)
        background = BGRtoARGB(app->frame->leBackground);
    else
        background = BGRtoARGB(app->gaBorderColour);

    r = ((background >> 16) & 0xFF)/255.0f;
    g = ((background >> 8) & 0xFF)/255.0f;
    b = (background & 0xFF)/255.0f;
    clear(r,g,b);
}

void CRenderer::swapBuffers()
{
    [EAGLContext setCurrentContext:context];
    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER];
    popRenderingState();
}

void CRenderer::flush()
{
    glFlush();
}

void CRenderer::checkForError()
{
    GLenum err = glGetError();
    if (GL_NO_ERROR != err)
        NSLog(@"Got OpenGL Error: %i", err);
}

void CRenderer::forgetShader()
{
    if(currentShader != NULL)
    {
        CShader* shaderToBindAgain = currentShader;
        currentShader = NULL;
        shaderToBindAgain->bindShader();
    }
}

void CRenderer::forgetCachedState()
{
    currentTextureID = -1;
    currentBlendEquationA = currentBlendEquationB = currentBlendFunctionA = currentBlendFunctionB = currentBlendFunctionC = currentBlendFunctionD =-1;
    cR = cG = cB = cA = 1.0f;
    currentViewport = Viewport(0,0,0,0);

    if(defaultShader != NULL)
        defaultShader->forgetCachedState();
    if(gradientShader != NULL)
        gradientShader->forgetCachedState();
    if(defaultEllipseShader != NULL)
        defaultEllipseShader->forgetCachedState();
    if(gradientEllipseShader != NULL)
        gradientEllipseShader->forgetCachedState();
    if(solidColorShader != NULL)
        solidColorShader->forgetCachedState();
    
    if(perspectiveShader != NULL)
        perspectiveShader->forgetCachedState();
    if(sinewaveShader != NULL)
        sinewaveShader->forgetCachedState();
    
    shaders.askEffectsToForgetCache();
    
}

void CRenderer::setCurrentShader(CShader* shader)
{
    if(shader == NULL)
        return;
    
    currentShader = shader;
    currentShader->bindShader();
}


void CRenderer::renderLine(Vec2f pA, Vec2f pB, int color, float thickness)
{
    float angle = atan2(pB.y-pA.y, pB.x-pA.x);
    float length = pA.distanceTo(pB);
    Mat3f lineMatrix = Mat3f::objectRotationMatrix(pA, Vec2f(length, thickness), Vec2fOne, Vec2f(0, thickness/2), -radiansToDegrees(angle));

    //Update shader information
    setInkBlendEffect(0, 0, gradientShader);
    currentShader->setGradientColors(color);
    currentShader->setObjectMatrix(lineMatrix);
    currentShader->bindVertexArray();
    draw();
    if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
        drawBackground(false, false);
}

void CRenderer::renderRect(int x, int y, int w, int h, int color, int thickness, int inkEffect, int inkEffectParam)
{
    GradientColor gradient = GradientColor(color);
    renderGradient(gradient, x, y, w - thickness, thickness, -1, inkEffect, inkEffectParam);
    renderGradient(gradient, x, y, thickness, h - thickness, -1, inkEffect, inkEffectParam);
    renderGradient(gradient, x, y + h - thickness, w - thickness, thickness, -1, inkEffect, inkEffectParam);
    renderGradient(gradient, x + w - thickness, y, thickness, h, -1, inkEffect, inkEffectParam);

}

void CRenderer::renderBackground(int x, int y, int w, int h)
{
    CShader* holdEffect = effectShader;
    
    GLuint buffer =currentRenderState.framebuffer;
    CRenderToTexture * pRtt = nullptr;
    if(renderScene.onLayer)
        pRtt = renderScene.back1Rtt;
    else
        pRtt = renderScene.frameRtt;
    GLint old_active = -1;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
    glBindFramebuffer(GL_FRAMEBUFFER, pRtt->framebuffer);
    glActiveTexture(GL_TEXTURE0);
    
    int mode = GL_RGBA;
    auto* texture = getOrCreatePoolCTexture(w, h, false);
    glBindTexture(GL_TEXTURE_2D, texture->textureId);
    
    if (!pRtt->flipProjection) {
        glCopyTexImage2D(GL_TEXTURE_2D, 0, mode, x, renderScene.height - y - h, w, h, 0);
    } else {
        glCopyTexImage2D(GL_TEXTURE_2D, 0, mode, x, y, w, h, 0);
    }
    glActiveTexture(old_active);
    //Restore drawing framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, buffer);
   
    // Draw background
    setInkBlendEffect(BOP_LAYER, -1, defaultShader);
    currentShader->setPreMultiply(false);
    if(!pRtt->flipProjection)
    {
        Mat3f texMatrix = pRtt->textureMatrix;
        texMatrix = texMatrix.flippedTexCoord(false, true);
        currentShader->setTexture(pRtt, texMatrix);
    }
    else
    {
        currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(w,h), Vec2fZero));
        currentShader->setTexture(pRtt);
    }
    draw();
    releasePoolCTexture(texture);
    effectShader = holdEffect;
}

void CRenderer::draw() {
    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
}

void CRenderer::drawBackground(bool clearBck, bool flip) {
    /**
     * Draw over background FBO
     */
     if(renderScene.back1Rtt != nullptr) {
         [renderScene.back1Rtt bindFrameBuffer];
         if (clearBck)
            [renderScene.back1Rtt clearWithAlphaDontBind:0.0f];

         glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
         [renderScene.back1Rtt unbindFrameBuffer];
     }
}

void CRenderer::renderColor(int x, int y, int w, int h, int color)
{
    CShader * holdEffect = effectShader;
    setInkBlendEffect(BOP_COPY, -1, solidColorShader);
    currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(w,h), Vec2fZero));
    GLfloat colors[4] = {GetRed(color), GetGreen(color), GetBlue(color), 1.0f};
    currentShader->setVariable4f("color", colors[0], colors[1], colors[2], colors[3]);
    currentShader->bindVertexArray();
    draw();
    effectShader = holdEffect;
}

//Renders the given image with the previously defined shaders and settings.
void CRenderer::renderSimpleImage(int x, int y, int w, int h)
{
    currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(w,h), Vec2fZero));
    currentShader->bindVertexArray();
    draw();
    if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
        drawBackground(false, false);
    if(currentShader->hasBackground)
        currentShader->releaseBackground();
}

//The most common image rendering method.
void CRenderer::renderImage(CTexture* image, int x, int y, int w, int h, int shaderIndex, int inkEffect, int inkEffectParam)
{
    uploadTexture(image);
    setEffectShaderByIndex(shaderIndex);
    setInkBlendEffect(inkEffect, inkEffectParam, effectShader);
    if(currentShader->hasPixelSize)
    {
        currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (w));
        currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (h));
    }
    if(currentShader->hasBackground)
        currentShader->getBackground(x, y, w, h);
    currentShader->setTexture(image);
    currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(w,h), Vec2fZero));
    currentShader->bindVertexArray();
    draw();
    if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
        drawBackground(false, false);
    if(currentShader->hasBackground)
        currentShader->releaseBackground();
    
}

//The most common image rendering method.
void CRenderer::renderScaledRotatedImage(CTexture* image, float angle, float sX, float sY, int hX, int hY, int x, int y, int w, int h, int shaderIndex, int inkEffect, int inkEffectParam)
{
    uploadTexture(image);
    setEffectShaderByIndex(shaderIndex);
    setInkBlendEffect(inkEffect, inkEffectParam, effectShader);
    if(currentShader->hasPixelSize)
    {
        currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (w));
        currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (h));
    }
    if(currentShader->hasBackground)
        currentShader->getBackground(x-round(hX*sX), y-round(hY*sY), round(w*sX), round(h*sY));
    currentShader->setTexture(image);
    currentShader->setObjectMatrix(Mat3f::objectRotationMatrix(Vec2f(x,y), Vec2f(w,h), Vec2f(sX,sY), Vec2f(hX, hY), angle));
    currentShader->bindVertexArray();
    draw();
    if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
        drawBackground(false, false);
    if(currentShader->hasBackground)
        currentShader->releaseBackground();
    if(effectShader != nil)
        removeEffectShader();
}

//Render Images with wrap and rotation
void CRenderer::renderScaledRotatedImageWrapAndFlip(CTexture* image, float angle, float sX, float sY, int hX, int hY, int x, int y, int w, int h,
                                                    int offsetX, int offsetY, int wrap, int flipH, int flipV, int resample, int transp, int shaderIndex, int inkEffect, int inkEffectParam)
{
    bool withOffset = false;
    float offX = 0;
    float offY = 0;

    if (offsetX != 0 || offsetY != 0) {
        withOffset = true;

        offX = (float) ((int) (offsetX) % w);
        offY = (float) ((int) (offsetY) % h);
    }
    [image uploadTexture];
    setEffectShaderByIndex(shaderIndex);
    setInkBlendEffect(inkEffect, inkEffectParam, effectShader);
    if(currentShader->hasPixelSize)
    {
        currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (w));
        currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (h));
    }
    currentShader->setPreMultiply(false);
    if(currentShader->hasBackground)
        currentShader->getBackground(x-round(hX*sX), y-round(hY*sY), round(w*sX), round(h*sY));

    if (withOffset) {
        if (wrap != 0) {
            resample = false;
            [image updateTextureMode:GL_REPEAT];
        }
    }
    image->resample = resample;
    [image updateFilterNoBind];
    
    Mat3f texMatrix = Mat3f::textureMatrix(offX, offY, w, h, image->originalWidth, image->originalHeight);
    texMatrix.flippedTexCoord(flipH, flipV);
    currentShader->setTexture(image, texMatrix);
    currentShader->setObjectMatrix(Mat3f::objectRotationMatrix(Vec2f(x,y), Vec2f(w,h), Vec2f(sX,sY), Vec2f(hX, hY), angle));
    currentShader->bindVertexArray();
    draw();
    if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
        drawBackground(false, false);
    if(currentShader->hasBackground)
        currentShader->releaseBackground();
    if(effectShader != nil)
        removeEffectShader();
    [image updateTextureMode:GL_CLAMP_TO_EDGE];
}

//Renders a tiled picture with clipping.
void CRenderer::renderPattern(CTexture* image, int x, int y, int w, int h, int shaderIndex, int inkEffect, int inkEffectParam, bool flipX, bool flipY, float scaleX, float scaleY)
{
    CRect visibleRect  = currentLayer->visibleRect;

    //Limit the amount of repetitions to what only is visible
    int startX = x;
    int startY = y;
    int endX = x+w;
    int endY = y+h;

    //Update shader information
    [image uploadTexture];
   
    int iw = image->width * scaleX;
    int ih = image->height * scaleY;
    int tw = image->textureWidth;
    int th = image->textureHeight;

    if(startX < -iw)
        startX %= iw;

    if(startY < -ih)
        startY %= ih;

    if(endX > visibleRect.right)
        endX = (endX-visibleRect.width()) % iw + visibleRect.right;

    if(endY > visibleRect.bottom)
        endY = (endY-visibleRect.height()) % ih + visibleRect.bottom;

    w = endX - startX;
    h = endY - startY;
    setEffectShaderByIndex(shaderIndex);
    setInkBlendEffect(inkEffect, inkEffectParam, effectShader);
    int wMiW = w % iw;
    int hMiH = h % ih;

    int lastX = endX - wMiW;
    int lastY = endY - hMiH;

    BOOL xDivisible = (wMiW == 0);
    BOOL yDivisible = (hMiH == 0);

    BOOL flipped = flipX || flipY;

    //Texture coordinate matrices
    Mat3f normalTexCoord = image->textureMatrix;
    Mat3f current = normalTexCoord;

    float rx, ry;
    for(int cY=startY; cY<endY; cY+=ih)
    {
        for(int cX=startX; cX<endX; cX+=iw)
        {
            int drawWidth = iw;
            int drawHeight = ih;

            current = normalTexCoord;

            if(cX==lastX && !xDivisible)
            {
                drawWidth = wMiW;
                rx = drawWidth/(float)(tw*scaleX);
                current.a = rx;
            }

            if(cY==lastY && !yDivisible)
            {
                drawHeight = hMiH;
                ry = drawHeight/(float)(th*scaleY);
                current.e = ry;
            }

            if(flipped)
                current = current.flippedTexCoord(flipX, flipY);

            currentShader->setTexCoord(current);
            currentShader->setTexture(image);
            if(currentShader->hasPixelSize)
            {
                currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (drawWidth));
                currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (drawHeight));
            }
            currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(cX,cY), Vec2f(drawWidth,drawHeight), Vec2fZero));
            if(currentShader->hasBackground)
                currentShader->getBackground(cX, cY, drawWidth,drawHeight);
            currentShader->bindVertexArray();
            draw();
            if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
                drawBackground(false, false);
            if(currentShader->hasBackground)
                currentShader->releaseBackground();
        }
    }
}


//Renders a tiled picture with clipping.
void CRenderer::renderPattern(CTexture* image, int x, int y, int w, int h, int shaderIndex, int inkEffect, int inkEffectParam)
{
    //Use the fast rendering mode if the image is Power-Of-Two sized
    if([image imageIsPOT] || !bOpenGL2)
    {
        [image expectTilableImage];
        [image uploadTexture];
        setEffectShaderByIndex(shaderIndex);
        setInkBlendEffect(inkEffect, inkEffectParam, effectShader);
        if(currentShader->hasPixelSize)
        {
            currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (w));
            currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (h));
        }
        if(currentShader->hasBackground)
            currentShader->getBackground(x, y, w, h);
        Mat3f texMatrix = Mat3f::textureMatrix(0, 0, w, h, image->originalWidth, image->originalHeight);
        currentShader->setTexture(image, texMatrix);
        currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(w,h), Vec2fZero));
        currentShader->bindVertexArray();
        draw();
        if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
            drawBackground(false, false);
        if(currentShader->hasBackground)
            currentShader->releaseBackground();
        
    }
    else
    {
        //Use the slow but pixel perfect rendering option
        renderPattern(image, x, y, w, h, shaderIndex, inkEffect, inkEffectParam, false, false);
    }
}

//Renders a tiled picture with ellipse clipping.
void CRenderer::renderPatternEllipse(CTexture* image, int x, int y, int w, int h, int shaderIndex, int inkEffect, int inkEffectParam, bool flipX, bool flipY, float scaleX, float scaleY)
{
    CRect visibleRect  = currentLayer->visibleRect;

    //Limit the amount of repetitions to what only is visible
    int startX = x;
    int startY = y;
    int endX = x+w;
    int endY = y+h;

    //Update shader information
    [image uploadTexture];
    CRenderToTexture* rtt = nullptr;
    setEffectShaderByIndex(shaderIndex);
    if(effectShader != nullptr)
    {
        rtt = [[CRenderToTexture alloc] initWithWidth:w Height:h withRenderer:this andSwapMode:NO andSwapProjection:YES];
        [rtt bindFrameBuffer];
        pushAndResetTransform();
        [rtt clearWithAlphaDontBind:0];
    }
    setInkBlendEffect(inkEffect, inkEffectParam, defaultEllipseShader);
    currentShader->setTexture(image);
    currentShader->bindVertexArray();
   
    int iw = image->width * scaleX;
    int ih = image->height * scaleY;
    int tw = image->textureWidth;
    int th = image->textureHeight;

    if(startX < -iw)
        startX %= iw;

    if(startY < -ih)
        startY %= ih;

    if(endX > visibleRect.right)
        endX = (endX-visibleRect.width()) % iw + visibleRect.right;

    if(endY > visibleRect.bottom)
        endY = (endY-visibleRect.height()) % ih + visibleRect.bottom;

    w = endX - startX;
    h = endY - startY;
    
    int wMiW = w % iw;
    int hMiH = h % ih;

    int lastX = endX - wMiW;
    int lastY = endY - hMiH;

    BOOL xDivisible = (wMiW == 0);
    BOOL yDivisible = (hMiH == 0);

    BOOL flipped = flipX || flipY;

    //Texture coordinate matrices
    Mat3f normalTexCoord = image->textureMatrix;
    Mat3f current = normalTexCoord;
    int xo = startX;
    int yo = startY;
    if(rtt != nullptr)
    {
        xo = yo = 0;
        endX = w;
        endY = y;
    }
    float rx, ry;
    for(int cY=yo; cY<endY; cY+=ih)
    {
        for(int cX=xo; cX<endX; cX+=iw)
        {
            int drawWidth = iw;
            int drawHeight = ih;

            current = normalTexCoord;

            if(cX==lastX && !xDivisible)
            {
                drawWidth = wMiW;
                rx = drawWidth/(float)(tw*scaleX);
                current.a = rx;
            }

            if(cY==lastY && !yDivisible)
            {
                drawHeight = hMiH;
                ry = drawHeight/(float)(th*scaleY);
                current.e = ry;
            }

            if(flipped)
                current = current.flippedTexCoord(flipX, flipY);

            currentShader->setTexCoord(current);
            currentShader->setEllipseCenter(xo + w / 2, yo + h / 2, w / 2, h / 2);
            currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(cX,cY), Vec2f(drawWidth,drawHeight), Vec2fZero));
            currentShader->bindVertexArray();
            draw();
            if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
                drawBackground(false, false);
        }
    }
    if(rtt != nullptr)
    {
        [rtt unbindFrameBuffer];
        popTransform();
        setInkBlendEffect(inkEffect, inkEffectParam, effectShader);
        if(currentShader->hasPixelSize) {
            currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (w));
            currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (h));
        }
        currentShader->setPreMultiply(false);
        if(currentShader->hasBackground)
            currentShader->getBackground(x, y, w, h);
        currentShader->setTexture(rtt);
        currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(w,h), Vec2fZero));
        currentShader->bindVertexArray();
        draw();
        if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
            drawBackground(false, false);
        if(currentShader->hasBackground)
            currentShader->releaseBackground();
        [rtt release];
    }
}

//Renders a tiled picture with ellipse clipping.
void CRenderer::renderPatternEllipse(CTexture* image, int x, int y, int w, int h, int shaderIndex, int inkEffect, int inkEffectParam)
{
    //Use the fast rendering mode if the image is Power-Of-Two sized
    if([image imageIsPOT] || !bOpenGL2)
    {
        [image expectTilableImage];
        [image uploadTexture];
        CRenderToTexture* rtt = nullptr;
        setEffectShaderByIndex(shaderIndex);
        if(effectShader != nullptr)
        {
            rtt = [[CRenderToTexture alloc] initWithWidth:w Height:h withRenderer:this andSwapMode:NO andSwapProjection:YES];
            [rtt bindFrameBuffer];
            pushAndResetTransform();
            [rtt clearWithAlphaDontBind:0];
        }
        setInkBlendEffect(inkEffect, inkEffectParam, defaultEllipseShader);
        currentShader->setPreMultiply(false);
        int xo = x;
        int yo = y;
        if(rtt != nullptr)
        {
            xo = yo = 0;
        }
        
        Mat3f texMatrix = Mat3f::textureMatrix(0, 0, w, h, image->originalWidth, image->originalHeight);
        currentShader->setTexture(image, texMatrix);
        currentShader->setEllipseCenter(xo + w / 2, yo + h / 2, w / 2, h / 2);
        currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(xo,yo), Vec2f(w,h), Vec2fZero));
        currentShader->bindVertexArray();
        draw();
        if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
            drawBackground(false, false);
        if(rtt != nullptr)
        {
            [rtt unbindFrameBuffer];
            popTransform();
            setInkBlendEffect(inkEffect, inkEffectParam, effectShader);
            if(currentShader->hasPixelSize) {
                currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (w));
                currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (h));
            }
            currentShader->setPreMultiply(false);
            if(currentShader->hasBackground)
                currentShader->getBackground(x, y, w, h);
            currentShader->setTexture(rtt);
            currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(w,h), Vec2fZero));
            currentShader->bindVertexArray();
            draw();
            if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
                drawBackground(false, false);
            if(currentShader->hasBackground)
                currentShader->releaseBackground();
            [rtt release];
        }
    }
    else
    {
        //Use the slow but pixel perfect rendering option
        renderPatternEllipse(image, x, y, w, h, shaderIndex, inkEffect, inkEffectParam, false, false);
    }
}

void CRenderer::renderSolidColor(int color, int x, int y, int w, int h, int inkEffect, int inkEffectParam)
{
    renderGradient(GradientColor(color), x, y, w, h, -1, inkEffect, inkEffectParam);
}

void CRenderer::renderSolidColor(int color, CRect rect, int inkEffect, int inkEffectParam)
{
    renderGradient(GradientColor(color), (int)rect.left, (int)rect.top, (int)rect.width(), (int)rect.height(), -1, inkEffect, inkEffectParam);
}

//The perspective rendering method.
void CRenderer::renderPerspective(CRenderToTexture* image, int x, int y, int w, int h, float fA, float fB, int dir, int inkEffect, int inkEffectParam)
{
    if(perspectiveShader == NULL)
    {
        perspectiveShader = new CShader(this);
        bool wait = perspectiveShader->loadShader(@"perspective_ext", true, false);
        sleep(.005);
        if(wait)
        {
            GLuint pers_program = perspectiveShader->program;
            perspectiveShader->uniforms[UNIFORM_VAR1] = glGetUniformLocation(pers_program, "fA");
            perspectiveShader->uniforms[UNIFORM_VAR2] = glGetUniformLocation(pers_program, "fB");
            perspectiveShader->uniforms[UNIFORM_VAR3] = glGetUniformLocation(pers_program, "pDir");
            if(!perspectiveShader->linkProgram(pers_program))
            {
                perspectiveShader = NULL;
                return;
            }
        }
    }
    
    if(perspectiveShader == NULL || perspectiveShader->program <= 0)
        return;
    
    setInkBlendEffect(inkEffect, inkEffectParam, perspectiveShader);

    currentShader->setVariable1f("fA", fA);
    currentShader->setVariable1f("fB", fB);
    currentShader->setVariable1i("pDir", dir);
    //Also You can send variable using the uniform array
    //perspectiveShader->setVariable1f(perspectiveShader->uniforms[UNIFORM_VAR1], fA);
    //perspectiveShader->setVariable1f(perspectiveShader->uniforms[UNIFORM_VAR2], fB);
    //perspectiveShader->setVariable1i(perspectiveShader->uniforms[UNIFORM_VAR3], dir);
    
    uploadTexture(image);
 
    //currentShader->setTexture(image);
    Mat3f texMatrix = image->textureMatrix;
    texMatrix.flippedTexCoord(false, true);
    currentShader->setTexture(image, texMatrix);
    currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(w,h), Vec2fZero));
    currentShader->bindVertexArray();
    draw();
    if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
        drawBackground(false, false);
}

//The sinewave rendering method.
void CRenderer::renderSinewave(CRenderToTexture* image, int x, int y, int w, int h, float zoom, float wave, float offset, int dir, int inkEffect, int inkEffectParam)
{
    if(!sinewaveShader)
    {
        sinewaveShader = new CShader(this);
        bool wait = sinewaveShader->loadShader(@"sinewave_ext", true, false);
        sleep(.005);
        if(wait)
        {
            GLuint sine_program = sinewaveShader->program;
            sinewaveShader->uniforms[UNIFORM_VAR1] = glGetUniformLocation(sine_program, "Zoom");
            sinewaveShader->uniforms[UNIFORM_VAR2] = glGetUniformLocation(sine_program, "Wave");
            sinewaveShader->uniforms[UNIFORM_VAR3] = glGetUniformLocation(sine_program, "OffSet");
            sinewaveShader->uniforms[UNIFORM_VAR4] = glGetUniformLocation(sine_program, "pDir");
            if(!sinewaveShader->linkProgram(sine_program))
            {
                sinewaveShader = NULL;
                return;
            }
        }
    }
    if(!sinewaveShader || sinewaveShader->program <= 0)
        return;
    
    setInkBlendEffect(inkEffect, inkEffectParam, sinewaveShader);

    currentShader->setVariable1f("Zoom", zoom);
    currentShader->setVariable1f("Wave", wave);
    currentShader->setVariable1f("OffSet", offset);
    currentShader->setVariable1i("pDir", dir);
    
    uploadTexture(image);
    
    currentShader->setTexture(image);
    currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(w,h), Vec2fZero));
    currentShader->bindVertexArray();
    draw();
    if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
        drawBackground(false, false);
}


void CRenderer::setOrigin(int x, int y)
{
    originX = x;
    originY = y;
}

//Blit wrappers for the transition system
void CRenderer::renderBlitFull(CRenderToTexture* source)
{
    renderStretch(source, 0, 0, source->width, source->height, 0, 0, source->width, source->height);
}

void CRenderer::renderBlit(CRenderToTexture* source, int xDst, int yDst, int xSrc, int ySrc, int width, int height)
{
    renderStretch(source, xDst, yDst, width, height, xSrc, ySrc, width, height);
}

void CRenderer::renderFade(CRenderToTexture* source, int alpha)
{
    renderStretch(source, 0, 0, source->width, source->height, 0, 0, source->width, source->height, -1, 1, alpha/2);
}

void CRenderer::renderStretch(CRenderToTexture* source, int xDst, int yDst, int wDst, int hDst, int xSrc, int ySrc, int wSrc, int hSrc, int shaderIndex, int inkEffect, int inkEffectParam)
{
    uploadTexture(source);

    if(currentRenderState.framebuffer == defaultFramebuffer)
    {
        xDst += originX;
        yDst += originY;
    }

    Mat3f texCoord = Mat3f::textureMatrixFlipped(xSrc, ySrc, wSrc, hSrc, source->height, source->textureWidth, source->textureHeight);
    Mat3f objectPos = Mat3f::objectMatrix(Vec2f(xDst, yDst), Vec2f(wDst, hDst), Vec2fZero);
    setEffectShaderByIndex(shaderIndex);
    setInkBlendEffect(inkEffect, inkEffectParam, effectShader);
    if(currentShader->hasPixelSize)
    {
        currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (wSrc));
        currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (hSrc));
    }
    if(currentShader->hasBackground)
        currentShader->getBackground(xSrc, ySrc, wSrc, hSrc);
    currentShader->setPreMultiply(false);
    currentShader->setTexture(source, texCoord);
    currentShader->setObjectMatrix(objectPos);
    currentShader->bindVertexArray();
    draw();
    if(renderScene.onLayer || renderScene.onAlpha)
        drawBackground(false, false);
    if(currentShader->hasBackground)
        currentShader->releaseBackground();
}

void CRenderer::renderGradient(GradientColor gradient, int x, int y, int w, int h, int shaderIndex, int inkEffect, int inkEffectParam)
{
    if(w == 0 || h == 0)
        return;
    
    float alpha;
    unsigned int rgbaCoeff = inkEffectParam;
    if((inkEffect & BOP_MASK)==BOP_EFFECTEX || (inkEffect & BOP_RGBAFILTER) != 0)
    {
        alpha = (rgbaCoeff >> 24)/255.0f;
    }
    else
    {
        if(inkEffectParam == -1)
            alpha = 1.0f;
        else
            alpha = 1.0f - inkEffectParam/128.0f;
    }
    gradient.a.a = alpha;
    gradient.b.a = alpha;
    gradient.c.a = alpha;
    gradient.d.a = alpha;

    CRenderToTexture* rtt = nullptr;
    setEffectShaderByIndex(shaderIndex);
    if(effectShader != nullptr)
    {
        rtt = [[CRenderToTexture alloc] initWithWidth:w Height:h withRenderer:this andSwapMode:NO andSwapProjection:YES];
        [rtt bindFrameBuffer];
        pushAndResetTransform();
        [rtt clearWithAlphaDontBind:0];
    }
    setInkBlendEffect(BOP_BLEND, inkEffectParam, gradientShader);
    currentShader->setPreMultiply(false);
    currentShader->setGradientColors(gradient);

    int xo = x;
    int yo = y;

    if(rtt != nullptr)
        xo = yo = 0;
    currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(xo,yo), Vec2f(w,h), Vec2fZero));
    currentShader->bindVertexArray();
    draw();
    if(!rtt && renderScene.onLayer)
        drawBackground(false, false);
    if(rtt != nullptr)
    {
        [rtt unbindFrameBuffer];
        popTransform();
        setInkBlendEffect(inkEffect, inkEffectParam, effectShader);
        if(currentShader->hasPixelSize)
        {
            currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (w));
            currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (h));
        }
        if(currentShader->hasBackground)
            currentShader->getBackground(x, y, w, h);
        currentShader->setPreMultiply(false);
        currentShader->setTexture(rtt);
        currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(w,h), Vec2fZero));
        draw();
        if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
            drawBackground(false, false);
        if(currentShader->hasBackground)
            currentShader->releaseBackground();
        [rtt release];
    }
}

void CRenderer::renderGradientEllipse(GradientColor gradient, int x, int y, int w, int h, int shaderIndex, int inkEffect, int inkEffectParam)
{
    if(w == 0 || h == 0)
        return;
    
    float alpha;
    unsigned int rgbaCoeff = inkEffectParam;
    if((inkEffect & BOP_MASK)==BOP_EFFECTEX || (inkEffect & BOP_RGBAFILTER) != 0)
    {
        alpha = (rgbaCoeff >> 24)/255.0f;
    }
    else
    {
        if(inkEffectParam == -1)
            alpha = 1.0f;
        else
            alpha = 1.0f - inkEffectParam/128.0f;
    }
    gradient.a.a = alpha;
    gradient.b.a = alpha;
    gradient.c.a = alpha;
    gradient.d.a = alpha;

    CRenderToTexture* rtt = nullptr;
    setEffectShaderByIndex(shaderIndex);
    if(effectShader != nullptr)
    {
        rtt = [[CRenderToTexture alloc] initWithWidth:w Height:h withRenderer:this andSwapMode:NO andSwapProjection:YES];
        [rtt bindFrameBuffer];
        pushAndResetTransform();
        [rtt clearWithAlphaDontBind:0];
    }
    setInkBlendEffect(inkEffect, inkEffectParam, gradientEllipseShader);
    currentShader->setPreMultiply(false);
    int xo = x;
    int yo = y;

    if(rtt != nullptr)
        xo = yo = 0;
    currentShader->setEllipseCenter(xo + w / 2, yo + h / 2, w / 2, h / 2);
    currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(xo,yo), Vec2f(w,h), Vec2fZero));
    currentShader->setGradientColors(gradient);
    currentShader->bindVertexArray();
    draw();
    if(!rtt && renderScene.onLayer)
        drawBackground(false, false);
    if(rtt != nullptr)
    {
        [rtt unbindFrameBuffer];
        popTransform();
        setInkBlendEffect(inkEffect, inkEffectParam, effectShader);
        if(currentShader->hasPixelSize)
        {
            currentShader->setVariable1f("fPixelWidth", 1.0f / (float) (w));
            currentShader->setVariable1f("fPixelHeight", 1.0f / (float) (h));
        }
        if(currentShader->hasBackground)
            currentShader->getBackground(x, y, w, h);
        currentShader->setPreMultiply(false);
        currentShader->setTexture(rtt);
        currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(x,y), Vec2f(w,h), Vec2fZero));
        draw();
        if(renderScene.needBck && (renderScene.onAlpha || renderScene.onLayer))
            drawBackground(false, false);
        if(currentShader->hasBackground)
            currentShader->releaseBackground();
        [rtt release];
    }
}

void CRenderer::renderGradient(GradientColor gradient, CRect rect, int inkEffect, int inkEffectParam)
{
    renderGradient(gradient, (int)rect.left, (int)rect.top, (int)rect.width(), (int)rect.height(), -1, inkEffect, inkEffectParam);
}

void CRenderer::useBlending(BOOL useBlending)
{
    if(useBlending)
    {
        if(!usesBlending)
        {
            glEnable(GL_BLEND);
            usesBlending = YES;
        }
    }
    else
    {
        if(usesBlending)
        {
            glDisable(GL_BLEND);
            usesBlending = NO;
        }
    }
}

BOOL CRenderer::getBlending()
{
    return usesBlending;
}

void CRenderer::updateViewport()
{
    setViewport(Viewport(0,0,backingWidth,backingHeight));
}

void CRenderer::setViewport(Viewport viewport)
{
    if(currentViewport != viewport)
    {
        glViewport(viewport.position.x, viewport.position.y, viewport.size.x, viewport.size.y);
        currentViewport = viewport;
    }
}

void CRenderer::setCurrentLayer(CLayer *layer)
{
    currentLayer = layer;
}

void CRenderer::resetTransform() {
    currentRenderState.transform = Mat3f::identity();
}

void CRenderer::setCurrentLayerByBool(bool mode)
{
    if(!mode)
        currentRenderState.transform = Mat3f::identity();
    else
        currentRenderState.transform = currentTransform;
}

void CRenderer::setLayerTransform()
{
    currentTransform = currentLayer != nil ? [currentLayer getTransformMatrix] : Mat3f::identity();
    setTransformMatrix( currentTransform );
}

void CRenderer::pushAndResetTransform()
{
    transformStack.push_back(currentRenderState.transform);
    currentRenderState.transform = Mat3f::identity();
}

void CRenderer::popTransform()
{
    currentRenderState.transform = transformStack.back();
    transformStack.pop_back();
}

void CRenderer::uploadTexture(CTexture* texture)
{
    if(texture == nil)
        return;
    texture->usageCount++;
    
    if(texture->textureId != -1)
        return;
    
    textureUsage += [texture uploadTexture];

    //Add texture to set of used textures if it has a valid image-bank handle
    if(texture->bPrunable && texture->handle != (unsigned short)-1 && [usedTextures containsObject:texture] == NO)
        [usedTextures addObject:texture];
}

void CRenderer::removeTexture(CTexture* texture, BOOL cleanMemory)
{
    if(texture == nil)
        return;
    textureUsage -= [texture deleteTexture];
    if(cleanMemory)
        [texture cleanMemory];

    if([usedTextures containsObject:texture] == YES)
        [usedTextures removeObject:texture];
}

void CRenderer::cleanMemory()
{
    NSEnumerator* enumerator = [usedTextures objectEnumerator];
    id value;
    while ((value = [enumerator nextObject]))
    {
        CTexture* texture = value;
        textureUsage -= [texture deleteTexture];
        texture->usageCount = 0;
    }
    [usedTextures removeAllObjects];
}


//Generates a list of all unused textures over a timespan of 10 seconds
//The renderer will release these textures spread out over the next 10 seconds for minimal speed impact
void CRenderer::cleanUnused()
{
    @synchronized(lock) {
        //NSLog(@"Texture usage: %f MB", (textureUsage/1024.0f)/1024.0f);
        
        //Clean prune list just to be sure no textures are added twice:
        [texturesToRemove clear];
        
        NSEnumerator* enumerator = [usedTextures objectEnumerator];
        id value;
        int cnt = 0;
        while ((value = [enumerator nextObject]))
        {
            CTexture* texture = value;
            if(texture->usageCount == 0)
                [texturesToRemove add:(void*)texture];
            texture->usageCount = 0;
            cnt++;
        }
        //NSLog(@"Generated clean list of %i entries and used textures is %i", [texturesToRemove size], cnt);
    }
}

//Remove one unused texture at a time.
void CRenderer::pruneTexture()
{
    int index = [texturesToRemove size]-1;
    if(index >= 0)
    {
        CTexture* texture = (CTexture*)[texturesToRemove get:index];
        //Recheck that the texture wasn't used
        if(texture != nil && texture->usageCount == 0)
            removeTexture(texture, true);

        [texturesToRemove removeIndex:index];
        //NSLog(@"Pruned texture %@", texture);
    }
}

void CRenderer::clearPruneList()
{
    [texturesToRemove clear];
}

void CRenderer::setClip(int x, int y, int w, int h)
{
    int currentWidth = currentRenderState.framebufferSize.x;
    int currentHeight = currentRenderState.framebufferSize.y;

    w = MIN(currentWidth,w);
    h = MIN(currentHeight,h);
    x = MAX(0,x);
    y = MAX(0,y);

    if(!(usesScissor=glIsEnabled(GL_SCISSOR_TEST)))
    {
        glEnable(GL_SCISSOR_TEST);
        usesScissor = YES;
    }
    glScissor(x, backingHeight-y-h, w, h);
}

void CRenderer::resetClip()
{
    if(usesScissor)
    {
        glDisable(GL_SCISSOR_TEST);
        usesScissor = NO;
    }
}

void CRenderer::setBlendEquation(GLenum equation)
{
    if(currentBlendEquationA != equation)
    {
        currentBlendEquationA = equation;
        glBlendEquation(equation);
    }
}

void CRenderer::setBlendEquationSeparate(GLenum equationA, GLenum equationB)
{
    if(currentBlendEquationA != equationA || equationB != currentBlendEquationB)
    {
        currentBlendEquationA = equationA;
        currentBlendEquationB = equationB;
        glBlendEquationSeparate(equationA, equationB);
    }
}

void CRenderer::setBlendFunction(GLenum sFactor, GLenum dFactor)
{
    if(currentBlendFunctionA != sFactor || currentBlendFunctionB != dFactor)
    {
        currentBlendFunctionA = sFactor;
        currentBlendFunctionB = dFactor;
        glBlendFunc(sFactor, dFactor);
    }
}

void CRenderer::setBlendFunctionSeparate(GLenum sFactorRGB, GLenum dFactorRGB, GLenum sFactorAlpha, GLenum dFactorAlpha) {
    if (currentBlendFunctionA != sFactorRGB
        || currentBlendFunctionB != dFactorRGB
           || currentBlendFunctionC != sFactorAlpha
           || currentBlendFunctionD != dFactorAlpha) {

        currentBlendFunctionA = sFactorRGB;
        currentBlendFunctionB = dFactorRGB;
        currentBlendFunctionC = sFactorAlpha;
        currentBlendFunctionD = dFactorAlpha;

        glBlendFuncSeparate(sFactorRGB, dFactorRGB, sFactorAlpha , dFactorAlpha);
    }
}

void CRenderer::setBlendColor(float red, float green, float blue, float alpha)
{
    if(cA != alpha || cR != red || cG != green || cB != blue)
    {
        cR = red;
        cG = green;
        cB = blue;
        cA = alpha;
    }
}

void CRenderer::bindRenderBuffer()
{
    [EAGLContext setCurrentContext:context];
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
    pushRenderingState();
}

void CRenderer::setInkBlendEffect(int effect, int effectParam, CShader *pShader)
{
    //bool useBasic = true;
    CShader *useShader = defaultShader;
    unsigned int rgbaCoeff;
    float red = 1.0f, green = 1.0f, blue = 1.0f, alpha = 1.0f;

    // filter by masking the cases first
    int maskedEffect = effect & BOP_MASK;

    if (maskedEffect == BOP_EFFECTEX) {
        // Handle BOP_EFFECTEX with the same block
        effect = maskedEffect;  // Assign the masked effect directly
        rgbaCoeff = effectParam;

        alpha = (float)(rgbaCoeff >> 24) / 255.0f;

        red   = (float) ((rgbaCoeff >> 16) & 0xFF) / 255.0f;
        green = (float) ((rgbaCoeff >> 8) & 0xFF) / 255.0f;
        blue  = (float) (rgbaCoeff & 0xFF) / 255.0f;
    }
    else if (maskedEffect == BOP_ONE || maskedEffect == BOP_LAYER) {
        // Handle BOP_ONE, and BOP_LAYER with the same block
        effect = maskedEffect;  // Assign the masked effect directly
        rgbaCoeff = effectParam;

        alpha = (float)(rgbaCoeff >> 24) / 255.0f;

        red   = (float) ((rgbaCoeff >> 16) & 0xFF) / 255.0f;
        green = (float) ((rgbaCoeff >> 8) & 0xFF) / 255.0f;
        blue  = (float) (rgbaCoeff & 0xFF) / 255.0f;
    }
    //Extracts the RGB Coefficient
    else if ((effect & BOP_RGBAFILTER) != 0) {
        effect = MAX(effect & BOP_MASK, BOP_BLEND);

        rgbaCoeff = effectParam;
        alpha = (float)(rgbaCoeff >> 24) / 255.0f;

        red   = (float) ((rgbaCoeff >> 16) & 0xFF) / 255.0f;
        green = (float) ((rgbaCoeff >> 8) & 0xFF) / 255.0f;
        blue  = (float) (rgbaCoeff & 0xFF) / 255.0f;

    }
    //Uses the generic INK-effect
    else {
        effect &= BOP_MASK;
        if (effectParam == -1)
            alpha = 1.0f;
        else {
            alpha = 1.0f - (float)effectParam / 128.0f;
        }
    }
    // Use pShader program
    if (pShader) {
        useShader = pShader;
        effect = MAX(effect & BOP_MASK, BOP_BLEND);
    // Use Basic shader or just keep Default
    } else if (effect == 0) {
        //useShader = defaultShader;
        effect = BOP_COPY;
    }

    //log("effectParam: %f", effectParam);

    GLenum equationA = GL_FUNC_ADD;
    GLenum equationB = GL_FUNC_ADD;

    // GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
    GLenum sFactor  = GL_SRC_ALPHA;
    GLenum sFactorA = GL_SRC_ALPHA;
    GLenum dFactor  = GL_ONE_MINUS_SRC_ALPHA;
    GLenum dFactorA = GL_ONE_MINUS_SRC_ALPHA;

    switch (effect) {
        default:
        case BOP_COPY:
        case BOP_BLEND:
        case BOP_BLEND_REPLACETRANSP:
        case BOP_BLEND_DONTREPLACECOLOR:
        case BOP_OR:
        case BOP_XOR:
        case BOP_MONO:
        case BOP_INVERT:
        case BOP_EFFECTEX:
            if(renderScene.onFrame)
            {
                if(!bPreMultiply || alpha != 1.0) {
                    //glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    sFactor  = GL_SRC_ALPHA;
                    sFactorA = GL_ONE;
                }
                else
                {
                    sFactor  = GL_ONE;
                    sFactorA = GL_ONE;
                }
            }
            break;
        case BOP_ADD:
            {
                sFactor = sFactorA = GL_SRC_ALPHA;
                dFactor = dFactorA = GL_ONE;
            }
            break;
        case BOP_SUB:
            {
                equationA = GL_FUNC_REVERSE_SUBTRACT;
                equationB = GL_FUNC_ADD;
                if(alpha != 1.0)
                {
                    sFactor   = GL_SRC_ALPHA;
                    sFactorA  = GL_SRC_ALPHA;
                    dFactor   = GL_ONE;
                    dFactorA  = GL_ONE;
                }
                else
                {
                    sFactor   = GL_ZERO;
                    sFactorA  = GL_ZERO;
                    dFactor   = GL_ONE_MINUS_SRC_COLOR;
                    dFactorA  = GL_ONE;
                }
            }
            break;
        case BOP_LAYER:
            sFactor  = sFactorA = GL_SRC_ALPHA;
            dFactor  = dFactorA = GL_ONE_MINUS_SRC_ALPHA;
            break;

        case BOP_ONE:
            sFactor  = sFactorA = GL_ONE;
            dFactor  = dFactorA = GL_ONE_MINUS_SRC_ALPHA;
            break;
    }

    setBlendEquationSeparate(equationA, equationB);
    setBlendFunctionSeparate(sFactor, dFactor, sFactorA, dFactorA);

    setCurrentShader(useShader);
    currentShader->setInkEffect(effect);
    currentShader->setRGBCoeff(red, green, blue, alpha);
}

void CRenderer::setTransformMatrix(const Mat3f matrix)
{
    currentRenderState.transform = matrix;
    forgetCachedState();
}

void CRenderer::setProjectionMatrix(int x, int y, int width, int height, bool flipY)
{
    currentProjection = Mat3f::orthogonalProjectionMatrix(x, y, width, height, flipY);
    currentRenderState.projection = currentProjection;

    setViewport(Viewport(x, y, width, height));
    forgetCachedState();
}

void CRenderer::flipProjection() {

    bool flipY = !currentRenderState.actProjstate;
    currentRenderState.oldProjstate = currentRenderState.actProjstate;
    currentRenderState.actProjstate = flipY;
    int width = (int)currentRenderState.contentSize.x;
    int height= (int)currentRenderState.contentSize.y;
    currentRenderState.projection = Mat3f::orthogonalProjectionMatrix(0, 0, width, height, flipY);
}

void CRenderer::restoreProjection() {

    if(currentRenderState.oldProjstate != currentRenderState.actProjstate)
    {
        currentRenderState.actProjstate = currentRenderState.oldProjstate;
        int width = (int)currentRenderState.contentSize.x;
        int height= (int)currentRenderState.contentSize.y;
        currentRenderState.projection = Mat3f::orthogonalProjectionMatrix(0,0, width, height,
                                                                     currentRenderState.actProjstate);
    }
}

void CRenderer::setSurfaceTextureAtIndex(CTexture* image, NSString* name, int index)
{
    if(image == NULL || effectShader == NULL)
        return;
    
    if(image->textureId == -1)
        uploadTexture(image);
    
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;
    
   shader->setSurfaceTextureAtIndex(image, [name UTF8String], index);
    
}

void CRenderer::updateSurfaceTexture()
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;
    
   shader->updateSurfaceTexture();

}

bool CRenderer::resizeFromLayer(CAEAGLLayer* layer)
{
    // Allocate color buffer backing based on the current layer size
    [EAGLContext setCurrentContext:context];
    
    layer.bounds = CGRectMake(layer.bounds.origin.x, layer.bounds.origin.y, windowSize.width, windowSize.height);

    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
    
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);

    currentRenderState.framebufferSize = Vec2i(backingWidth, backingHeight);
    currentRenderState.contentSize = Vec2f(backingWidth, backingHeight);
    
    screenViewport = Viewport(0,0, backingWidth, backingHeight);
    
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);

    glBindRenderbuffer(GL_RENDERBUFFER, stencilbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, backingWidth, backingHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencilbuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        NSLog(@"Failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return NO;
    }

    setProjectionMatrix(topLeft.x, topLeft.y, currentRenderState.contentSize.x, currentRenderState.contentSize.y, currentRenderState.actProjstate);
    return YES;
}


void CRenderer::screenAreaToTexture(CRenderToTexture* rtt, int x , int y, int width, int height, int mode)
{
    CShader* holdEffect = effectShader;

    CRenderToTexture * pRtt = nullptr;
    if(renderScene.onLayer)
        pRtt = renderScene.back1Rtt;
    else
        pRtt = renderScene.frameRtt;

    GLint actualFramebuffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &actualFramebuffer);
    
    float in[3] = {0.0f, 0.0f, 1.0f};
    in[0] = x;
    in[1] = y;
    float out[3] = {0.0f, 0.0f, 0.0f};
    Mat3fXVec3f((const float*) &currentRenderState.transform,in, out);
    int transfX = out[0];
    int transfY = out[1];

    if(bOpenGL2)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pRtt->framebuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        
        glBindTexture(GL_TEXTURE_2D, rtt->textureId);
        
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, transfX, transfY, width, height);
    }
    else
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, pRtt->framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rtt->framebuffer);

        // Blit from source to destination
        glBlitFramebuffer(
          transfX, transfY, transfX + width, transfY + height,      // Source rectangle
            0, 0, width, height,      // Destination rectangle
            GL_COLOR_BUFFER_BIT,      // Mask indicating what buffer is to be copied
            GL_NEAREST                // Interpolation method
        );
    }

    // Unbind framebuffers if necessary
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    Mat3f texMatrix = Mat3f::textureMatrix(0, 0, width, height, rtt->originalWidth, rtt->originalHeight);
    rtt->textureMatrix = texMatrix;
    
    //Reset
    glBindTexture(GL_TEXTURE_2D, 0);
    if(pRtt->framebuffer != actualFramebuffer)
        glBindFramebuffer(GL_FRAMEBUFFER,actualFramebuffer);
    
    effectShader = holdEffect;
}

void CRenderer::screenPixelsToTexture(CTexture* texture, int x , int y, int width, int height)
{
    float in[3] = {0.0f, 0.0f, 1.0f};
    in[0] = x;
    in[1] = y;
    float out[3] = {0.0f, 0.0f, 0.0f};
    Mat3fXVec3f((const float*) &currentRenderState.transform,in, out);
    int transfX = out[0];
    int transfY = out[1];
    
    NSInteger dataLen = width * height * 4;
    // allocate array and read pixels into it.
    GLubyte *buffer = (GLubyte *) malloc(dataLen);
    
    glReadPixels(transfX, transfY, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)buffer);
    
    // mirrored to screen
    GLubyte *buffer2 = (GLubyte *) malloc(dataLen);
    if(buffer2 == nil)
    {
        free(buffer);
        return;
    }
    for(int j = 0; j <height; j++)
    {
        for(int i = 0; i <width * 4; i++)
        {
            buffer2[(height - 1 - j) * width * 4 + i] = buffer[j * 4 * width + i];
        }
    }
    
    free(buffer);
    
    glBindTexture(GL_TEXTURE_2D, texture->textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    
    if(texture->textureWidth <= width && texture->textureHeight <= height)
    {
        
        int pixels = texture->textureWidth*texture->textureHeight;
        int newSize = pixels*4;
        
        if(texture->textureWidth == width && texture->textureHeight == height)
        {
            //Texture data can be directly transfered to the graphics card without copying
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->textureWidth, texture->textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer2);
        }
        else
        {
            //Copy to intermediate texture is required
            GLubyte* texData = (GLubyte*)malloc(newSize);
            memset(texData, 0, newSize);
            
            int lineWidth = width*4;
            int bLineWidth = (lineWidth+3) & ~3;
            
            for(int j=0; j<height; ++j)
            memcpy((char*)texData + texture->textureWidth*j*4, (char*)buffer2 + bLineWidth*j, lineWidth);
            
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->textureWidth, texture->textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
            free(texData);
        }
    }
    if(buffer2 != nil)
        free(buffer2);
}

UIImage* CRenderer::screenAreaToImage(int x , int y, int width, int height)
{
    int scale = 1;
    UIScreen* screen = [ UIScreen mainScreen ];
    if ( [ screen respondsToSelector:@selector(scale) ] )
    scale = (int) [ screen scale ];
    
    width *= scale;
    height*= scale;
    NSInteger dataLen = width * height * 4;
    // allocate array and read pixels into it.
    GLubyte *buffer = (GLubyte *) malloc(dataLen);
    
    GLint viewPort[4];
    glGetIntegerv(GL_VIEWPORT, (GLint*)&viewPort);
    
    glReadPixels(x, viewPort[3] - y - height, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)buffer);
    // mirrored to screen
    GLubyte *buffer2 = (GLubyte *) malloc(dataLen);
    if(buffer2 == nil)
    {
        free(buffer);
        return nil;
    }
    for(int j = 0; j <height; j++)
    {
        for(int i = 0; i <width * 4; i++)
        {
            buffer2[(height - 1 - j) * width * 4 + i] = buffer[j * 4 * width + i];
        }
    }
    free(buffer);
    // make data provider with data.
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, buffer2, dataLen, NULL);
    
    int bitsPerComponent = 8;
    int bitsPerPixel = 32;
    int bytesPerRow = 4 * width;
    CGColorSpaceRef colorSpaceRef = CGColorSpaceCreateDeviceRGB();
    CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault;
    CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
    // make the cgimage
    CGImageRef imageRef = CGImageCreate(width, height, bitsPerComponent, bitsPerPixel, bytesPerRow, colorSpaceRef, bitmapInfo, provider, NULL, NO, renderingIntent);
    // then make the uiimage from imageref
    UIImage *myImage = [UIImage imageWithCGImage:imageRef];
    
    CGImageRelease( imageRef );
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpaceRef);
    free(buffer2);
    
    return myImage;
}

//Shader operation
int CRenderer::addShader(NSString* shaderName, NSString* vertexShader, NSString* fragmentShader, NSArray* shaderVariables, bool useTexCoord, bool useColors)
{

    CShader* shader = new CShader(this);
    if(shader->loadShader(shaderName, vertexShader, fragmentShader, useTexCoord, useColors))
    {
        GLuint shader_program = shader->program;
        for (int i = 0; i < [shaderVariables count]; i++)
        {
            if([shaderVariables objectAtIndex:i] != nil)
            {
                NSString *shaderVariable = shaderVariables[i];
                if (shaderVariable != nil) {
                    const char *szVariable = [shaderVariable UTF8String];
                    if (szVariable != NULL) {
                        shader->uniforms[UNIFORM_VAR1 + i] = glGetUniformLocation(shader_program, szVariable);
                    }
                }
            }
        }
        if(shader->linkProgram(shader_program))
        {
            int newIndex = shaders.AddShader(shader);
            //NSLog(@"Shader:%@ with index:%d", shaderName, newIndex);
            return newIndex;
        }
    }
    //NSLog(@"Shader:%@ failed to load", shaderName);
    return -1;
}
int CRenderer::addShader(NSString* shaderName, NSArray* shaderVariables, bool useTexCoord, bool useColors)
{

    CShader* shader = new CShader(this);
    if(shader->loadShader(shaderName, useTexCoord, useColors))
    {
        GLuint shader_program = shader->program;
        for (int i = 0; i < [shaderVariables count]; i++)
        {
            if([shaderVariables objectAtIndex:i] != nil)
                shader->uniforms[UNIFORM_VAR1+i] = glGetUniformLocation(shader_program, [[shaderVariables objectAtIndex:i] UTF8String]);
        }
        if(shader->linkProgram(shader_program))
        {
            int newIndex = shaders.AddShader(shader);
            //NSLog(@"Shader:%@ with index:%d", shaderName, newIndex);
            return newIndex;
        }
    }
    //NSLog(@"Shader:%@ failed to load", shaderName);
    return -1;
}
void CRenderer::removeShader(int shaderIndex)
{
    if(shaderIndex < 0 || shaderIndex >= shaders.Size())
        return;
    
    CShader* shader = (CShader*)shaders.SearchShaderByIndex(shaderIndex);
    if(shader != NULL)
    {
        shader->detachShader();
        shaders.RemoveShaderByIndex(shaderIndex);
    }
    
}
void CRenderer::removeAllShaders()
{
    shaders.RemoveAllShaders();
    effectShader = NULL;
}
void CRenderer::setEffectShader(int shaderIndex)
{
    if(shaderIndex < 0 || shaderIndex >= shaders.Size())
    {
        iShader = -1;
        effectShader = NULL;
        return;
    }

    iShader = shaderIndex;
    effectShader = (CShader*)shaders.SearchShaderByIndex(shaderIndex);
    //NSLog(@"shader index: %d and vector %p", shaderIndex, effectShader);
}
void CRenderer::setEffectShaderByIndex(int shaderIndex)
{
    if(shaderIndex < 0 || shaderIndex >= shaders.Size())
    
    {
        iShader = -1;
        effectShader = NULL;
        return;
    }
    iShader = shaderIndex;
    effectShader = (CShader*)shaders.SearchShaderByIndex(shaderIndex);
    //NSLog(@"shader index: %d and vector %p", shaderIndex, effectShader);
}
void CRenderer::removeEffectShader()
{
    effectShader = NULL;
    iShader = -1;
}
void CRenderer::updateVariable1i(NSString* varName, int value)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;
    const GLchar *name = [varName UTF8String];
    shader->setVariable1i(name, value);

}
void CRenderer::updateVariable1i(int varIndex, int value)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;

    shader->setVariable1i(varIndex, value);

}
void CRenderer::updateVariable1f(NSString* varName, float value)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;

    const GLchar *name = [varName UTF8String];
    shader->setVariable1f(name, value);

}
void CRenderer::updateVariable1f(int varIndex, float value)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;

    shader->setVariable1f(varIndex, value);

}

void CRenderer::updateVariable2i(NSString* varName, int value0, int value1)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;

    const GLchar *name = [varName UTF8String];
    shader->setVariable2i(name, value0, value1);

}
void CRenderer::updateVariable2i(int varIndex, int value0, int value1)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;

    shader->setVariable2i(varIndex, value0, value1);

}
void CRenderer::updateVariable2f(NSString* varName, float value0, float value1)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;

    const GLchar *name = [varName UTF8String];
    shader->setVariable2f(name, value0, value1);

}
void CRenderer::updateVariable2f( int varIndex, float value0, float value1)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;

    shader->setVariable2f(varIndex, value0, value1);

}

void CRenderer::updateVariable3i(NSString* varName, int value0, int value1, int value2)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;

    const GLchar *name = [varName UTF8String];
    shader->setVariable3i(name, value0, value1, value2);

}
void CRenderer::updateVariable3i(int varIndex, int value0, int value1, int value2)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;

    shader->setVariable3i(varIndex, value0, value1, value2);

}
void CRenderer::updateVariable3f(NSString* varName, float value0, float value1, float value2)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;
    const GLchar *name = [varName UTF8String];
    shader->setVariable3f(name, value0, value1, value2);

}
void CRenderer::updateVariable3f( int varIndex, float value0, float value1, float value2)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;

    shader->setVariable3f(varIndex, value0, value1, value2);

}
void CRenderer::updateVariable4i(NSString* varName, int value0, int value1, int value2, int value3)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;
    const GLchar *name = [varName UTF8String];
    shader->setVariable4i(name, value0, value1, value2, value3);

}
void CRenderer::updateVariable4i(int varIndex, int value0, int value1, int value2, int value3)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;

    shader->setVariable4i(varIndex, value0, value1, value2, value3);
}
void CRenderer::updateVariable4f(NSString* varName, float value0, float value1, float value2, float value3)
{
    if(iShader < 0)
            return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;
    const GLchar *name = [varName UTF8String];
    shader->setVariable4f(name, value0, value1, value2, value3);

}
void CRenderer::updateVariable4f(int varIndex, float value0, float value1, float value2, float value3)
{
    if(iShader < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(iShader);
    if(shader == NULL)
        return;

    shader->setVariable4f(varIndex, value0, value1, value2, value3);
}
void CRenderer::copyRttToRtt(CRenderToTexture *src, CRenderToTexture *dest, int effect, int effectParam, int flipY) {

    CShader* holdEffect = currentShader;
    [dest bindFrameBuffer];
    [dest clearWithAlphaDontBind:0.0f];
    setInkBlendEffect(effect, effectParam, defaultShader);
    currentShader->setPreMultiply(false);
    currentShader->setTexture(src);
    currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(0,0), Vec2f(src->width,src->height), Vec2fZero));

    draw();
    if(flipY)
        flipProjection();
    draw();
    if(flipY)
        restoreProjection();
    [dest unbindFrameBuffer];
    currentShader = holdEffect;
}
void CRenderer::setBackgroundUse(int shaderIndex)
{
    if(shaderIndex < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(shaderIndex);
    if(shader == NULL)
        return;

    shader->setBackgroundUse();
}
void CRenderer::setPixelSizeUse(int shaderIndex)
{
    if(shaderIndex < 0)
        return;

    CShader* shader = (CShader*)shaders.SearchShaderByIndex(shaderIndex);
    if(shader == NULL)
        return;

    shader->setPixelSizeUse();
}
void CRenderer::setCurrentView(int x, int y, float sX, float sY)
{
    baseX = x;
    baseY = y;
    scaleX = sX;
    scaleY = sY;
}

UIImage* CRenderer::readFBO(int texScene) {

    GLuint framebuffer = 0;
    int x = 0, y = 0;
    int w = renderScene.width;
    int h = renderScene.height;
    GLint readFramebuffer;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    
    if(texScene == 1)
    {
        framebuffer = renderScene.frameRtt->framebuffer;
    }
    else if(texScene == 2)
    {
        framebuffer = renderScene.layerRtt->framebuffer;
    }
    else if(texScene == 3)
    {
        framebuffer = renderScene.back1Rtt->framebuffer;
    }
    else if(texScene == 0) {
        framebuffer = colorRenderbuffer;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    
    auto* pixelData = (GLubyte*)malloc(w * h * 4); // Allocate buffer for RGBA data
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixelData); // Read pixels from FBO
    // Create a CGImage from the pixel data
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context;
    context = CGBitmapContextCreate(pixelData, w, h, 8, w * 4, colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGImageRef cgImage = CGBitmapContextCreateImage(context);

    // Convert the CGImage to a UIImage
    UIImage *image = [[UIImage imageWithCGImage:cgImage] retain];

    // Release the allocated memory
    CGContextRelease(context);
    CGColorSpaceRelease(colorSpace);
    CGImageRelease(cgImage);
    free(pixelData);

    if(framebuffer != readFramebuffer)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
    // Return the image for viewing in the Xcode debugger
    return image;
}

UIImage* CRenderer::readFBO(RenderingState* currentState) {

    int x = 0, y = 0;
    int w = currentState->contentSize.x;
    int h = currentState->contentSize.y;
    GLint readFramebuffer;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, currentState->framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    
    auto* pixelData = (GLubyte*)malloc(w * h * 4); // Allocate buffer for RGBA data
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixelData); // Read pixels from FBO
    // Create a CGImage from the pixel data
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context;
    context = CGBitmapContextCreate(pixelData, w, h, 8, w * 4, colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);

    CGImageRef cgImage = CGBitmapContextCreateImage(context);

    // Convert the CGImage to a UIImage
    UIImage *image = [[UIImage imageWithCGImage:cgImage] retain];

    // Release the allocated memory
    CGContextRelease(context);
    CGColorSpaceRelease(colorSpace);
    CGImageRelease(cgImage);
    free(pixelData);

    if(currentState->framebuffer != readFramebuffer)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
    // Return the image for viewing in the Xcode debugger
    return image;
}

UIImage* CRenderer::readAreaToImage(int x, int y, int w, int h)
{
    float in[3] = {0.0f, 0.0f, 1.0f};
    in[0] = x;
    in[1] = y;
    float out[3] = {0.0f, 0.0f, 0.0f};
    Mat3fXVec3f((const float*) &currentRenderState.transform,in, out);
    int transfX = out[0];
    int transfY = out[1];

    auto* pixelData = (GLubyte*)malloc(w * h * 4); // Allocate buffer for RGBA data
    glReadPixels(transfX, transfY, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixelData); // Read pixels from actual FBO
    // Create a CGImage from the pixel data
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context;
    context = CGBitmapContextCreate(pixelData, w, h, 8, w * 4, colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGImageRef cgImage = CGBitmapContextCreateImage(context);

    // Convert the CGImage to a UIImage
    UIImage *image = [[UIImage imageWithCGImage:cgImage] retain];

    // Release the allocated memory
    CGContextRelease(context);
    CGColorSpaceRelease(colorSpace);
    CGImageRelease(cgImage);
    free(pixelData);
    return image;
}

UIImage* CRenderer::readTextureToImage(GLuint textureID, int w, int h)
{
    CShader* holdEffect = currentShader;
    GLint old_active;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
    glActiveTexture(GL_TEXTURE0);

    CRenderToTexture *rtt = getOrCreatePoolRenderTexture(w, h, NO, YES);
    //[[CRenderToTexture alloc] initWithWidth:w Height:h withRenderer:this andSwapMode:NO andSwapProjection:YES];
    [rtt bindFrameBuffer];
    [rtt clearWithAlphaDontBind:0];
    setInkBlendEffect(BOP_COPY, -1, defaultShader);
    Mat3f texMatrix = Mat3f::textureMatrix(0, 0, w, h, w, h);
    currentShader->setTextureID(textureID, texMatrix);
    currentShader->setObjectMatrix(Mat3f::objectMatrix(Vec2f(0,0), Vec2f(w,h), Vec2fZero));
    currentShader->bindVertexArray();
    draw();
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    auto* pixelData = (GLubyte*)malloc(w * h * 4); // Allocate buffer for RGBA data
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixelData); // Read pixels from actual FBO
    // Create a CGImage from the pixel data
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context;
    context = CGBitmapContextCreate(pixelData, w, h, 8, w * 4, colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGImageRef cgImage = CGBitmapContextCreateImage(context);

    // Convert the CGImage to a UIImage
    UIImage *image = [[UIImage imageWithCGImage:cgImage] retain];

    // Release the allocated memory
    CGContextRelease(context);
    CGColorSpaceRelease(colorSpace);
    CGImageRelease(cgImage);
    free(pixelData);
    [rtt unbindFrameBuffer];
    releasePoolRenderTexture(rtt);
    glActiveTexture(old_active);
    currentShader = holdEffect;
    return image;
}

GLuint CRenderer::getOrCreatePoolGLTexture(int width, int height, int mode) {

    for (auto & tex : glTexturePool) {
        if (tex.width == width && tex.height == height &&
            tex.mode == mode &&
            !tex.isUsed) {
            tex.isUsed = true; // Mark as used
            return tex.textureID;
        }
    }

    if (glTexturePool.size() < MAX_TEXPOOL) {
        GLuint newTextureID;
        glGenTextures(1, &newTextureID);
        glBindTexture(GL_TEXTURE_2D, newTextureID);
        
        auto texData = (unsigned int*)malloc(width*height*4);
        memset(texData, 255, width*height*4);
        
        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, mode, width, height, 0, mode, GL_UNSIGNED_BYTE, texData);
        free(texData);
        
        glTexturePool.push_back({newTextureID, width, height, mode, true});

        glBindTexture(GL_TEXTURE_2D, 0);

        return newTextureID;
    }

    for (auto & tex : glTexturePool) {
        if (!tex.isUsed) {
            glBindTexture(GL_TEXTURE_2D, tex.textureID);
            glDeleteTextures(1, &tex.textureID);
            glGenTextures(1, &tex.textureID);
            glBindTexture(GL_TEXTURE_2D, tex.textureID);
            
            int size = width*height*(mode == GL_RGBA ? 4 :3);
            auto texData = (unsigned int*)malloc(size);
            memset(texData, 255, size);
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glTexImage2D(GL_TEXTURE_2D, 0, mode, width, height, 0, mode, GL_UNSIGNED_BYTE, texData);
            free(texData);
            
            tex.width = width;
            tex.height = height;
            tex.mode = mode;
            tex.isUsed = true; // Mark as used

            glBindTexture(GL_TEXTURE_2D, 0);

            return tex.textureID;
        }
    }

    NSLog(@"GLTexture pool is full and no free texture is available.\n");
    return -1;
}

void CRenderer::releaseGLPoolTexture(GLuint textureID) {
    for (auto & tex : glTexturePool) {
        if (tex.textureID == textureID) {
            tex.isUsed = false; // Mark the texture as unused
            return;
        }
    }
}

void CRenderer::deleteGLPoolTexture(GLuint textureID) {
    int count = 0, index = -1;
    for (auto & tex : glTexturePool) {
        if (tex.textureID == textureID) {
            glBindTexture(GL_TEXTURE_2D, tex.textureID);
            glDeleteTextures(1, &tex.textureID);
            index = count;
            break;
        }
        count++;
    }
    if(index != -1)
        glTexturePool.erase(glTexturePool.begin()+index);
}

void CRenderer::clearGLPoolTexture() {
    for (auto & tex : glTexturePool) {
        if (!tex.isUsed) {
            glBindTexture(GL_TEXTURE_2D, tex.textureID);
            glDeleteTextures(1, &tex.textureID);
        }
    }
    glTexturePool.clear();
    glBindTexture(GL_TEXTURE_2D, 0);
}

CTexture* CRenderer::getOrCreatePoolCTexture(int width, int height, bool resampling) {

    for (auto & tex : cTexturePool) {
        if (tex.width == width && tex.height == height &&
            tex.resampling == resampling &&
            !tex.isUsed) {
            tex.isUsed = true; // Mark as used
            return tex.texture;
        }
    }

    if (cTexturePool.size() < MAX_TEXPOOL) {
        CTexture* newTexture = [[CTexture alloc] init];
        [newTexture createGLTextureWidth:width withHeight:height andResampling:resampling];
        cTexturePool.push_back({newTexture, newTexture->textureId, width, height, resampling, true});

        return newTexture;
    }

    for (auto & tex : cTexturePool) {
        if (!tex.isUsed) {
            [tex.texture release];
            CTexture* newTexture = [[CTexture alloc] init];
            [newTexture createGLTextureWidth:width withHeight:height andResampling:resampling];
            tex.textureID =  newTexture->textureId;
            tex.texture = newTexture;
            tex.width = width;
            tex.height = height;
            tex.resampling = resampling;
            tex.isUsed = true; // Mark as used

            return tex.texture;
        }
    }

    NSLog(@"CTexture pool is full and no free texture is available.\n");
    return  nullptr;
}

void CRenderer::releasePoolCTexture(CTexture* texture) {
    for (auto & tex : cTexturePool) {
        if (tex.texture == texture) {
            tex.isUsed = false; // Mark the texture as unused
            return;
        }
    }
}

void CRenderer::clearPoolCTexture() {
    for (auto & tex : cTexturePool) {
        if (!tex.isUsed) {
            [tex.texture release];
        }
    }
    cTexturePool.clear();
}

CRenderToTexture* CRenderer::getOrCreatePoolRenderTexture(int width, int height, bool swapCoord, bool swapProj, int openGLmode) {

    for (auto & fbo : renderTexturePool) {
        if (fbo.width == width && fbo.height == height &&
            fbo.swapCoord == swapCoord && fbo.swapProj == swapProj &&
            fbo.mode == openGLmode &&
            !fbo.isUsed) {
            fbo.isUsed = true; // Mark as used
            return fbo.rtt;
        }
    }

    if (renderTexturePool.size() < MAX_RTTPOOL) {
        CRenderToTexture* rtt =[[CRenderToTexture alloc] initWithWidth:width Height:height withRenderer:this andSwapMode:NO andSwapProjection:swapProj];
        renderTexturePool.push_back({rtt, width, height, openGLmode, swapCoord, swapProj, true});

        return rtt;
    }

    for (auto & fbo : renderTexturePool) {
        if (!fbo.isUsed) {
            [fbo.rtt release];
            CRenderToTexture* rtt =[[CRenderToTexture alloc] initWithWidth:width Height:height withRenderer:this andSwapMode:NO andSwapProjection:swapProj];

            fbo.rtt    = rtt;
            fbo.width  = width;
            fbo.height = height;
            fbo.swapCoord = swapCoord;
            fbo.swapProj  = swapProj;
            fbo.mode = openGLmode;
            fbo.isUsed = true; // Mark as used
            return fbo.rtt;
        }
    }

    NSLog(@"Render To Texture pool is full and no free rtt is available.\n");
    return nullptr;
}

void CRenderer::releasePoolRenderTexture(CRenderToTexture* rtt) {
    for (auto & fbo : renderTexturePool) {
        if (fbo.rtt == rtt) {
            fbo.isUsed = false; // Mark the texture as unused
            return;
        }
    }
}

void CRenderer::deletePoolRenderTexture(CRenderToTexture* rtt) {
    int count = 0;
    for (auto & fbo : renderTexturePool) {
        if (fbo.rtt == rtt) {
            [fbo.rtt release];
            renderTexturePool.erase(renderTexturePool.begin()+count);
            return;
        }
        count++;
    }
}

void CRenderer::clearPoolRenderTexture() {
    for (auto & fbo : renderTexturePool) {
        if (!fbo.isUsed) {
           [fbo.rtt release];
        }
    }
    renderTexturePool.clear();
}

int CRenderer::getOrCreatePoolSubScene(int w, int h) {
    int count = 0;
    for (auto & scene : subScenePool) {
        if (scene.w == w && scene.h == h &&
            !scene.isUsed) {
            scene.isUsed = true; // Mark as used
            return (scene.index = count);
        }
        count++;
    }

    if (subScenePool.size() < MAX_SSCPOOL) {
        CRenderToTexture* frameRtt = [[CRenderToTexture alloc] initWithWidth:w Height:h withRenderer:this andSwapMode:NO andSwapProjection:YES];
        CRenderToTexture* layerRtt = [[CRenderToTexture alloc] initWithWidth:w Height:h withRenderer:this andSwapMode:NO andSwapProjection:YES];
        CRenderToTexture* back1Rtt = [[CRenderToTexture alloc] initWithWidth:w Height:h withRenderer:this andSwapMode:NO andSwapProjection:YES];
        subScenePool.push_back({count, w, h, frameRtt, layerRtt, back1Rtt, true});

        return count;
    }
    count = 0;
    for (auto & scene : subScenePool) {
        if (!scene.isUsed) {
            [scene.frameRtt release];
            [scene.layerRtt release];
            [scene.back1Rtt release];

            scene.frameRtt = [[CRenderToTexture alloc] initWithWidth:w Height:h withRenderer:this andSwapMode:NO andSwapProjection:YES];
            scene.layerRtt = [[CRenderToTexture alloc] initWithWidth:w Height:h withRenderer:this andSwapMode:NO andSwapProjection:YES];
            scene.back1Rtt = [[CRenderToTexture alloc] initWithWidth:w Height:h withRenderer:this andSwapMode:NO andSwapProjection:YES];

            scene.w = w;
            scene.h = h;
            scene.index = count;
            scene.isUsed = true; // Mark as used
            return scene.index;
        }
        count++;
    }

    NSLog(@"SubScene pool is full and no free slot is available.\n");
    return -1;
}

void CRenderer::releasePoolSubScene(int idx) {
    for (auto & scene : subScenePool) {
        if (scene.index == idx) {
            scene.isUsed = false; // Mark the texture as unused
            return;
        }
    }
}

void CRenderer::clearPoolSubScene() {
    for (auto & scene : subScenePool) {
        if (!scene.isUsed) {
            [scene.frameRtt release];
            [scene.layerRtt release];
            [scene.back1Rtt release];
        }
    }
    subScenePool.clear();
}

// Fast utility to move X/Y according the transformation matrix.
void CRenderer::Mat3fXVec3f(const float mat[9], const float vec[3], float result[3])
{
    result[0] = mat[0] * vec[0] + mat[3] * vec[1] + mat[6] * vec[2];
    result[1] = mat[1] * vec[0] + mat[4] * vec[1] + mat[7] * vec[2];
    result[2] = mat[2] * vec[0] + mat[5] * vec[1] + mat[8] * vec[2];
}
