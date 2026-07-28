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
//  FileAccessManager.m
//
//  Created by Fernando Vivolo on 5/6/25.
//  Copyright © 2025 Clickteam. All rights reserved.
//
#import "FileAccessManager.h"

static NSURL *activeScopedURL = nil;

@implementation FileAccessManager

+ (void)saveBookmarkForURL:(NSURL *)url {
    if (!url) return;

    NSString *key = url.lastPathComponent;
    [self saveBookmarkForURL:url withKey:key];
}

+ (void)saveBookmarkForURL:(NSURL *)url withKey:(NSString *)key {
    if (!url || !key) return;

    NSError *error = nil;
    NSData *bookmark = [url bookmarkDataWithOptions:0  // iOS: no security scope
                      includingResourceValuesForKeys:nil
                                       relativeToURL:nil
                                               error:&error];

    if (bookmark && !error) {
        [[NSUserDefaults standardUserDefaults] setObject:bookmark forKey:key];
        [[NSUserDefaults standardUserDefaults] synchronize];
        NSLog(@"Bookmark saved for key: %@", key);
    } else {
        NSLog(@"Failed to create bookmark: %@", error);
    }
}

+ (NSURL *)resolveBookmarkedURLForKey:(NSString *)key {
    if (!key) return nil;

    NSData *bookmark = [[NSUserDefaults standardUserDefaults] objectForKey:key];
    if (!bookmark) {
        NSLog(@"No bookmark found for key: %@", key);
        return nil;
    }

    NSError *error = nil;
    BOOL stale = NO;
    NSURL *resolvedURL = [NSURL URLByResolvingBookmarkData:bookmark
                                                    options:0
                                              relativeToURL:nil
                                        bookmarkDataIsStale:&stale
                                                      error:&error];
    if (!resolvedURL || error) {
        NSLog(@"Failed to resolve bookmark for key %@: %@", key, error);
        return nil;
    }

    return resolvedURL;
}

+ (NSURL *)resolveBookmarkedURLWithAccessForKey:(NSString *)key {
    NSURL *url = [self resolveBookmarkedURLForKey:key];
    if (url && [url startAccessingSecurityScopedResource]) {
        activeScopedURL = url;
        return url;
    }
    return nil;
}

+ (void)stopAccessingResolvedURL {
    if (activeScopedURL) {
        [activeScopedURL stopAccessingSecurityScopedResource];
        activeScopedURL = nil;
    }
}

@end
