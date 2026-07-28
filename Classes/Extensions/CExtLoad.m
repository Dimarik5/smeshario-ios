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
// CEXTLOAD Chargement des extensions
//
//----------------------------------------------------------------------------------
#import "CExtLoad.h"
#import "CFile.h"
#import "CRunExtension.h"

//F01
#import "CRunkcfile.h"
#import "CRunkcini.h"
//F01END

@implementation CExtLoad

-(id)initWithHandle:(short)_handle andName:(NSString*)_name andSubtype:(NSString*)_subType
{
	handle = _handle;
	name = _name;
	subType = _subType;
	return self;
}

-(void)dealloc
{
	[name release];
	[subType release];
	[super dealloc];
}
-(void)loadInfo:(CFile*)file
{
	NSUInteger debut = [file getFilePointer];
	
	short size = abs([file readAShort]);
	handle = [file readAShort];
	[file skipBytes:12];
	
	name = [file readAString];
	NSRange index = [name rangeOfString:@"."];
	if (index.location!=NSNotFound)
	{
		NSString* temp=[name substringToIndex:index.location];
		[name release];
		name=temp;
		[name retain];
	}
	subType = [file readAString];
	
	[file seek:debut + size];
}

-(CRunExtension*)loadRunObject 
{
	CRunExtension* object=nil;
	
//F02 			
	
if ([name caseInsensitiveCompare:@"kcfile"]==0)
{
object=[[CRunkcfile alloc] init];
}

if ([name caseInsensitiveCompare:@"kcini"]==0)
{
object=[[CRunkcini alloc] init];
}
//F02END
	
	return object;
}

@end
