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
// CIMAGEBANK : Stockage des images
//
//----------------------------------------------------------------------------------
#import <Foundation/Foundation.h>
#import "CRunView.h"
#import "IEnum.h"
#import "CImage.h"

@class CRunApp;

@interface CImageBank : NSObject <IEnum>
{
	CRunApp* runApp;
    CImage** images;
    int nHandlesReel;
    int nHandlesTotal;
    int nImages;
    NSUInteger* offsetsToImages;
    unsigned short* handleToIndex;
    short* useCount;
    short* useCountCopy;
    BOOL ispremultiply;
}

-(id)initWithApp:(CRunApp*)app;
-(void)dealloc;
-(void)preLoad;
-(void)cleanMemory;
-(short)enumerate:(short)num;
-(CImage*)getImageFromHandle:(unsigned short)handle;
-(CImage*)getImageFromIndex:(unsigned short)index;
-(void)resetToLoad;
-(void)setToLoad:(unsigned short)handle;
-(void)copyUseCount;
-(void)load;
-(void)delImage:(unsigned short)handle;
-(void)unloadLoaded;
-(void)loadUnloaded;
-(unsigned short)addImageCompare:(CImage*)img withXSpot:(short)xSpot andYSpot:(short)ySpot andXAP:(short)xAP andYAP:(short)yAP;
-(unsigned short)addImage:(CImage*)img withXSpot:(short)xSpot andYSpot:(short)ySpot andXAP:(short)xAP andYAP:(short)yAP andCount:(short)count;
-(void)loadImageList:(unsigned short*)handles withLength:(int)length;
-(void)loadImageByHandle:(unsigned short)handle;
-(void)removeImageByHandle:(unsigned short)handle;
-(ImageInfo)getImageInfoEx:(unsigned short)nImage withAngle:(float)nAngle andScaleX:(float)fScaleX andScaleY:(float)fScaleY;

@end
