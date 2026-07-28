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
// CText : Objet string
//
//----------------------------------------------------------------------------------
#import "CText.h"
#import "CRun.h"
#import "CRSpr.h"
#import "CObjectCommon.h"
#import "CMask.h"
#import "CSprite.h"
#import "CImageBank.h"
#import "CCreateObjectInfo.h"
#import "CValue.h"
#import "CDefTexts.h"
#import "CRunApp.h"
#import "CFontBank.h"
#import "CRect.h"
#import "CFontInfo.h"
#import "CRCom.h"
#import "CDefText.h"
#import "CServices.h"
#import "CBitmap.h"
#import "CRenderer.h"
#import "CTextSurface.h"

@implementation CText

-(void)dealloc
{
    if (rsTextBuffer!=nil)
    {
        [rsTextBuffer release];
    }
    if (rsCachedText!=nil)
    {
        [rsCachedText release];
    }
	[textSurface release];
	[super dealloc];
}

-(void)initObject:(CObjectCommon*)ocPtr withCOB:(CCreateObjectInfo*)cob
{
	rsFlag = 0;										// ???? adlo->loFlags;
	CDefTexts* txt = (CDefTexts*) ocPtr->ocObject;
	hoImgWidth = txt->otCx;
	hoImgHeight = txt->otCy;
	rsBoxCx = txt->otCx;
	rsBoxCy = txt->otCy;
    textHeight = rsBoxCy;

	// Recuperer la couleur et le nombre de phrases
	rsMaxi = txt->otNumberOfText;
	rsTextColor=0;
	if (txt->otNumberOfText>0)
	{
		rsTextColor = txt->otTexts[0]->tsColor;
	}
	rsHidden = (unsigned char) cob->cobFlags;					// A Toujours?
	rsTextBuffer = @"";
    rsCachedText = @"";
	rsFont = -1;
	rsMini = 0;
	//if ((rsHidden & COF_FIRSTTEXT) != 0)
	{
		if (txt->otNumberOfText>0)
		{
			rsTextBuffer = [[NSString alloc] initWithString:txt->otTexts[0]->tsText];
		}
		else
		{
			rsTextBuffer=[[NSString alloc] init];
		}
	}
    
    bUpdate = YES;
	textSurface = [[CTextSurface alloc] initWidthWidth:hoImgWidth andHeight:hoImgHeight];
}

-(void)handle
{
	[ros handle];
	if (roc->rcChanged)
	{
		roc->rcChanged = NO;
		[self modif];
	}
}

-(void)modif
{
	[ros modifRoutine];
}

-(void)display
{
	[ros displayRoutine];
}

-(void)getZoneInfos
{
    // Get text
    CDefTexts* txt = (CDefTexts*) hoCommon->ocObject;
    if(txt->otNumberOfText == 0)
        return;
    NSString* s = nil;
    if (rsMini >= 0)
    {
        s = txt->otTexts[rsMini]->tsText;
    }
    else
    {
        s = rsTextBuffer;
    }
    if (s == nil)
    {
        s = @"";
    }
    
    // Is it identical to cached text?
    if ([s compare:rsCachedText] != 0)
    {
        // Different => update text height
        if (rsCachedText!=nil)
        {
            [rsCachedText release];
        }
        rsCachedText = [[NSString alloc] initWithString:s];
        bUpdate = YES;
    }

    if(!bUpdate)
        return;
    
    // Get font
    short nFont = rsFont;
    if (nFont == -1)
    {
        if (txt->otNumberOfText>0)
        {
            nFont = txt->otTexts[0]->tsFont;
        }
    }
    CFont* font = [hoAdRunHeader->rhApp->fontBank getFontFromHandle:nFont];
        
    // Alignment flags
    short flags = txt->otTexts[0]->tsFlags;
    short dtflags = (short) (flags & (DT_LEFT | DT_CENTER | DT_RIGHT | DT_TOP | DT_BOTTOM | DT_VCENTER | DT_SINGLELINE));

    // Adjust rectangle
    CRect rc;
    rc.left = hoX;
    rc.top = hoY;
    rc.right = rc.left + rsBoxCx;
    rc.bottom = rc.top + rsBoxCy;

    hoImgWidth = (short) rc.width();
    hoImgHeight = (short) rc.height();
    hoImgXSpot = hoImgYSpot = 0;

    // Get exact size
    textHeight = [textSurface calculateTextSizeWithString:s andFlags:(short)(dtflags|DT_CALCRECT) andRect:rc andFont:font].height;
    if (textHeight != 0)
    {
        if (hoImgHeight < textHeight)
        {
            hoImgHeight = (short)MIN(textHeight,32767);
        }
        if (hoImgHeight != textSurface->height)
        {
            [textSurface setSizeWithWidth:hoImgWidth andHeight:hoImgHeight];
        }
    }
    bUpdate = NO;
}

-(void)draw:(CRenderer*)renderer
{
	CDefTexts* txt = (CDefTexts*) hoCommon->ocObject;
	if(txt->otNumberOfText == 0)
		return;	
	short flags = txt->otTexts[0]->tsFlags;
	
	int effect = ros->rsEffect;
	int effectParam = ros->rsEffectParam;
    int effectShader = ros->rsEffectShader;
	
	// Get font
	short nFont = rsFont;
	if (nFont == -1)
	{
		if (txt->otNumberOfText>0)
		{
			nFont = txt->otTexts[0]->tsFont;
		}
	}
	CFont* font = [hoAdRunHeader->rhApp->fontBank getFontFromHandle:nFont];
	
	// Affichage
	NSString* s = nil;
	if (rsMini >= 0)
	{
		s = txt->otTexts[rsMini]->tsText;
	}
	else
	{
		s = rsTextBuffer;
		if (s == nil)
		{
			s = @"";
		}
	}
	
	// Allow only the following flags
	short dtflags = (short) (flags & (DT_LEFT | DT_CENTER | DT_RIGHT | DT_TOP | DT_BOTTOM | DT_SINGLELINE));
    [textSurface setAntialiased:(hoAdRunHeader->rhApp->hdr2Options & AH2OPT_ANTIALIASED) != 0];
	[textSurface setText:s withFlags:dtflags andColor:BGRtoARGB(rsTextColor) andFont:font];
    int y = hoY;
    if (flags & DT_VCENTER)
        y += rsBoxCy/2 - textHeight/2;
    [textSurface draw:renderer withX:hoX andY:y andShader:effectShader andEffect:effect andEffectParam:effectParam];
}

-(CMask*)getCollisionMask:(int)flags
{
	return nil;
}

-(CFontInfo*)getFont
{
	short nFont = rsFont;
	if (nFont == -1)
	{
		CDefTexts* txt = (CDefTexts*) hoCommon->ocObject;
		if(txt->otNumberOfText == 0)
			return nil;
		nFont = txt->otTexts[0]->tsFont;
	}
	return [hoAdRunHeader->rhApp->fontBank getFontInfoFromHandle:nFont];
}

-(void)setFont:(CFontInfo*)info withRect:(CRect)pRc
{
    BOOL bResize = NO;
	rsFont = [hoAdRunHeader->rhApp->fontBank addFont:info];
	if (!pRc.isNil())
	{
        if(pRc.width() > hoImgWidth || pRc.height() > hoImgHeight)
            bResize = YES;
		hoImgWidth = rsBoxCx = (int)pRc.width();
		hoImgHeight = rsBoxCy = (int)pRc.height();
	}
    bUpdate = YES;
    if(bResize)
        [textSurface setSizeWithWidth:hoImgWidth andHeight:hoImgHeight];
	[self modif];
    roc->rcChanged = YES;
}

-(int)getFontColor
{
	return rsTextColor;
}

-(void)setFontColor:(int)rgb
{
	rsTextColor = rgb;
	[self modif];
	roc->rcChanged = YES;
}

-(BOOL)txtChange:(int)num
{
	if (num < -1)
	{
		num = -1;							// -1==chaine stockee...
	}
	if (num >= rsMaxi)
	{
		num = rsMaxi - 1;
	}
	if (num == rsMini)
	{
		return NO;
	}
	
	rsMini = num;
	
	// -------------------------------
	// Recopie le texte dans la chaine
	// -------------------------------
	if (num >= 0)
	{
		CDefTexts* txt = (CDefTexts*) hoCommon->ocObject;
		[self txtSetString:txt->otTexts[rsMini]->tsText];
	}
	
	// Reafficher ou pas?
	// ------------------
	if ((ros->rsFlags & RSFLAG_HIDDEN) != 0)
	{
		return NO;
	}
	return YES;
}

-(void)txtSetString:(NSString*)s
{
	if (rsTextBuffer!=nil)
	{
		[rsTextBuffer release];
	}
	rsTextBuffer = [[NSString alloc] initWithString:s];
}

// IDrawable
-(void)spriteDraw:(CRenderer*)renderer withSprite:(CSprite*)spr andImageBank:(CImageBank*)bank andX:(int)x andY:(int)y
{
	[self draw:renderer];
}

-(void)spriteKill:(CSprite*)spr
{
	[spr->sprExtraInfo release];
}
-(CMask*)spriteGetMask
{
	return nil;
}


@end
