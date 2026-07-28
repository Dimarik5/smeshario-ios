/* Copyright (c) 1996-2025 Clickteam
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
//  FileAccessManager.h
//
//  Created by Fernando Vivolo on 5/6/25.
//  Copyright © 2025 Clickteam. All rights reserved.
//
@interface FileAccessManager : NSObject

+ (void)saveBookmarkForURL:(NSURL *)url;
+ (void)saveBookmarkForURL:(NSURL *)url withKey:(NSString *)key;

+ (NSURL *)resolveBookmarkedURLForKey:(NSString *)key;
+ (NSURL *)resolveBookmarkedURLWithAccessForKey:(NSString *)key;
+ (void)stopAccessingResolvedURL;

@end
