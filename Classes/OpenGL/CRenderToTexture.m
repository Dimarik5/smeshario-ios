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
//  CRenderToTexture.m
//  RuntimeIPhone
//
//  Created by Anders Riggelsen on 8/10/10.
//  Copyright 2010 Clickteam. All rights reserved.
//

#import "CRenderToTexture.h"
#import "CRunApp.h"
#import "CServices.h"
#import "CRenderer.h"
#import "CRunView.h"

@implementation CRenderToTexture

- (id)initWithWidth:(int)w andHeight:(int)h andRunApp:(CRunApp*)runApp andSwapMode:(bool)bSwap andPOTMode:(bool)potMode andDepth:(int)bDepth
{
    handle = -1;
    
    app = runApp;
    renderer = app->renderer;
    
    width = w;
    height = h;
    
    format = RGBA8888;
    
//#ifdef POT_TEX
    if(potMode)
    {
        int nW = 16;
        int nH = 16;
        
        while(nW < w)
            nW *= 2;
        
        while(nH < h)
            nH *= 2;
        
        textureWidth = nW;
        textureHeight = nH;
    }
//#else
    else
    {
        textureWidth = w;
        textureHeight = h;
    }
//#endif
    originalWidth = width;
    originalHeight = height;

    wrapS = wrapT = GL_CLAMP_TO_EDGE;
    
    textureId = [self newEmptyTextureWithWidth:textureWidth andHeight:textureHeight];
    useDepth = bDepth;
    [self createFBO];
    coordsAreSwapped = bSwap;
    [self updateTextureMatrix];
    return self;
}

- (id)initWithWidth:(int)w andHeight:(int)h andRenderer:(CRenderer*)render andSwapMode:(bool)bSwap andPOTMode:(bool)potMode andDepth:(int)bDepth
{
    handle = -1;
    
    app = nil;
    renderer = render;
    
    width = w;
    height = h;
    
    format = RGBA8888;
//#ifdef POT_TEX
    if(potMode)
    {
        int nW = 16;
        int nH = 16;
        
        while(nW < w)
            nW *= 2;
        
        while(nH < h)
            nH *= 2;
        
        textureWidth = nW;
        textureHeight = nH;
    }
//#else
    else
    {
        textureWidth = w;
        textureHeight = h;
    }
//#endif
    originalWidth = width;
    originalHeight = height;

    wrapS = wrapT = GL_CLAMP_TO_EDGE;
    
    textureId = [self newEmptyTextureWithWidth:textureWidth andHeight:textureHeight];
    useDepth = bDepth;
    [self createFBO];
    coordsAreSwapped = bSwap;
    [self updateTextureMatrix];
    return self;
}

-(void)dealloc
{
    glDeleteTextures(1, &textureId);
    textureId = -1;
    if(useDepth)
        glDeleteRenderbuffers(1, &stencilbuffer);
    if(useStencil)
    {
        
    }
    if(glIsFramebuffer(framebuffer))
        glDeleteFramebuffers(1, &framebuffer);
    framebuffer = 0;
    [super dealloc];
}

- (id)initWithWidth:(int)w andHeight:(int)h andRunApp:(CRunApp*)runApp
{
   return [self initWithWidth:w andHeight:h andRunApp:runApp andSwapMode:YES andPOTMode:NO andDepth:NO];
}

- (id)initWithWidth:(int)w andHeight:(int)h andRunApp:(CRunApp*)runApp andSwapMode:(bool)bSwap
{
    return [self initWithWidth:w andHeight:h andRunApp:runApp andSwapMode:bSwap andPOTMode:NO andDepth:NO];
}

- (id)initWithWidth:(int)w andHeight:(int)h andRunApp:(CRunApp*)runApp andPOTMode:(bool)potMode
{
    return [self initWithWidth:w andHeight:h andRunApp:runApp andSwapMode:YES andPOTMode:potMode andDepth:NO];
}

- (id)initWithWidth:(int)w andHeight:(int)h andRenderer:(CRenderer*)renderer
{
   return [self initWithWidth:w andHeight:h andRenderer:renderer andSwapMode:YES andPOTMode:NO andDepth:NO];
}

- (id)initWithWidth:(int)w andHeight:(int)h andRenderer:(CRenderer*)renderer andSwapMode:(bool)bSwap
{
    return [self initWithWidth:w andHeight:h andRenderer:renderer andSwapMode:bSwap andPOTMode:NO andDepth:NO];
}

- (id)initWithWidth:(int)w andHeight:(int)h andRenderer:(CRenderer*)renderer andPOTMode:(bool)potMode
{
    return [self initWithWidth:w andHeight:h andRenderer:renderer andSwapMode:YES andPOTMode:potMode andDepth:NO];
}

- (id)initWithWidth:(int)w andHeight:(int)h andRenderer:(CRenderer*)renderer andDepthMode:(bool)depthMode
{
    return [self initWithWidth:w andHeight:h andRenderer:renderer andSwapMode:YES andPOTMode:NO andDepth:depthMode];
}

- (id)initWithWidth:(int)w Height:(int)h withRenderer:(CRenderer *)renderer andSwapMode:(bool)bSwap andSwapProjection:(bool)bSwapProj
{
    self = [self initWithWidth:w andHeight:h andRenderer:renderer andSwapMode:bSwap andPOTMode:NO andDepth:NO];
    [self setFlipProjection:bSwapProj];
    return self;
}

- (id)initWithWidth:(int)w Height:(int)h withRenderer:(CRenderer *)renderer withSwapMode:(bool)bSwap withSwapProjection:(bool)bSwapProj andStencil:(bool)bStencil
{
    self = [self initWithWidth:w andHeight:h andRenderer:renderer andSwapMode:bSwap andPOTMode:NO andDepth:NO];
    [self setFlipProjection:bSwapProj];
    return self;
}

- (void)resizeToWidth:(int)w andHeight:(int)h
{
    if(w != width || h != height)
    {
        glDeleteFramebuffers(1, &framebuffer);
        framebuffer = 0;

        [self deleteTexture];
        textureId = [self newEmptyTextureWithWidth:w andHeight:h];
        [self setResampling:false];
        [self updateTextureMatrix];
        [self createFBO];
    }
}

- (GLuint)newEmptyTextureWithWidth:(int)w andHeight:(int)h
{
	void* data = calloc(w*h, sizeof(char)*4);

	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	free(data);
	return texId;
}

- (void)createFBO {
    //Generate the render-to-texture framebuffer
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);
    if(useDepth)
    {
        glGenRenderbuffers(1, &stencilbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, stencilbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, textureWidth, textureHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, stencilbuffer);
    }
    int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
        NSLog(@"Error: Could not create framebuffer!");
    
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->currentRenderState.framebuffer);

}

- (void)setFlipProjection:(BOOL)flipY
{
    flipProjection = flipY;
}
- (void)bindFrameBuffer
{
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	renderer->pushRenderingState();
	renderer->currentRenderState.framebuffer = framebuffer;
	renderer->currentRenderState.framebufferSize = Vec2i(textureWidth, textureHeight);
	renderer->currentRenderState.contentSize = Vec2f(width, height);
    renderer->currentRenderState.viewport = Viewport(0, 0, width, height);
    renderer->currentRenderState.actProjstate = flipProjection;
    renderer->setProjectionMatrix(0, 0, width, height, flipProjection);
	renderer->forgetCachedState();
}

-(void)bindFrameBufferWithOffset:(Vec2f)offset
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    renderer->pushRenderingState();
    renderer->currentRenderState.framebuffer = framebuffer;
    renderer->currentRenderState.framebufferSize = Vec2i(textureWidth, textureHeight);
    renderer->currentRenderState.contentSize = Vec2f(width, height);
    renderer->currentRenderState.viewport = Viewport(0, 0, width, height);
    renderer->currentRenderState.actProjstate = flipProjection;
    renderer->setProjectionMatrix(0, 0, width, height, flipProjection);
    renderer->setViewport(Viewport(0, 0, width, height));
    renderer->forgetCachedState();
}

- (void)unbindFrameBuffer
{
	renderer->popRenderingState();
}

- (void)fillWithColor:(int)color
{
	GLint prevbuff = renderer->currentRenderState.framebuffer;
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glClearColor(getR(color)/255.0f, getG(color)/255.0f, getB(color)/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, prevbuff);
}

- (void)copyAlphaFrom:(CRenderToTexture*)rtt
{
	GLint prevbuff = renderer->currentRenderState.framebuffer;
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
	renderer->renderBlitFull(rtt);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glBindFramebuffer(GL_FRAMEBUFFER, prevbuff);
}

- (void)clearColorChannelWithColor:(int)color
{
	GLint prevbuff = renderer->currentRenderState.framebuffer;
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
	glClearColor(getR(color)/255.0f, getG(color)/255.0f, getB(color)/255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	
	glBindFramebuffer(GL_FRAMEBUFFER, prevbuff);
}

- (void)fillWithColor:(int)color andAlpha:(unsigned char)alpha
{
	GLint prevbuff = renderer->currentRenderState.framebuffer;
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glClearColor(getR(color)/255.0f, getG(color)/255.0f, getB(color)/255.0f, alpha/255.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, prevbuff);
}

-(void) fillWithPixels:(unsigned int*)buffer withWidth:(int)w andHeight:(int)h
{
    width  = w;
    height = h;
    
    if(width > textureWidth || height > textureHeight || textureId == -1)
        return;
    
    unsigned int * texData = nil;
    int bytesPrPixel = 4;
    
    glBindTexture(GL_TEXTURE_2D, textureId);        //Start working with our new texture id
    
    if(textureWidth == width && textureHeight == height)
    {
        //Texture data can be directly transfered to the graphics card without copying
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    }
    else
    {
        
        int pixels = textureWidth*textureHeight;
        //Copy to intermediate texture is required
        // the followingchange to avoid analaize screaming for something it doesn't understand
        texData = (unsigned int*)calloc(pixels, sizeof(unsigned int)); //(sizeof(char)*bytesPrPixel));
        memset(texData, 0, pixels*bytesPrPixel);
        
        int lineWidth = width*bytesPrPixel;
        int bLineWidth = (lineWidth+3) & ~3;
        
        for(int y=0; y<height; ++y)
            memcpy((char*)texData + textureWidth*y*bytesPrPixel, (char*)buffer + bLineWidth*y, lineWidth);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
        free(texData);
        
    }
    [self updateTextureMatrix];
}

-(void) clearWithColorDontBind:(int)color
{
    glClearColor(getR(color)/255.0f, getG(color)/255.0f, getB(color)/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

//Clears the texture and sets the alpha to the specified value
-(void) clearWithAlpha:(float)alpha
{
	GLint prevbuff = renderer->currentRenderState.framebuffer;
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glClearColor(0,0,0,alpha);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, prevbuff);
}

//Clears the texture and sets the alpha to the specified value (doesn't bind the buffer first)
-(void) clearWithAlphaDontBind:(float)alpha
{
	glClearColor(0,0,0,alpha);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

//Sets the contents of the alpha channel without modifying the contents of the texture
-(void) clearAlphaChannel:(float)alpha
{
	GLint prevbuff = renderer->currentRenderState.framebuffer;
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glColorMask(false, false, false, true);
	glClearColor(0,0,0,alpha);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glColorMask(true, true, true, true);
	glBindFramebuffer(GL_FRAMEBUFFER, prevbuff);
} 

-(size_t)uploadTexture
{
	// No need to upload the texture as a rendertarget in itself is a graphics card only texture.
	isUploaded = YES;
	return 0;
}

-(int)deleteTexture
{
	// No deletion of the texture. Waits to the object is released.
	isUploaded = NO;
	return 0;
}

-(void)expectTilableImage
{
	//Render to texture's are not supported for tileable textures
}



//Mipmaps not supported by RenderToTextures as it could
//cause massive slowdowns for often updated textures
-(void)generateMipMaps
{
}

-(void)cleanMemory
{
	//No resources to clean (done in dealloc)
}

-(void)setResampling:(BOOL)_resample
{
	resample = _resample;
	[self updateFilter];
}

- (BOOL)isOkSizeWidth:(int)w andHeight:(int) h {
    if(w == width && h == height)
        return YES;
    return NO;
}

- (BOOL)isRGBA {
    return format == RGBA8888;
}

- (UIImage*) readFBO
{
    int x = 0, y = 0;
    int w = width;
    int h = height;
    GLuint actualFramebuffer = renderer->currentRenderState.framebuffer;
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
    UIImage *image = [UIImage imageWithCGImage:cgImage];

    // Release the allocated memory
    CGContextRelease(context);
    CGColorSpaceRelease(colorSpace);
    CGImageRelease(cgImage);
    free(pixelData);

    if(framebuffer !=  actualFramebuffer)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, actualFramebuffer);
    // Return the image for viewing in the Xcode debugger
    return image;
}
@end
