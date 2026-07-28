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
//----------------------------------------------------------------------------------
//
// COBJECTCOMMON : Donn�es d'un objet normal
//
//----------------------------------------------------------------------------------
#import "COCQBackdrop.h"
#import "CFile.h"
#import "IEnum.h"
#import "CImage.h"
#import "CImageBank.h"
#import "CServices.h"
#import "CBitmap.h"
#import "CSprite.h"
#import "CRenderer.h"
#import "CRenderToTexture.h"
#import "COI.h"

@implementation COCQBackdrop

-(id)init
{
	self=[super init];
	return self;
}
-(void)dealloc
{
	[super dealloc];
}
-(void)load:(CFile*)file withType:(short)type andCOI:(COI*)pOi
{
	pCOI=pOi;

	[file skipBytes:4];		// ocDWSize
	ocObstacleType = [file readAShort];
	ocColMode = [file readAShort];
	ocCx = [file readAInt];
	ocCy = [file readAInt];
	ocBorderSize = [file readAShort];
	ocBorderColor = [file readAColor];
	ocShape = [file readAShort];
	
	ocFillType = [file readAShort];
	if (ocShape == 1)		// SHAPE_LINE
	{
		ocLineFlags = [file readAShort];
	}
	else
	{
		switch (ocFillType)
		{
			case FILLTYPE_SOLID:
				ocColor1 = ocColor2 = [file readAColor];
				ocFillType = FILLTYPE_GRADIENT;	//Changes the solid-color to a gradient of the same color
				break;
			case FILLTYPE_GRADIENT:
                ocColor1 = [file readAColor];
				ocColor2 = [file readAColor];
				ocGradientFlags = [file readAInt];
				break;
			case FILLTYPE_MOTIF:
				ocImage = [file readAUnsignedShort];
				break;
		}
	}
}
-(void)enumElements:(id)enumImages withFont:(id)enumFonts
{
	if (ocFillType == 3)		    // FILLTYPE_IMAGE
	{
		if (enumImages != nil)
		{
			id<IEnum> pImages=enumImages;
            unsigned short num = [pImages enumerate:ocImage];
			if (num != (unsigned short)-1)
			{
				ocImage = num;
			}
		}
	}
}

-(void)spriteDraw:(CRenderer*)renderer withSprite:(CSprite*)spr andImageBank:(CImageBank*)bank andX:(int)x andY:(int)y
{

    switch (ocFillType)
    {
        case FILLTYPE_MOTIF:
        {
            CImage* image = [bank getImageFromHandle:ocImage];
            switch (ocShape) {
                case SHAPE_RECTANGLE:
                    
                    renderer->renderPattern(image, x, y, ocCx, ocCy, spr->sprEffectShader, pCOI->oiInkEffect, pCOI->oiInkEffectParam);
                    break;
                    
                case SHAPE_ELLIPSE:
                    //Log.d("MMFRuntime", "Effect in COCQB: "+pCOI.oiIndexEffect);
                    renderer->renderPatternEllipse(image, x, y, ocCx, ocCy, spr->sprEffectShader, pCOI->oiInkEffect, pCOI->oiInkEffectParam);
                    break;
            }
            break;
        }

        case FILLTYPE_GRADIENT:
        {
            GradientColor gradient = GradientColor(ocColor1, ocColor2, (ocGradientFlags == GRADIENT_HORIZONTAL));

            if(ocShape == SHAPE_RECTANGLE)
            {
                renderer->renderGradient(gradient, x, y, ocCx, ocCy, spr->sprEffectShader, pCOI->oiInkEffect, pCOI->oiInkEffectParam);
            }
            else if(ocShape == SHAPE_ELLIPSE)
            {
                renderer->renderGradientEllipse(gradient, x, y, ocCx, ocCy, spr->sprEffectShader, pCOI->oiInkEffect, pCOI->oiInkEffectParam);
            }
            break;
        }
    }
    if (ocBorderSize > 0)
    {
        int cx = ocCx;
        int cy = ocCy;

        switch (ocShape)
        {
        // SHAPE_LINE
        case 1:
            if ((ocLineFlags & LINEF_INVX) != 0)
            {
                x += cx;
                cx = -cx;
            }
            if ((ocLineFlags & LINEF_INVY) != 0)
            {
                y += cy;
                cy = -cy;
            }
            renderer->renderRect(x, y, x + cx, y + cy, ocBorderColor, ocBorderSize, pCOI->oiInkEffect, pCOI->oiInkEffectParam);
            break;
            // SHAPE_RECTANGLE
        case 2:
            renderer->renderRect(x, y, cx, cy, ocBorderColor, ocBorderSize, pCOI->oiInkEffect, pCOI->oiInkEffectParam);
            break;
            // SHAPE_ELLIPSE
        case 3:
            /* TODO */
            break;
        }
    }
}

-(void)spriteKill:(CSprite*)spr
{
}
-(CMask*)spriteGetMask
{
	return nil;
}

@end
