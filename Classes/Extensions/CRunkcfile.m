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
// CRunkcfile 03/28/2025
//
//----------------------------------------------------------------------------------
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "CRunkcfile.h"
#import "CFile.h"
#import "CRunApp.h"
#import "CBitmap.h"
#import "CCreateObjectInfo.h"
#import "CValue.h"
#import "CExtension.h"
#import "CRun.h"
#import "CCndExtension.h"
#import "FileAccessManager.h"

#define    CND_OK            0
#define    CND_EXISTS        1
#define    CND_ISREADABLE    2
#define    CND_ISWRITABLE    3
#define    CND_ISFILE        4
#define    CND_ISDIR         5
#define    CND_SELECTOR_OK   6
#define    CND_SELECTOR_CANCEL 7
#define    CND_LAST 8

#define    ACT_DIRSET        0
#define    ACT_DIRSETORG    1
#define    ACT_DIRCREATE    2
#define    ACT_DIRDELETE    3
#define    ACT_FILECREATE    4
#define    ACT_FILEDELETE    5
#define    ACT_FILERENAME    6
#define    ACT_FILEAPPEND    7
#define    ACT_FILECOPY    8
#define    ACT_FILEMOVE    9
#define    ACT_FILEWRITE    10
#define    ACT_CLEARERROR    11
#define    ACT_RUN            12
#define    ACT_SETFSTITLE    13
#define    ACT_SETFSFLAGS    14
#define    ACT_RESETFSFLAGS 15
#define    ACT_SETFSFILTER    16
#define    ACT_SETFSEXT    17
#define    ACT_OPENLOADFS    18
#define    ACT_OPENSAVEFS    19
#define    ACT_SETFSSINGLESEL   20
#define    ACT_SETFSMULTIPLESEL 21
#define    ACT_OPENDIRSELECTOR 22

#define    EXP_SIZE            0
#define    EXP_CREATEDATE        1
#define    EXP_MODIFDATE        2
#define    EXP_ACCESSDATE        3
#define    EXP_DRIVENAME        4
#define    EXP_DIRNAME            5
#define    EXP_FILENAME        6
#define    EXP_EXTNAME            7
#define    EXP_TOTALNAME        8
#define    EXP_CURRENTDIR        9
#define    EXP_FILEVERSIONMS    10
#define    EXP_FILEVERSIONLS    11
#define    EXP_LASTERROR        12
#define    EXP_TEMPFILE        13
#define    EXP_WINDIR            14
#define    EXP_GETFSRESULT        15
#define    EXP_CREATEPROMPT        16
#define    EXP_ALLOWBADFILE    17
#define    EXP_CHANGEDIR        18
#define    EXP_NONETWORKBUTTON    19
#define    EXP_NOOVERWRITEPROMPT    20
#define    EXP_ALLOWBADPATH    21
#define    EXP_GETFSDFILTER    22
#define    EXP_GETFSNUMBER     23
#define    EXP_GETFSRESULTAT   24
#define    EXP_DRIVEDIRFROMLABEL 25
#define    EXP_SYSDIR            26
#define    EXP_MYDOCDIR        27
#define    EXP_APPDATADIR        28
#define    EXP_USERDIR            29
#define    EXP_ALLUSERDIR        30
#define    EXP_ALLUSERDOCDIR    31
#define    EXP_ALLUSERAPPDATADIR 32

#define    _CREATEPROMPT    0x0001
#define    _ALLOWBADFILE   0x0002
#define    _CHANGEDIR       0x0004
#define    _NONETWORKBUTTON   0x0008
#define    _NOOVERWRITEPROMPT 0x0010
#define    _ALLOWBADPATH   0x0020

@implementation CRunkcfile

-(int)getNumberOfConditions
{
    return CND_LAST;
}

-(BOOL)createRunObject:(CFile*)file withCOB:(CCreateObjectInfo*)cob andVersion:(int)version
{
    multiSelection = NO;
    lastError = 0;
    okEventCount=-1;
    cancelEventCount=-1;
    initialCurrentDir = [[NSFileManager defaultManager] currentDirectoryPath];
    if ( initialCurrentDir != nil )
        [initialCurrentDir retain];
    fileSelectorTitle = nil;
    fileSelectorResult = nil;
    fileTypesArray = nil;
    allowOtherFileTypes = NO;

    return YES;
}

-(void)destroyRunObject:(BOOL)bFast
{
    if ( fileSelectorResult != nil )
    {
        [fileSelectorResult release];
        fileSelectorResult = nil;
    }
    [fileTypesArray release];
    if ( initialCurrentDir != nil )
        [initialCurrentDir release];
}

-(int)handleRunObject
{
    return 0;   // REFLAG_NONE;
}

- (UIViewController *)documentInteractionControllerViewControllerForPreview:(UIDocumentInteractionController *)controller {
    return nil;
}

- (void)documentInteractionControllerWillBeginPreview:(UIDocumentInteractionController *)controller {
    NSLog(@"Will begin previewing the document");
}

- (void)documentInteractionControllerDidEndPreview:(UIDocumentInteractionController *)controller {
    NSLog(@"Finished previewing the document");
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL *selectedFolderURL = [urls firstObject];
    if (selectedFolderURL) {
        //NSLog(@"Selected first path: %@", selectedFolderURL.path);
        if(fileSelectorResult != nil)
           [fileSelectorResult release];

        fileSelectorResult = [[NSArray alloc] initWithArray:urls];
        for (NSURL *url in urls) {
            if (url != nil) {
                // Save using the lastPathComponent as the key
                [url startAccessingSecurityScopedResource];
                [FileAccessManager saveBookmarkForURL:url];
                [url stopAccessingSecurityScopedResource];
            }
        }
        [ho generateEvent:CND_SELECTOR_OK withParam:0];
    }
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller {
    NSLog(@"User cancelled the document picker");
    [ho generateEvent:CND_SELECTOR_CANCEL withParam:0];
    lastError = -5;
}

// Conditions
// --------------------------------------------------
-(BOOL)cndOK
{
    return (lastError == 0);
}
-(BOOL)cndExists:(CCndExtension*)cnd
{
    NSString* path = [cnd getParamExpString:rh withNum:0];
    NSString* altPath = [self findPath:path];
    return [[NSFileManager defaultManager] fileExistsAtPath:path] || (altPath != nil && [[NSFileManager defaultManager] fileExistsAtPath:altPath]);
}
-(BOOL)cndIsReadable:(CCndExtension*)cnd
{
    NSString* path = [cnd getParamExpString:rh withNum:0];
    NSString* altPath = [self findPath:path];
    return [[NSFileManager defaultManager] isReadableFileAtPath:path] || (altPath != nil && [[NSFileManager defaultManager] isReadableFileAtPath:altPath]);
}
-(BOOL)cndIsWritable:(CCndExtension*)cnd
{
    NSString* path = [cnd getParamExpString:rh withNum:0];
    NSString* altPath = [self findPath:path];
    return [[NSFileManager defaultManager] isWritableFileAtPath:path] || (altPath != nil && [[NSFileManager defaultManager] isWritableFileAtPath:altPath]);
}
-(BOOL)cndIsFile:(CCndExtension*)cnd
{
    BOOL bDirectory = NO;
    NSString* path = [cnd getParamExpString:rh withNum:0];
    if ( [[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&bDirectory] )
        return !bDirectory;
    NSString* altPath = [self findPath:path];
    if ( altPath != nil && [[NSFileManager defaultManager] fileExistsAtPath:altPath isDirectory:&bDirectory] )
        return !bDirectory;
    return NO;
}
-(BOOL)cndIsDir:(CCndExtension*)cnd
{
    BOOL bDirectory = NO;
    NSString* path = [cnd getParamExpString:rh withNum:0];
    if ( [[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&bDirectory] )
        return bDirectory;
    NSString* altPath = [self findPath:path];
    if ( altPath != nil && [[NSFileManager defaultManager] fileExistsAtPath:altPath isDirectory:&bDirectory] )
        return bDirectory;
    return NO;
}
-(BOOL)cndSelectorOK
{
    if ((ho->hoFlags & HOF_TRUEEVENT) != 0)
    {
        return YES;
    }
    if ([ho getEventCount] == okEventCount)
    {
        return YES;
    }
    return NO;
}
-(BOOL)cndSelectorCancel
{
    if ((ho->hoFlags & HOF_TRUEEVENT) != 0)
    {
        return YES;
    }
    if ([ho getEventCount] == cancelEventCount)
    {
        return YES;
    }
    return NO;
}
-(BOOL)condition:(int)num withCndExtension:(CCndExtension*)cnd
{
    switch (num)
    {
        case CND_OK:
            return [self cndOK];
        case CND_EXISTS:
            return [self cndExists:cnd];
        case CND_ISREADABLE:
            return [self cndIsReadable:cnd];
        case CND_ISWRITABLE:
            return [self cndIsWritable:cnd];
        case CND_ISFILE:
            return [self cndIsFile:cnd];
        case CND_ISDIR:
            return [self cndIsDir:cnd];
        case CND_SELECTOR_OK:
            return [self cndSelectorOK];
        case CND_SELECTOR_CANCEL:
            return [self cndSelectorCancel];
    }
    return NO;
}


// Actions
// -------------------------------------------------

-(void)setErrorCode:(NSError*)error
{
    if ( error != nil )
        lastError = (int)[error code];
}

-(void)actSetCurrentDir:(CActExtension*)act
{
    NSString* path = [act getParamFilename:rh withNum:0];
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:path];
}
-(void)actRestoreOriginalDir
{
    if ( initialCurrentDir != nil )
        [[NSFileManager defaultManager] changeCurrentDirectoryPath:initialCurrentDir];
}
-(void)actCreateDir:(CActExtension*)act
{
    NSError* error;
    NSString* path = [act getParamFilename:rh withNum:0];
    NSString* altPath = [self findPath:path];
    if ( altPath != nil )
        path = altPath;
    if ( [[NSFileManager defaultManager] createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:&error] == NO )
        [self setErrorCode:error];
}
-(void)actDeleteDir:(CActExtension*)act
{
    NSError* error;
    NSString* path = [act getParamFilename:rh withNum:0];
    NSString* altPath = [self findPath:path];
    if ( altPath != nil )
        path = altPath;
    if ( [[NSFileManager defaultManager] removeItemAtPath:path error:&error] == NO )
        [self setErrorCode:error];
}
-(void)actCreateFile:(CActExtension*)act
{
    NSString* path = [act getParamFilename:rh withNum:0];
    NSString* altPath = [self findPath:path];
    if ( altPath != nil )
        path = altPath;
    if ( [[NSFileManager defaultManager] createFileAtPath:path contents:nil attributes:nil] == NO )
    {
        lastError = -1;
    }
}
-(void)actDeleteFile:(CActExtension*)act
{
    NSError* error;
    NSString* path = [act getParamFilename:rh withNum:0];
    NSString* altPath = [self findPath:path];
    if ( altPath != nil )
        path = altPath;
    if ( [[NSFileManager defaultManager] removeItemAtPath:path error:&error] == NO )
        [self setErrorCode:error];
}
-(void)actRenameFileOrDir:(CActExtension*)act
{
    NSString* path1 = [act getParamFilename:rh withNum:0];
    NSString* altPath = [self findPath:path1];
    if ( altPath != nil )
        path1 = altPath;
    NSString* path2 = [act getParamFilename:rh withNum:1];
    altPath = [self findPath:path2];
    if ( altPath != nil )
        path2 = altPath;
    else
        path2 = [rh->rhApp getPathForWriting:path2];
    NSError* error;
    if ( [[NSFileManager defaultManager] moveItemAtPath:path1 toPath:path2 error:&error] == NO )
        [self setErrorCode:error];
}
-(void)actAppendFileToFile:(CActExtension*)act
{
    NSString* pathToAdd = [act getParamFilename:rh withNum:0];
    NSString* altPath = [self findPath:pathToAdd];
    if ( altPath != nil && [[NSFileManager defaultManager] fileExistsAtPath:altPath] )
        pathToAdd = altPath;
    NSString* pathDest = [act getParamFilename:rh withNum:1];
    altPath = [self findPath:pathDest];
    if ( altPath != nil )
        pathDest = altPath;
    if ( ![[NSFileManager defaultManager] fileExistsAtPath:pathToAdd] )
    {
        lastError = NSFileReadNoSuchFileError;
        return;
    }

    NSOutputStream* os = [NSOutputStream outputStreamToFileAtPath:pathDest append:YES];
    if(os == nil)
    {
        NSError* error;
        if ( [[NSFileManager defaultManager] copyItemAtPath:pathToAdd toPath:pathDest error:&error] == NO )
            [self setErrorCode:error];
        return;
    }
    [os open];

    NSInputStream* is = [NSInputStream inputStreamWithFileAtPath:pathToAdd];
    if(is == nil)
    {
        [os close];
        lastError = NSFileReadNoPermissionError;
        return;
    }
    [is open];
    
    u_int8_t* buffer = (u_int8_t*)malloc(65536);
    if ( buffer != NULL )
    {
        do {
            NSInteger read = [is read:buffer maxLength:65536];
            if ( read < 0 )
            {
                lastError = NSFileReadUnknownError;
                break;
            }
            if ( read != 0 )
            {
                if ( [os write:buffer maxLength:read] < 0 )
                {
                    lastError = NSFileWriteUnknownError;
                    [self setErrorCode:[os streamError]];
                    break;
                }
            }
            if ( read < 65536 )
                break;
        } while(TRUE);
        free(buffer);
    }
    [os close];
    [is close];
}
-(void)actCopyFile:(CActExtension*)act
{
    NSString* path1 = [act getParamFilename:rh withNum:0];
    NSString* altPath = [self findPath:path1];
    if ( altPath != nil && [[NSFileManager defaultManager] fileExistsAtPath:altPath] )
        path1 = altPath;
    NSString* path2 = [act getParamFilename:rh withNum:1];
    altPath = [self findPath:path2];
    if ( altPath != nil )
        path2 = altPath;
    else
        path2 = [rh->rhApp getPathForWriting:path2];
    NSError* error;
    if ( [[NSFileManager defaultManager] copyItemAtPath:path1 toPath:path2 error:&error] == NO )
        [self setErrorCode:error];
}
-(void)actMoveFile:(CActExtension*)act
{
    NSString* path1 = [act getParamFilename:rh withNum:0];
    NSString* altPath = [self findPath:path1];
    if ( altPath != nil && [[NSFileManager defaultManager] fileExistsAtPath:altPath] )
        path1 = altPath;
    NSString* path2 = [act getParamFilename:rh withNum:1];
    altPath = [self findPath:path2];
    if ( altPath != nil )
        path2 = altPath;
    else
        path2 = [rh->rhApp getPathForWriting:path2];
    NSError* error;
    if ( [[NSFileManager defaultManager] moveItemAtPath:path1 toPath:path2 error:&error] == NO )
        [self setErrorCode:error];
}
-(void)actAppendTextToFile:(CActExtension*)act
{
    NSString* text = [act getParamFilename:rh withNum:0];
    NSString* fileName1 = [act getParamFilename:rh withNum:1];
    NSString* path = [rh->rhApp getPathForWriting:fileName1];
    NSOutputStream* os = [NSOutputStream outputStreamToFileAtPath:path append:YES];
    if(os == nil)
    {
        [text writeToFile:path atomically:NO encoding:NSUTF8StringEncoding error:nil];
        return;
    }
    
    NSData* data = [text dataUsingEncoding:NSUTF8StringEncoding];
    [os open];
    [os write:(const u_int8_t*)[data bytes] maxLength:[data length]];
    [os close];
}
-(void)actClearLastError
{
    lastError = 0;
}
-(void)actRunFile:(CActExtension*)act {
    NSString* path = [act getParamFilename:rh withNum:0];
    //NSString* options = [act getParamFilename:rh withNum:1];
    //int flags = 0;

    // Decide how to handle based on the path (document or URL)
    if ([path hasPrefix:@"http"]) {
        NSURL *url = [NSURL URLWithString:path];
        [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
    } else {
        NSURL *fileURL = [NSURL fileURLWithPath:path];
        UIDocumentInteractionController *docController = [UIDocumentInteractionController interactionControllerWithURL:fileURL];
        docController.delegate = self;
        [docController presentPreviewAnimated:YES];
    }
}
-(void)actSetFileSelectorTitle:(CActExtension*)act
{
    fileSelectorTitle = [act getParamExpString:rh withNum:0];
}
-(void)actSetFileSelectorFlags:(CActExtension*)act
{
}
-(void)actResetFileSelectorFlags
{
    [fileTypesArray release];
    fileTypesArray = nil;
}
-(void)actSetFileSelectorFilter:(CActExtension*)act
{
    NSString* filter = [act getParamExpString:rh withNum:0];
    //int defaultFilter = [act getParamExpression:rh withNum:1];
    
    // Clear filetypes array
    [fileTypesArray release];
    allowOtherFileTypes = NO;
    
    fileTypesArray = [[self documentTypesFromLabeledFilterString:filter] retain];
}
-(void)actSetFileSelectorExtension:(CActExtension*)act
{
    // TODO?
    NSString* extension = [act getParamExpString:rh withNum:0];
    if([extension length] > 0) {
        [fileTypesArray release];
        fileTypesArray = [self documentTypesFromLabeledFilterString:extension];
    }

}
- (void)actOpenLoadFileSelector:(CActExtension*)act {
    NSString* path = [act getParamFilename:rh withNum:0];
    NSURL *directoryURL = [NSURL fileURLWithPath:path isDirectory:YES];
    NSArray<NSString *> *documentTypes = fileTypesArray; // Adjust based on your needs
    UIDocumentPickerViewController *documentPicker = [[UIDocumentPickerViewController alloc] initWithDocumentTypes:documentTypes inMode:UIDocumentPickerModeOpen];
    documentPicker.delegate = self;
    documentPicker.allowsMultipleSelection = NO;
    if(@available(ios 13.0, *))
        documentPicker.directoryURL = directoryURL;
    documentPicker.title = fileSelectorTitle;
    documentPicker.modalPresentationStyle = UIModalPresentationFormSheet;
    [ho->hoAdRunHeader->rhApp->mainViewController presentViewController:documentPicker animated:YES completion:nil];
}

- (void)actOpenSaveFileSelector:(CActExtension*)act {
    NSString *localFilePath = [self getFullPathname:[act getParamFilename:rh withNum:0]];
    NSString *documentsDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *fullPath = [documentsDir stringByAppendingPathComponent:localFilePath];

    if ([[NSFileManager defaultManager] fileExistsAtPath:fullPath])
    {
        NSURL *fileURL = [NSURL fileURLWithPath:fullPath];
        UIDocumentPickerViewController *documentPicker = [[UIDocumentPickerViewController alloc] initWithURL:fileURL inMode:UIDocumentPickerModeExportToService];
        documentPicker.delegate = self;
        documentPicker.allowsMultipleSelection = NO;
        documentPicker.title = fileSelectorTitle;
        documentPicker.modalPresentationStyle = UIModalPresentationFormSheet;
        [ho->hoAdRunHeader->rhApp->mainViewController presentViewController:documentPicker animated:YES completion:nil];
    }
    else
    {
        NSLog(@"File not found at path: %@", fullPath);
    }

}

-(void)actSetFileSelectorSingleSelection
{
    multiSelection = NO;
}
-(void)actSetFileSelectorMultiSelection
{
    multiSelection = YES;
}
- (void)actOpenDirSelector:(CActExtension*)act {
    NSString* path = [act getParamFilename:rh withNum:0];
    // Check for iOS 14 or newer
    if (@available(iOS 13.0, *)) {
        NSURL *directoryURL = [NSURL fileURLWithPath:path isDirectory:YES];
        UIDocumentPickerViewController *documentPicker = [[UIDocumentPickerViewController alloc] initWithDocumentTypes:@[@"public.folder"] inMode:UIDocumentPickerModeOpen];
        documentPicker.delegate = self;
        documentPicker.directoryURL = directoryURL;  // Set the initial directory to the path from your extension
        documentPicker.allowsMultipleSelection = multiSelection;
        documentPicker.title = fileSelectorTitle;
        documentPicker.modalPresentationStyle = UIModalPresentationFormSheet;
        [ho->hoAdRunHeader->rhApp->mainViewController presentViewController:documentPicker animated:YES completion:nil];
    } else {
        // For iOS versions lower than 13
        UIAlertController *alertController = [UIAlertController alertControllerWithTitle:@"Not Supported" message:@"Directory selection is not supported on your iOS version." preferredStyle:UIAlertControllerStyleAlert];
        UIAlertAction *okAction = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil];
        [alertController addAction:okAction];
        [ho->hoAdRunHeader->rhApp->mainViewController presentViewController:alertController animated:YES completion:nil];
    }
}

-(void)action:(int)num withActExtension:(CActExtension*)act
{
    switch (num)
    {
        case ACT_DIRSET:
            [self actSetCurrentDir:act];
            break;
        case ACT_DIRSETORG:
            [self actRestoreOriginalDir];
            break;
        case ACT_DIRCREATE:
            [self actCreateDir:act];
            break;
        case ACT_DIRDELETE:
            [self actDeleteDir:act];
            break;
        case ACT_FILECREATE:
            [self actCreateFile:act];
            break;
        case ACT_FILEDELETE:
            [self actDeleteFile:act];
            break;
        case ACT_FILERENAME:
            [self actRenameFileOrDir:act];
            break;
        case ACT_FILEAPPEND:
            [self actAppendFileToFile:act];
            break;
        case ACT_FILECOPY:
            [self actCopyFile:act];
            break;
        case ACT_FILEMOVE:
            [self actMoveFile:act];
            break;
        case ACT_FILEWRITE:
            [self actAppendTextToFile:act];
            break;
        case ACT_CLEARERROR:
            [self actClearLastError];
            break;
        case ACT_RUN:
            [self actRunFile:act];
            break;
        case ACT_SETFSTITLE:
            [self actSetFileSelectorTitle:act];
            break;
        case ACT_SETFSFLAGS:
            [self actSetFileSelectorFlags:act];
            break;
        case ACT_RESETFSFLAGS:
            [self actResetFileSelectorFlags];
            break;
        case ACT_SETFSFILTER:
            [self actSetFileSelectorFilter:act];
            break;
        case ACT_SETFSEXT:
            [self actSetFileSelectorExtension:act];
            break;
        case ACT_OPENLOADFS:
            [self actOpenLoadFileSelector:act];
            break;
        case ACT_OPENSAVEFS:
            [self actOpenSaveFileSelector:act];
            break;
        case ACT_SETFSSINGLESEL:
            [self actSetFileSelectorSingleSelection];
            break;
        case ACT_SETFSMULTIPLESEL:
            [self actSetFileSelectorMultiSelection];
            break;
        case ACT_OPENDIRSELECTOR:
            [self actOpenDirSelector:act];
            break;
    }
}


// Expressions
// --------------------------------------------
-(CValue*)expFileSize
{
    NSError* error;
    NSString* path = [[ho getExpParam] getString];
    NSString* altPath = [self findPath:path];
    if ( altPath != nil )
        path = altPath;
    NSDictionary* info = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:&error];
    if ( info == nil )
    {
        [self setErrorCode:error];
        return [rh getTempValue:0];
    }
    return [rh getTempValue:(int)info.fileSize];
}
-(CValue*)expFileCreationDate
{
    NSError* error;
    NSString* path = [[ho getExpParam] getString];
    NSString* altPath = [self findPath:path];
    if ( altPath != nil )
        path = altPath;
    NSDictionary* info = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:&error];
    if ( info == nil )
    {
        [self setErrorCode:error];
        return [rh getTempValue:0];
    }
    NSDateComponents *components = [[NSCalendar currentCalendar] components:NSCalendarUnitDay | NSCalendarUnitMonth | NSCalendarUnitYear fromDate:info.fileCreationDate];
    NSString* res = [NSString stringWithFormat:@"%2.2d/%2.2d/%2.2d", (int)[components day], (int)[components month], ((int)[components year] % 100)];
    return [rh getTempString:res];
}
-(CValue*)expFileModificationDate
{
    NSError* error;
    NSString* path = [[ho getExpParam] getString];
    NSString* altPath = [self findPath:path];
    if ( altPath != nil )
        path = altPath;
    NSDictionary* info = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:&error];
    if ( info == nil )
    {
        [self setErrorCode:error];
        return [rh getTempValue:0];
    }
    NSDateComponents *components = [[NSCalendar currentCalendar] components:NSCalendarUnitDay | NSCalendarUnitMonth | NSCalendarUnitYear fromDate:info.fileModificationDate];
    NSString* res = [NSString stringWithFormat:@"%2.2d/%2.2d/%2.2d", (int)[components day], (int)[components month], ((int)[components year] % 100)];
    return [rh getTempString:res];
}
-(CValue*)expFileLastAccessDate
{
    //NSString* path = [[ho getExpParam] getString];
    return [self expFileModificationDate];        // no info on Mac?
}
-(NSString*)getFullPathname:(NSString*)path
{
    if ( path != nil && [path length] > 0 && [path characterAtIndex:0] == '/' )
        return path;

    NSString* str = [[NSFileManager defaultManager] currentDirectoryPath];
    int ln = (int)[str length];
    if ( ln == 0 || [str characterAtIndex:ln-1] != '/' )
        str = [str stringByAppendingString:@"/"];
    str = [str stringByAppendingString:path];
    return str;
}
-(CValue*)expDriveName
{
    /*NSString* path =*/ [[ho getExpParam] getString];      // do NOT remove this otherwise it breaks expressions
    return [rh getTempString:@"/"];
}
-(CValue*)expDirName
{
    NSString* path = [[ho getExpParam] getString];
    int ln = (int)[path length];
    if ( ln > 0 )
    {
        NSString* fullPathname = [self getFullPathname:path];
        ln = (int)[fullPathname length];
        if ( ln > 1 )
        {
            NSString* filename = [path lastPathComponent];
            int dirLen = ln - 1 - (int)[filename length];
            if ( dirLen > 0 )
            {
                NSRange nsr = NSMakeRange(1, dirLen);
                NSString* dir = [fullPathname substringWithRange:nsr];
                return [rh getTempString:dir];
            }
        }
    }
    return [rh getTempString:@""];
}
-(CValue*)expFileName
{
    NSString* path = [[ho getExpParam] getString];
    NSString* fileTitle = [[path lastPathComponent] stringByDeletingPathExtension];
    return [rh getTempString:fileTitle];
}
-(CValue*)expExtensionName
{
    NSString* path = [[ho getExpParam] getString];
    NSString* ext = [path pathExtension];
    if ( ext != nil || [ext length] != 0 )
    {
        NSString* pt = @".";
        ext = [pt stringByAppendingString:ext];
    }
    return [rh getTempString:ext];
}
-(CValue*)expFullPathName
{
    NSString* path = [[ho getExpParam] getString];
    NSString* fullPathname = [self getFullPathname:path];
    return [rh getTempString:fullPathname];
}
-(CValue*)expCurrentDir
{
    NSString* str = [[NSFileManager defaultManager] currentDirectoryPath];
    return [rh getTempString:str];
}
-(CValue*)expFileVersionHigh
{
    return [rh getTempValue:0];
}
-(CValue*)expFileVersionLow
{
    return [rh getTempValue:0];
}
-(CValue*)expLastError
{
    return [rh getTempValue:lastError];
}

- (NSString *)pathForTemporaryFileWithPrefix:(NSString *)prefix
{
    NSString *  result;
    CFUUIDRef   uuid;
    CFStringRef uuidStr;
    
    uuid = CFUUIDCreate(NULL);
    assert(uuid != NULL);
    
    uuidStr = CFUUIDCreateString(NULL, uuid);
    assert(uuidStr != NULL);
    
    result = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"%@-%@", prefix, uuidStr]];
    assert(result != nil);
    
    CFRelease(uuidStr);
    CFRelease(uuid);
    
    return result;
}
-(CValue*)expTempFilename
{
    NSString* prefix = [[ho getExpParam] getString];
    NSString* tempPath = [self pathForTemporaryFileWithPrefix:prefix];
    return [rh getTempString:tempPath];
}
-(CValue*)expGetSelectedFile
{
    if ( fileSelectorResult != nil )
    {
        if ( [fileSelectorResult count] > 0 )
        {
            NSURL* url = [fileSelectorResult objectAtIndex:0];
            if ( url != nil )
            {
                return [rh getTempString:[url path]];
            }
        }
    }
    return [rh getTempString:@""];
}
-(CValue*)expFSFlagAllowCreateFilePrompt
{
    return [rh getTempValue:_CREATEPROMPT];
}
-(CValue*)expFSFlagAllowNonExistingFile
{
    return [rh getTempValue:_ALLOWBADFILE];
}
-(CValue*)expFSFlagAllowDirectoryChange
{
    return [rh getTempValue:_CHANGEDIR];
}
-(CValue*)expFSFlagNoNetworkButton
{
    return [rh getTempValue:_NONETWORKBUTTON];
}
-(CValue*)expFSFlagNoOverwritePrompt
{
    return [rh getTempValue:_NOOVERWRITEPROMPT];
}
-(CValue*)expFSFlagAllowNonExistingPath
{
    return [rh getTempValue:_ALLOWBADPATH];
}
-(CValue*)expFSDefaultFilter
{
    // TODO?
    return [rh getTempValue:0];
}
-(CValue*)expFSNumberOfSelectedFiles
{
    if ( fileSelectorResult != nil )
        return [rh getTempValue:(int)[fileSelectorResult count]];
    return [rh getTempValue:0];
}
-(CValue*)expGetSelectedFileAt
{
    int index = [[ho getExpParam] getInt];
    if ( fileSelectorResult != nil )
    {
        int count = (int)[fileSelectorResult count];
        if ( count > 0 && index < count)
        {
            NSURL* url = [fileSelectorResult objectAtIndex:index];
            if ( url != nil )
            {
                return [rh getTempString:[url path]];
            }
        }
    }
    return [rh getTempString:@""];
}
-(CValue*)expFindDriveDirectoryFromLabel
{
    // UNSUPPORTED
    return [rh getTempString:@""];
}
-(CValue*)expSystemDir
{
    NSArray* dirs = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
    if ( dirs != nil && [dirs count] > 0)
    {
        NSString* path = [dirs objectAtIndex:0];
        return [rh getTempString:path];
    }
    return [rh getTempString:@""];
}
-(CValue*)expDocumentsDir
{
    NSArray* dirs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    if ( dirs != nil && [dirs count] > 0)
    {
        NSString* path = [dirs objectAtIndex:0];
        return [rh getTempString:path];
    }
    return [rh getTempString:@""];
}
-(CValue*)expAppDataDir
{
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString* path = [paths objectAtIndex:0];
    return [rh getTempString:path];
}
-(CValue*)expUserDir
{
    NSString* path = NSHomeDirectory();
    return [rh getTempString:path];
}
-(CValue*)expAllUserDir
{
    // TODO?
    return [rh getTempString:@""];
}
-(CValue*)expAllUserDocumentDir
{
    NSArray* dirs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSLocalDomainMask, YES);
    if ( dirs != nil && [dirs count] > 0)
    {
        NSString* path = [dirs objectAtIndex:0];
        return [rh getTempString:path];
    }
    return [rh getTempString:@""];
}
-(CValue*)expAllUserAppDataDir
{
    // UNSUPPORTED
    return [rh getTempString:@""];
}
-(CValue*)expression:(int)num
{
    switch (num)
    {
        case EXP_SIZE:
            return [self expFileSize];
        case EXP_CREATEDATE:
            return [self expFileCreationDate];
        case EXP_MODIFDATE:
            return [self expFileModificationDate];
        case EXP_ACCESSDATE:
            return [self expFileLastAccessDate];
        case EXP_DRIVENAME:
            return [self expDriveName];
        case EXP_DIRNAME:
            return [self expDirName];
        case EXP_FILENAME:
            return [self expFileName];
        case EXP_EXTNAME:
            return [self expExtensionName];
        case EXP_TOTALNAME:
            return [self expFullPathName];
        case EXP_CURRENTDIR:
            return [self expCurrentDir];
        case EXP_FILEVERSIONMS:
            return [self expFileVersionHigh];
        case EXP_FILEVERSIONLS:
            return [self expFileVersionLow];
        case EXP_LASTERROR:
            return [self expLastError];
        case EXP_TEMPFILE:
            return [self expTempFilename];
        case EXP_WINDIR:
            return [rh getTempString:@""];
        case EXP_GETFSRESULT:
            return [self expGetSelectedFile];
        case EXP_CREATEPROMPT:
            return [self expFSFlagAllowCreateFilePrompt];
        case EXP_ALLOWBADFILE:
            return [self expFSFlagAllowNonExistingFile];
        case EXP_CHANGEDIR:
            return [self expFSFlagAllowDirectoryChange];
        case EXP_NONETWORKBUTTON:
            return [self expFSFlagNoNetworkButton];
        case EXP_NOOVERWRITEPROMPT:
            return [self expFSFlagNoOverwritePrompt];
        case EXP_ALLOWBADPATH:
            return [self expFSFlagAllowNonExistingPath];
        case EXP_GETFSDFILTER:
            return [self expFSDefaultFilter];
        case EXP_GETFSNUMBER:
            return [self expFSNumberOfSelectedFiles];
        case EXP_GETFSRESULTAT:
            return [self expGetSelectedFileAt];
        case EXP_DRIVEDIRFROMLABEL:
            return [self expFindDriveDirectoryFromLabel];
        case EXP_SYSDIR:
            return [self expSystemDir];
        case EXP_MYDOCDIR:
            return [self expDocumentsDir];
        case EXP_APPDATADIR:
            return [self expAppDataDir];
        case EXP_USERDIR:
            return [self expUserDir];
        case EXP_ALLUSERDIR:
            return [self expAllUserDir];
        case EXP_ALLUSERDOCDIR:
            return [self expAllUserDocumentDir];
        case EXP_ALLUSERAPPDATADIR:
            return [self expAllUserAppDataDir];
    }
    return [rh getTempValue:0];
}

// Function Helpers

- (NSString*)getUserAppDataPath {
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString* basePath = [paths count] > 0 ? [paths objectAtIndex:0] : NSTemporaryDirectory();

    if (![basePath hasSuffix:@"/"])
        basePath = [basePath stringByAppendingString:@"/"];

    NSString* bundleID = [[NSBundle mainBundle] bundleIdentifier];
    if (bundleID != nil && [bundleID length] > 0) {
        basePath = [basePath stringByAppendingPathComponent:bundleID];
    }
    basePath = [basePath stringByAppendingString:@"/"];

    return basePath;
}

- (NSArray<NSString *> *)documentTypesFromLabeledFilterString:(NSString *)filterString {
    NSDictionary *extToUTI = @{
        @"jpg": @"public.jpeg",
        @"jpeg": @"public.jpeg",
        @"png": @"public.png",
        @"pdf": @"com.adobe.pdf",
        @"xls": @"com.microsoft.excel.xls",
        @"xlsx": @"org.openxmlformats.spreadsheetml.sheet",
        @"doc": @"com.microsoft.word.doc",
        @"docx": @"org.openxmlformats.wordprocessingml.document",
        @"txt": @"public.plain-text",
        @"zip": @"public.zip-archive"
    };

    NSMutableSet<NSString *> *utiSet = [NSMutableSet set];

    // Split by "|"
    NSArray<NSString *> *sections = [filterString componentsSeparatedByString:@"|"];

    for (NSString *section in sections) {
        NSString *trimmed = [section stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];

        // Find if it contains extensions (e.g. "*.pdf;*.doc")
        NSRange starRange = [trimmed rangeOfString:@"*."];
        if (starRange.location != NSNotFound || [trimmed rangeOfString:@";"].location != NSNotFound) {
            // Extract extensions, split by ";"
            NSArray<NSString *> *parts = [trimmed componentsSeparatedByString:@";"];
            for (NSString *part in parts) {
                NSString *cleanExt = [[part stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]
                                      stringByReplacingOccurrencesOfString:@"*." withString:@""];
                cleanExt = [cleanExt lowercaseString];
                NSString *uti = extToUTI[cleanExt];
                if (uti != nil) {
                    [utiSet addObject:uti];
                }
            }
        } else if ([trimmed hasPrefix:@"*."]) {
            // Single extension with "*."
            NSString *cleanExt = [[trimmed substringFromIndex:2] lowercaseString];
            NSString *uti = extToUTI[cleanExt];
            if (uti != nil) {
                [utiSet addObject:uti];
            }
        }
        else if ([trimmed hasPrefix:@"."]) {
            // Single extension with "*."
            NSString *cleanExt = [[trimmed substringFromIndex:1] lowercaseString];
            NSString *uti = extToUTI[cleanExt];
            if (uti != nil) {
                [utiSet addObject:uti];
            }
        }
    }

    return [utiSet allObjects];
}

- (NSString*)findPath:(NSString*)inputPath {
    if (inputPath == nil || inputPath.length == 0)
        return nil;

    // Normalize slashes
    inputPath = [inputPath stringByReplacingOccurrencesOfString:@"\\" withString:@"/"];
    inputPath = [inputPath stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"/"]];

    NSString* lower = [inputPath lowercaseString];
    NSFileManager* fm = [NSFileManager defaultManager];

    // Extract filename (last component)
    NSString* fileName = [inputPath lastPathComponent];

    // Common iOS paths
    NSString* docPath  = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
    NSString* libPath  = [NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES) firstObject];
    NSString* cachePath = [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) firstObject];
    NSString* tmpPath  = NSTemporaryDirectory();
    NSString* bundlePath = [[NSBundle mainBundle] bundlePath];

    // Shortcut keyword mappings
    NSDictionary* keywordMap = @{
        @"documents": docPath,
        @"document": docPath,
        @"library": libPath,
        @"caches": cachePath,
        @"cache": cachePath,
        @"tmp": tmpPath,
        @"temporary": tmpPath,
        @"bundle": bundlePath,
    };

    // Check for keyword prefix (e.g. documents/foo.txt)
    for (NSString* keyword in keywordMap) {
        if ([lower hasPrefix:[keyword stringByAppendingString:@"/"]]) {
            NSString* relative = [inputPath substringFromIndex:keyword.length + 1];
            NSString* result = [keywordMap[keyword] stringByAppendingPathComponent:relative];
            if ([fm fileExistsAtPath:result])
                return result;
        }
    }

    // Absolute path? Try it directly
    if ([fm fileExistsAtPath:inputPath])
        return inputPath;

    // Search common places for exact path
    NSArray* searchRoots = @[docPath, cachePath, tmpPath, libPath, bundlePath];
    for (NSString* base in searchRoots) {
        NSString* fullPath = [base stringByAppendingPathComponent:inputPath];
        if ([fm fileExistsAtPath:fullPath])
            return fullPath;
    }

    // If only filename: lets try if it is located at Documents
    if ([inputPath isEqualToString:fileName]) {
        NSString* fallback = [docPath stringByAppendingPathComponent:fileName];
        if ([fm fileExistsAtPath:fallback])
            return fallback;

    }

    return nil;
}


@end
