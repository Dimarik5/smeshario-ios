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
//  CShader.m
//  RuntimeIPhone
//
//  Created by Anders Riggelsen on 6/10/10.
//  Copyright 2010 Clickteam. All rights reserved.
//

#import "CShader.h"
#import "CBitmap.h"
#import "CRenderer.h"
#import "CRenderToTexture.h"

CShader::CShader(CRenderer* renderer)
{
	render = renderer;
	currentEffect = -1;
	currentR = currentG = currentB = currentA = -1;

	for (int i=0; i<NUM_UNIFORMS; ++i) {
		uniforms[i] = -1;
	}
	forgetCachedState();
    
    for (int i=0; i<NUM_XTRATEX; ++i) {
        extraTexID[i] = -1;
        extraLocNb[i] = -1;
    }
    hasExtras = false;
    hasPixelSize = false;
    hasBackground = false;
    bckgTexID = -1;
    bckgRtt = nil;

}
CShader::~CShader()
{
    deleteBackground();
    
    if(bckgRtt)
    {
        [bckgRtt release];
        bckgRtt = nil;
    }
    
    if(program)
        glDeleteProgram(program);
    
    if(sname)
    {
        [sname release];
        sname = nil;
    }
}

void CShader::checkError()
{
    GLenum err = glGetError();
    if (GL_NO_ERROR != err)
        NSLog(@"Shader, got OpenGL Error: %i", err);

}

bool CShader::loadShader(NSString* name, NSString* vertexShader, NSString* fragmentShader, bool useTexCoord, bool useColors)
{
    sname = [[NSString alloc] initWithString:name];

	program = glCreateProgram();
	usesTexCoord = useTexCoord;
	usesColor = useColors;

	// Create and compile vertex shader
	if (!compileShader(&vertexProgram, vertexShader, GL_VERTEX_SHADER))
	{
		NSLog(@"Failed to compile vertex shader");
		return FALSE;
	}
	// Create and compile fragment shader
	if (!compileShader(&fragmentProgram, fragmentShader, GL_FRAGMENT_SHADER))
	{
		NSLog(@"Failed to compile fragment shader");
		return FALSE;
	}

	glAttachShader(program, vertexProgram);
	glAttachShader(program, fragmentProgram);

	glBindAttribLocation(program, ATTRIB_VERTEX, "position");
    
	if (!linkProgram(program))
	{
		NSLog(@"Failed to link program: %d", program);

		if (vertexProgram)
		{
			glDeleteShader(vertexProgram);
			vertexProgram = 0;
		}
		if (fragmentProgram)
		{
			glDeleteShader(fragmentProgram);
			fragmentProgram = 0;
		}
		if (program)
		{
			glDeleteProgram(program);
			program = 0;
		}
		return FALSE;
	}
    
	glUseProgram(program);
    
	uniforms[UNIFORM_PROJECTIONMATRIX] = glGetUniformLocation(program, "projectionMatrix");
	uniforms[UNIFORM_INKEFFECT] = glGetUniformLocation(program, "inkEffect");
	uniforms[UNIFORM_RGBA] = glGetUniformLocation(program, "blendColor");
	uniforms[UNIFORM_TRANSFORMMATRIX] = glGetUniformLocation(program, "transformMatrix");
	uniforms[UNIFORM_OBJECTMATRIX] = glGetUniformLocation(program, "objectMatrix");

	if(useTexCoord)
	{
		uniforms[UNIFORM_TEXTURE] = glGetUniformLocation(program, "imgTexture");
        uniforms[UNIFORM_BCKGTEXTURE] = glGetUniformLocation(program, "bckgTexture");
		uniforms[UNIFORM_TEXTUREMATRIX] = glGetUniformLocation(program, "textureMatrix");
        uniforms[UNIFORM_PREMULTIPLY] = glGetUniformLocation(program, "premult");
		glUniform1i(uniforms[UNIFORM_TEXTURE], 0);
		glActiveTexture(GL_TEXTURE0);
	}

	if(useColors)
	{
		uniforms[UNIFORM_GRADIENT] = glGetUniformLocation(program, "colorMatrix");
	}

    if ([name isEqualToString:@"gradientEllipse"] || [name isEqualToString:@"defaultEllipse"])
    {
        uniforms[UNIFORM_ELLIPSE_CENTERPOS] = glGetUniformLocation(program, "centerpos");
        uniforms[UNIFORM_ELLIPSE_RADIUS] = glGetUniformLocation(program, "radius");
    }

    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(ATTRIB_VERTEX);

	return TRUE;
}

GLuint CShader::compileShader(GLuint* shader, NSString* shaderSource, GLint type)
{
	GLint status;
	const GLchar *source = [shaderSource UTF8String];

	*shader = glCreateShader(type);
	glShaderSource(*shader, 1, &source, NULL);
	glCompileShader(*shader);

	GLint logLength;
	glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logLength);
	if (logLength > 0)
	{
		GLchar *log = (GLchar *)malloc(logLength);
		glGetShaderInfoLog(*shader, logLength, &logLength, log);
        if(type == GL_VERTEX_SHADER)
            NSLog(@"Shader vertex %@ compile log:\n%s", sname, log);
        if(type == GL_FRAGMENT_SHADER)
            NSLog(@"Shader fragment %@ compile log:\n%s", sname, log);
		free(log);
	}

	glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE)
	{
		glDeleteShader(*shader);
		NSLog(@"Unable to compile shader");
		return FALSE;
	}

	return TRUE;
}


bool CShader::loadShader(NSString* shaderName, bool useTexCoord, bool useColors)
{
	NSString* vertShaderPathname = [[NSBundle mainBundle] pathForResource:shaderName ofType:@"vsh" inDirectory:@""];
	NSString* vertexShader = [NSString stringWithContentsOfFile:vertShaderPathname encoding:NSUTF8StringEncoding error:nil];
	NSString* fragShaderPathname = [[NSBundle mainBundle] pathForResource:shaderName ofType:@"fsh" inDirectory:@""];
	NSString* fragmentShader = [NSString stringWithContentsOfFile:fragShaderPathname encoding:NSUTF8StringEncoding error:nil];
	return loadShader(shaderName, vertexShader, fragmentShader, useTexCoord, useColors);
}

bool CShader::linkProgram(GLuint prog)
{
	GLint status;

    glLinkProgram(prog);

    GLint logLength;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetProgramInfoLog(prog, logLength, &logLength, log);
        NSLog(@"Program link log:\n%s", log);
        free(log);
    }

    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        checkError();
        return FALSE;
    }

    return TRUE;
}

bool CShader::validateProgram(GLuint prog)
{

	 GLint logLength, status;

    glValidateProgram(prog);
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar *log = (GLchar *)malloc(logLength);
        glGetProgramInfoLog(prog, logLength, &logLength, log);
        NSLog(@"Program validate log:\n%s", log);
        free(log);
    }

    glGetProgramiv(prog, GL_VALIDATE_STATUS, &status);
    if (status == 0)
        return FALSE;

    return TRUE;
}

void CShader::detachShader()
{
    glDetachShader(program, vertexProgram);
    glDetachShader(program, fragmentProgram);

    if (vertexProgram)
    {
        glDeleteShader(vertexProgram);
        vertexProgram = 0;
    }
    if (fragmentProgram)
    {
        glDeleteShader(fragmentProgram);
        fragmentProgram = 0;
    }
    if (program)
    {
        glDeleteProgram(program);
        program = 0;
    }
    if(sname)
    {
        [sname release];
        sname = nil;
    }
}
void CShader::setRGBCoeff(float red, float green, float blue, float alpha)
{
	if(currentA != alpha || currentR != red || currentG != green || currentB != blue)
	{
		int uniformLoc = uniforms[UNIFORM_RGBA];
		glUniform4f(uniformLoc, red, green, blue, alpha);
		currentR = red;
		currentG = green;
		currentB = blue;
		currentA = alpha;
	}
}

void CShader::setPreMultiply(bool premult) {
    if(uniforms[UNIFORM_PREMULTIPLY] != -1
        && currentPremultiply != premult)
    {
        glUniform1i(uniforms[UNIFORM_PREMULTIPLY], premult);
        currentPremultiply = premult;
    }
}

void CShader::setInkEffect(int effect)
{
	if(currentEffect != effect)
	{
		glUniform1i(uniforms[UNIFORM_INKEFFECT], effect);
		currentEffect = effect;
	}
}

void CShader::setEllipseCenter (int x, int y, int rA, int rB)
{
    glUniform2f(uniforms[UNIFORM_ELLIPSE_CENTERPOS], (float)x, (float)y);
    glUniform2f(uniforms[UNIFORM_ELLIPSE_RADIUS], (float)(rA*rA), (float)(rB*rB));
}

void CShader::bindVertexArray()
{
    glBindVertexArray(render->vao);
}

void CShader::unbindVertexArray()
{
    glBindVertexArray(0);
}

void CShader::bindShader()
{
    if(render == NULL)
        return;
        
	if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram= program;
    }
    
    if(newProjection)
    {
        glUniformMatrix3fv(uniforms[UNIFORM_PROJECTIONMATRIX], 1, GL_FALSE, (float*)&render->currentRenderState.projection);
        newProjection = NO;
    }

    if(newTransform)
    {
        glUniformMatrix3fv(uniforms[UNIFORM_TRANSFORMMATRIX], 1, GL_FALSE, (float*)&render->currentRenderState.transform);
        newTransform = NO;
    }

}

void CShader::unbindShader()
{
    glUseProgram(0);
}

void CShader::forgetCachedState()
{
	prevTexCoord = Mat3f::zero();
	currentA = -1;
	currentR = -1;
	currentG = -1;
	currentB = -1;
	newProjection = YES;
	newTransform = YES;
}

void CShader::setTexCoord(Mat3f &texCoord)
{
	if(prevTexCoord != texCoord)
	{
		glUniformMatrix3fv(uniforms[UNIFORM_TEXTUREMATRIX], 1, GL_FALSE, (float*)&texCoord);
		prevTexCoord = texCoord;
	}
}

void CShader::setTexture(CTexture* texture)
{
    updateSurfaceTexture();
    
	int texId = texture->textureId;
	if(render->currentTextureID != texId)
	{
        glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texId);
		render->currentTextureID = texId;
	}
	if(prevTexCoord != texture->textureMatrix)
	{
		glUniformMatrix3fv(uniforms[UNIFORM_TEXTUREMATRIX], 1, GL_FALSE, (float*)&texture->textureMatrix);
		prevTexCoord = texture->textureMatrix;
	}
    
}

void CShader::setTexture(CTexture* texture, Mat3f &textureMatrix)
{
    updateSurfaceTexture();
    
	int texId = texture->textureId;
	if(render->currentTextureID != texId)
	{
        glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texId);
		render->currentTextureID = texId;
	}
	if(prevTexCoord != textureMatrix)
	{
		glUniformMatrix3fv(uniforms[UNIFORM_TEXTUREMATRIX], 1, GL_FALSE, (float*)&textureMatrix);
		prevTexCoord = textureMatrix;
	}

}

void CShader::setTextureID(GLuint textureId, Mat3f &textureMatrix)
{

    int texId = textureId;
    if(render->currentTextureID != texId)
    {
        glBindTexture(GL_TEXTURE_2D, texId);
        render->currentTextureID = texId;
    }
    if(prevTexCoord != textureMatrix)
    {
        glUniformMatrix3fv(uniforms[UNIFORM_TEXTUREMATRIX], 1, GL_FALSE, (float*)&textureMatrix);
        prevTexCoord = textureMatrix;
    }

}

void CShader::getBackground(int x, int y, int w, int h)
{
    if(!hasBackground || uniforms[UNIFORM_BCKGTEXTURE] == -1)
        return;

    int i = 0;
    if(hasExtras)
    {
        for(i=0; i < NUM_XTRATEX ; i++)
        {
            if(extraTexID[i] == -1)
                break;
        }
    }
    
    GLint old_active = -1;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
//
    GLint actualFramebuffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &actualFramebuffer);
   
    bool flipY = true;
    GLint toRead;
    GLsizei backheight;

    if(render->renderScene.onLayer && render->renderScene.needBck)
    {
        toRead = render->renderScene.back1Rtt->framebuffer;
        backheight = render->renderScene.height;
    }
    else
    {
        toRead = render->renderScene.frameRtt->framebuffer;
        backheight = render->renderScene.height;
    }
    
    glActiveTexture(GL_TEXTURE1+i);

    if(render->bOpenGL2)
    {
       if(toRead != actualFramebuffer)
       {
           glBindFramebuffer(GL_FRAMEBUFFER, toRead);
       }
       
       glActiveTexture(GL_TEXTURE1+i);

       if(bckgTexID != -1 && (w != bckgWidth || h != bckgHeight))
           deleteBackground();
       
       if(bckgTexID == -1)
       {
           glGenTextures(1, &bckgTexID);
           glBindTexture(GL_TEXTURE_2D, bckgTexID);
           
           bckgWidth = w;
           bckgHeight= h;
           int size = w*h*4; // yes is GL_RGBA
           auto texData = (unsigned int*)malloc(size);
           memset(texData, 0, size);
           
           glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
           glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
           glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
           glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

           glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
           free(texData);
       }
       if (bckgTexID != -1) {
           glBindTexture(GL_TEXTURE_2D, bckgTexID);

           // Set the read buffer to the correct attachment
           glReadBuffer(GL_COLOR_ATTACHMENT0);

           float in[3] = {0.0f, 0.0f, 1.0f};
           in[0] = x;
           in[1] = y;
           float out[3] = {0.0f, 0.0f, 0.0f};
           render->Mat3fXVec3f((const float*) &render->currentRenderState.transform,in, out);
           int transfX = out[0];
           int transfY = out[1];
           // Ensure x, y, w, h are within bounds
           if (transfX <= render->renderScene.width && transfY <= render->renderScene.height) {
               if (flipY) {
                   // Copy region directly, anyone of these take the same time to copy a texture.
                   glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, transfX, transfY, w, h);
                   //glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, transfX, transfY, w, h, 0);
               } else {
                   // Adjust Y to handle non-flipped coordinates
                   GLint adjustedY = render->renderScene.height - transfY - h;
                   if (adjustedY >= 0) { // Ensure adjustedY is valid
                       glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, transfX, adjustedY, w, h);
                       //glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, transfX, adjustedY, w, h, 0);
                   } else {
                       NSLog(@"Invalid region: adjustedY is out of bounds.\n");
                   }
               }
           } else {
               NSLog(@"Specified region exceeds FBO bounds.\n");
           }

           // Check for OpenGL errors
           //checkError();

           // Pass texture unit to shader
           glUniform1i(uniforms[UNIFORM_BCKGTEXTURE], i + 1);
       }

       else
           NSLog(@"bad ID texture");

       if(toRead != actualFramebuffer)
           glBindFramebuffer(GL_FRAMEBUFFER,actualFramebuffer);

    }
    else
    {
        if(!bckgRtt || bckgRtt->width != w || bckgRtt->height != h)
        {
            if(bckgRtt)
                [bckgRtt release];
            bckgRtt = [[CRenderToTexture alloc] initWithWidth:w andHeight:h andRenderer:render andSwapMode:flipY];
        }
        if(bckgRtt)
        {
            glBindTexture(GL_TEXTURE_2D, bckgRtt->textureId);
            float in[3] = {0.0f, 0.0f, 1.0f};
            in[0] = x;
            in[1] = y;
            float out[3] = {0.0f, 0.0f, 0.0f};
            render->Mat3fXVec3f((const float*) &render->currentRenderState.transform,in, out);
            int transfX = out[0];
            int transfY = out[1];
            
            glBindFramebuffer(GL_READ_FRAMEBUFFER, toRead);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bckgRtt->framebuffer);

            // Blit from source to destination
            glBlitFramebuffer(
                transfX, transfY, transfX + w, transfY + h,                 // Source rectangle
                0, 0, w, h,                 // Destination rectangle
                GL_COLOR_BUFFER_BIT,        // Mask indicating what buffer is to be copied
                GL_NEAREST                  // Interpolation method
            );
        }
        // Pass texture unit to shader
        glUniform1i(uniforms[UNIFORM_BCKGTEXTURE], i + 1);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        
        glBindFramebuffer(GL_FRAMEBUFFER, actualFramebuffer);
    }

    glActiveTexture(old_active);

}

void CShader::getBackground(CRenderToTexture* rtt, int x, int y, int w, int h)
{
    if(!hasBackground || uniforms[UNIFORM_BCKGTEXTURE] == -1)
        return;

    int i = 0;
    if(hasExtras)
    {
        for(i=0; i < NUM_XTRATEX ; i++)
        {
            if(extraTexID[i] == -1)
                break;
        }
    }
    
    GLint old_active = -1;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);

    GLint buffer = render->currentRenderState.framebuffer;

    glActiveTexture(GL_TEXTURE1+i);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, rtt->framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    if(bckgTexID != -1 && (w != bckgWidth || h != bckgHeight))
        deleteBackground();

    if(bckgTexID == -1)
    {
        glGenTextures(1, &bckgTexID);
        glBindTexture(GL_TEXTURE_2D, bckgTexID);
        
        bckgWidth = w;
        bckgHeight= h;
        int size = w*h*4; // yes is GL_RGBA
        auto texData = (unsigned int*)malloc(size);
        memset(texData, 0, size);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
        free(texData);
    }
    
    if(bckgTexID != -1)
    {
        glBindTexture(GL_TEXTURE_2D, bckgTexID);
        float in[3] = {0.0f, 0.0f, 1.0f};
        in[0] = x;
        in[1] = y;
        float out[3] = {0.0f, 0.0f, 0.0f};
        render->Mat3fXVec3f((const float*) &render->currentRenderState.transform,in, out);
        int transfX = out[0];
        int transfY = out[1];

        if(!rtt->flipProjection)
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, transfX, transfY, w, h);
        else
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, transfX, render->renderScene.height-transfY-h, w, h);
        
        glUniform1i(uniforms[UNIFORM_BCKGTEXTURE], i+1);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, buffer);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(old_active);
    glBindTexture(GL_TEXTURE_2D, 0);

}

void CShader::deleteBackground()
{
    if(!hasBackground || uniforms[UNIFORM_BCKGTEXTURE] == -1)
        return;

    if(bckgTexID != -1)
    {
        glBindTexture(GL_TEXTURE_2D, bckgTexID);
        glDeleteTextures (1, (GLuint *) &bckgTexID);
        bckgTexID = -1;
    }
}

void CShader::releaseBackground()
{
    // keep it until delete shader.
}

void CShader::setSurfaceTextureAtIndex(CTexture* texture, const GLchar* name, int index)
{
    if(index <= 0 || index > NUM_XTRATEX+1)
        return;
    
    glUseProgram(program);
    int loc = glGetUniformLocation(program, name);
    if(loc != -1)
    {
        glUniform1i(loc, index);
    }
    //NSLog(@"Setting in shader: %s and handle: %d texture: %d in index:%d in location:%d",name, texture->handle, texture->textureId, index, loc);
    extraTexID[index-1] = texture->textureId;
    extraLocNb[index-1] = loc;
    hasExtras |= TRUE;
    
}

void CShader::updateSurfaceTexture()
{
    if(!hasExtras)
        return;

    GLint actActive = -1;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &actActive);
    
    for(int i=0 ; i < NUM_XTRATEX; i++)
    {
        if(extraTexID[i] != -1)
        {
            glActiveTexture(GL_TEXTURE1 + i);
            glBindTexture(GL_TEXTURE_2D, extraTexID[i]);
        }
        if(extraLocNb[i] != -1)
            glUniform1i(extraLocNb[i], i+1);
    }

    glActiveTexture(actActive);

}

void CShader::setObjectMatrix(const Mat3f &matrix)
{
	glUniformMatrix3fv(uniforms[UNIFORM_OBJECTMATRIX], 1, GL_FALSE, (float*)&matrix);
}

void CShader::setGradientColors(GradientColor gradient)
{
    glUniformMatrix4fv(uniforms[UNIFORM_GRADIENT], 1, GL_FALSE, (float*)&gradient);
}

void CShader::setGradientColors(int color)
{
	GradientColor gradient = GradientColor(color);
	glUniformMatrix4fv(uniforms[UNIFORM_GRADIENT], 1, GL_FALSE, (float*)&gradient);
}

void CShader::setGradientColors(int a, int b, BOOL horizontal)
{
	GradientColor gradient = GradientColor(a, b, horizontal);
	glUniformMatrix4fv(uniforms[UNIFORM_GRADIENT], 1, GL_FALSE, (float*)&gradient);
}

void CShader::setGradientColors(int a, int b, int c, int d)
{
	GradientColor gradient = GradientColor(a, b, c, d);
	glUniformMatrix4fv(uniforms[UNIFORM_GRADIENT], 1, GL_FALSE, (float*)&gradient);
}

void CShader::setColors(float* colors)
{
    glVertexAttribPointer(ATTRIB_COLORS, 4, GL_FLOAT, GL_FALSE, 0, &colors);
    glEnableVertexAttribArray(ATTRIB_COLORS);
}

void CShader::setVariable1i(const GLchar* field, int value)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    GLint uniformId = glGetUniformLocation(program, field);
    if(uniformId != -1)
        glUniform1i(uniformId, value);
}
void CShader::setVariable1f(const GLchar* field, float value)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    GLint uniformId = glGetUniformLocation(program, field);
    if(uniformId != -1)
        glUniform1f(uniformId, value);
}
void CShader::setVariable2i(const GLchar* field, int value0, int value1)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    GLint uniformId = glGetUniformLocation(program, field);
    if(uniformId != -1)
        glUniform2i(uniformId, value0, value1);
}
void CShader::setVariable2f(const GLchar* field, float value0, float value1)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    GLint uniformId = glGetUniformLocation(program, field);
    if(uniformId != -1)
        glUniform2f(uniformId, value0, value1);
}
void CShader::setVariable3i(const GLchar* field, int value0, int value1, int value2)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    };
    GLint uniformId = glGetUniformLocation(program, field);
    if(uniformId != -1)
        glUniform3i(uniformId, value0, value1, value2);
}
void CShader::setVariable3f(const GLchar* field, float value0, float value1, float value2)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    GLint uniformId = glGetUniformLocation(program, field);
    if(uniformId != -1)
        glUniform3f(uniformId, value0, value1, value2);
}
void CShader::setVariable4i(const GLchar* field, int value0, int value1, int value2, int value3)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    GLint uniformId = glGetUniformLocation(program, field);
    if(uniformId != -1)
        glUniform4i(uniformId, value0, value1, value2, value3);
}
void CShader::setVariable4f(const GLchar* field, float value0, float value1, float value2, float value3)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    GLint uniformId = glGetUniformLocation(program, field);
    if(uniformId != -1)
        glUniform4f(uniformId, value0, value1, value2, value3);
}
void CShader::setVariable1i(int uniform_field, int value)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    if(uniform_field > -1)
        glUniform1i(uniform_field, value);
}
void CShader::setVariable1f(int uniform_field, float value)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    if(uniform_field > -1)
        glUniform1f(uniform_field, value);
}
void CShader::setVariable2i(int uniform_field, int value0, int value1)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    if(uniform_field > -1)
        glUniform2i(uniform_field, value0, value1);
}
void CShader::setVariable2f(int uniform_field, float value0, float value1)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    if(uniform_field > -1)
        glUniform2f(uniform_field, value0, value1);
}
void CShader::setVariable3i(int uniform_field, int value0, int value1, int value2)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    if(uniform_field > -1)
        glUniform3i(uniform_field, value0, value1, value2);
}
void CShader::setVariable3f(int uniform_field, float value0, float value1, float value2)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    if(uniform_field > -1)
        glUniform3f(uniform_field, value0, value1, value2);
}
void CShader::setVariable4i(int uniform_field, int value0, int value1, int value2, int value3)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    if(uniform_field > -1)
        glUniform4i(uniform_field, value0, value1, value2, value3);
}
void CShader::setVariable4f(int uniform_field, float value0, float value1, float value2, float value3)
{
    if(render->currentProgram != program)
    {
        glUseProgram(program);
        render->currentProgram = program;
    }
    if(uniform_field > -1)
        glUniform4f(uniform_field, value0, value1, value2, value3);
}
void CShader::setBackgroundUse()
{
    deleteBackground();
    hasBackground = true;
}

void CShader::setPixelSizeUse() 
{
    hasPixelSize = true;
}

void CShader::MatrixLog(const Mat3f &mat)
{
    //order according Coremath -> float a,b,c,    d,e,f,   g,h,i;
    NSMutableString *arrayString = [NSMutableString stringWithString:@"{"];
    [arrayString appendFormat:@"%f", mat.a];
    [arrayString appendString:@", "]; // Add a comma between elements
    [arrayString appendFormat:@"%f", mat.b];
    [arrayString appendString:@", "]; // Add a comma between elements
    [arrayString appendFormat:@"%f", mat.c];
    [arrayString appendString:@", "]; // Add a comma between elements
    [arrayString appendFormat:@"%f", mat.d];
    [arrayString appendString:@", "]; // Add a comma between elements
    [arrayString appendFormat:@"%f", mat.e];
    [arrayString appendString:@", "]; // Add a comma between elements
    [arrayString appendFormat:@"%f", mat.f];
    [arrayString appendString:@", "]; // Add a comma between elements
    [arrayString appendFormat:@"%f", mat.g];
    [arrayString appendString:@", "]; // Add a comma between elements
    [arrayString appendFormat:@"%f", mat.h];
    [arrayString appendString:@", "]; // Add a comma between elements
    [arrayString appendFormat:@"%f", mat.i];
    [arrayString appendString:@", "]; // Add a comma between elements
    [arrayString appendString:@"}"];
    NSLog(@"Matrix: %@", arrayString);
}

const char* CShader::getGLTypeName(GLenum type) {
    switch (type) {
        case GL_FLOAT: return "GL_FLOAT";
        case GL_FLOAT_VEC2: return "GL_FLOAT_VEC2";
        case GL_FLOAT_VEC3: return "GL_FLOAT_VEC3";
        case GL_FLOAT_VEC4: return "GL_FLOAT_VEC4";
        case GL_INT: return "GL_INT";
        case GL_BOOL: return "GL_BOOL";
        case GL_FLOAT_MAT3: return "GL_FLOAT_MAT3";
        case GL_FLOAT_MAT4: return "GL_FLOAT_MAT4";
        case GL_SAMPLER_2D: return "GL_SAMPLER_2D";
        // Add other cases as needed
        default: return "Unknown";
    }
}
