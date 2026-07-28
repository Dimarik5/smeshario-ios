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
//  CTexture.m
//  RuntimeIPhone
//
//  Created by Anders Riggelsen on 9/28/13.
//  Copyright (c) 2013 Clickteam. All rights reserved.
//

#import "CTexture.h"
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>

@implementation CTexture


-(size_t)uploadTexture
{
	return 0;
}

-(int)deleteTexture
{
	return 0;
}

-(void)generateMipMaps
{

}

-(void)cleanMemory
{

}

-(void)expectTilableImage
{

}

-(BOOL)imageIsPOT
{
    return [self powerOfTwo:width] && [self powerOfTwo:height];
}

-(void)setResampling:(BOOL)_resample
{
	if(resample != _resample)
	{
		resample = _resample;
		[self updateFilter];
	}
}

-(void)createGLTextureWidth:(int)width withHeight:(int)height andResampling:(int)resampling
{
    void* data = calloc(width*height, sizeof(char)*4);

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    free(data);
    resample = resampling;
    [self updateFilter];
    
}

-(void)updateFilter
{
	glBindTexture(GL_TEXTURE_2D, textureId);
	if(resample)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ((hasMipMaps) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ((hasMipMaps) ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST));
	}
}

-(void)updateFilterNoBind
{
    if(resample)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ((hasMipMaps) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ((hasMipMaps) ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST));
    }
}

-(void)updateTextureMode:(int)wrapMode
{
    if(wrapS != wrapMode || wrapT != wrapMode)
    {
        wrapS = wrapMode;
        wrapT = wrapMode;
        if((int)textureId != -1)
        {
            glBindTexture(GL_TEXTURE_2D, textureId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
        }
    }
}

-(void)updateTextureMatrix
{
	if(!coordsAreSwapped)
		textureMatrix = Mat3f::textureMatrix(0, 0, width, height, textureWidth, textureHeight);
	else
		textureMatrix = Mat3f::textureMatrixFlipped(0, 0, width, height, height, textureWidth, textureHeight);
}

-(bool)powerOfTwo:(int)n
{
    return n && (!(n & (n-1)));
}

-(UIImage*)getTextureImage
{
    return nil;
}
@end
