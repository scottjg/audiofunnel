// audiofunnel.cpp : Defines the entry point for the DLL application.
//

#include <windows.h>
#include <FLAC/all.h>
#include "hooks.h"
#include "sockets.h"

//#include "sockets.h"

#include "iTunesApi/iTunesAPI.h"
#include "iTunesApi/iTunesVisualAPI.h"
//#include <dsound.h>
#define kSampleVisualPluginName "Audio Funnel"
#define kSampleVisualPluginCreator 'sjg_'
#define kSampleVisualPluginMajorVersion 1
#define kSampleVisualPluginMinorVersion 0
#define kSampleVisualPluginReleaseStage 0
#define kSampleVisualPluginNonFinalRelease 0

#define IMPEXP __declspec(dllexport)
#define GRAPHICS_DEVICE	HWND
static OSStatus VisualPluginHandler (OSType message, VisualPluginMessageInfo *messageInfo, void *refCon);
static OSStatus RegisterVisualPlugin (PluginMessageInfo *messageInfo);

//kind of a hack -- we load these at runtime from a QuickTime DLL. 
//I need these and they aren't defined in the sdk!
static unsigned (* GetHandleSize)(Handle h);
static void (* DisposeHandle)(Handle h);


struct VisualPluginData {
	void *				appCookie;
	ITAppProcPtr		appProc;

#if TARGET_OS_MAC
	ITFileSpec			pluginFileSpec;
#endif
	
	GRAPHICS_DEVICE		destPort;
	Rect				destRect;
	OptionBits			destOptions;
	UInt32				destBitDepth;

	RenderVisualData	renderData;
	UInt32				renderTimeStampID;
	
	SInt8				waveformData[kVisualMaxDataChannels][kVisualNumWaveformEntries];
	
	UInt8				level[kVisualMaxDataChannels];		/* 0-128 */
	
	ITTrackInfoV1		trackInfo;
	ITStreamInfoV1		streamInfo;

	Boolean				playing;
	Boolean				padding[3];

/*
	Plugin-specific data
*/
#if TARGET_OS_MAC
	GWorldPtr			offscreen;
#endif
};
typedef struct VisualPluginData VisualPluginData;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{

	if(ul_reason_for_call == DLL_PROCESS_ATTACH) //dll attach
	{
		WSADATA wsaData;
	    WSAStartup(MAKEWORD( 2, 2 ),&wsaData);
	}
	else if(ul_reason_for_call == DLL_PROCESS_DETACH)
	{

	}

    return TRUE;
}


IMPEXP OSStatus iTunesPluginMain(OSType message, PluginMessageInfo *messageInfo, void *refCon)
{
    OSStatus		status;
	HMODULE mod;
	switch (message)
	{
		case kPluginInitMessage:
			status = RegisterVisualPlugin(messageInfo);
			//MessageBoxA(NULL,"registered!","",MB_OK);
			/*
			LPDIRECTSOUND ds;
			if(DirectSoundCreate(NULL,&ds,NULL)== DS_OK)
			{
				LPDIRECTSOUNDBUFFER sb;
				if(ds->lpVtbl->CreateSoundBuffer(ds,NULL,&sb,NULL) == DS_OK)
				    MessageBoxA(NULL,"Created Buffer!","",MB_OK);
				
			}
			*/
			mod = LoadLibrary(L"C:/Program Files (x86)/Quicktime/QTSystem/QTMLClient.dll");
			GetHandleSize = (unsigned (__cdecl *)(Handle))GetProcAddress(mod, "GetHandleSize");
			DisposeHandle = (void (__cdecl *)(Handle))GetProcAddress(mod, "DisposeHandle");
			do_hotpatch();
			break;

		case kPluginCleanupMessage:
			status = noErr;
			break;

		default:
			status = unimpErr;
	}
	return status;
}

static OSStatus RegisterVisualPlugin (PluginMessageInfo *messageInfo)
{
	OSStatus			status;
	PlayerMessageInfo	playerMessageInfo;

	memset(&playerMessageInfo.u.registerVisualPluginMessage,0,sizeof(playerMessageInfo.u.registerVisualPluginMessage));

	// copy in name length byte first
	playerMessageInfo.u.registerVisualPluginMessage.name[0] = strlen(kSampleVisualPluginName);
	// now copy in actual name
	memcpy(&playerMessageInfo.u.registerVisualPluginMessage.name[1], kSampleVisualPluginName, strlen(kSampleVisualPluginName));

	SetNumVersion(&playerMessageInfo.u.registerVisualPluginMessage.pluginVersion, kSampleVisualPluginMajorVersion, kSampleVisualPluginMinorVersion, kSampleVisualPluginReleaseStage, kSampleVisualPluginNonFinalRelease);

	playerMessageInfo.u.registerVisualPluginMessage.options					= 0;//kVisualWantsIdleMessages | kVisualWantsConfigure;
	playerMessageInfo.u.registerVisualPluginMessage.handler					= VisualPluginHandler;
	playerMessageInfo.u.registerVisualPluginMessage.registerRefCon			= 0;
	playerMessageInfo.u.registerVisualPluginMessage.creator					= kSampleVisualPluginCreator;

	playerMessageInfo.u.registerVisualPluginMessage.timeBetweenDataInMS		= 0xFFFFFFFF; // 16 milliseconds = 1 Tick, 0xFFFFFFFF = Often as possible.
	playerMessageInfo.u.registerVisualPluginMessage.numWaveformChannels		= 2;
	playerMessageInfo.u.registerVisualPluginMessage.numSpectrumChannels		= 2;

	playerMessageInfo.u.registerVisualPluginMessage.minWidth				= 64;
	playerMessageInfo.u.registerVisualPluginMessage.minHeight				= 64;
	playerMessageInfo.u.registerVisualPluginMessage.maxWidth				= 32767;
	playerMessageInfo.u.registerVisualPluginMessage.maxHeight				= 32767;
	playerMessageInfo.u.registerVisualPluginMessage.minFullScreenBitDepth	= 0;
	playerMessageInfo.u.registerVisualPluginMessage.maxFullScreenBitDepth	= 0;
	playerMessageInfo.u.registerVisualPluginMessage.windowAlignmentInBytes	= 0;

	status = PlayerRegisterVisualPlugin(messageInfo->u.initMessage.appCookie, messageInfo->u.initMessage.appProc,&playerMessageInfo);

	return status;

}

static OSStatus VisualPluginHandler (OSType message, VisualPluginMessageInfo *messageInfo, void *refCon)
{
	OSStatus status = noErr;
	char *msg = (char *)&message;
	VisualPluginData *	visualPluginData = (VisualPluginData *)refCon;
	Handle albumArt;
	OSType albumArtType;
	OSType r;

	switch(message)
	{
		case kVisualPluginInitMessage:
		{
			visualPluginData = (VisualPluginData *)malloc(sizeof(VisualPluginData));
			if (visualPluginData == nil)
			{
				status = memFullErr;
				break;
			}

			visualPluginData->appCookie	= messageInfo->u.initMessage.appCookie;
			visualPluginData->appProc	= messageInfo->u.initMessage.appProc;

			messageInfo->u.initMessage.refCon	= (void *)visualPluginData;
			break;
		}

		case kVisualPluginPlayMessage:
			sendSongTitle(server, messageInfo->u.playMessage.trackInfo->name+1, messageInfo->u.playMessage.trackInfo->artist+1, messageInfo->u.playMessage.trackInfo->album+1);
			r = PlayerGetCurrentTrackCoverArt(visualPluginData->appCookie, visualPluginData->appProc, &albumArt, &albumArtType);
			if(albumArt)
				sendAlbumArt(server, albumArt[0], GetHandleSize(albumArt));
		break;
		case kVisualPluginChangeTrackMessage:			
			sendSongTitle(server, messageInfo->u.changeTrackMessage.trackInfo->name+1, messageInfo->u.changeTrackMessage.trackInfo->artist+1, messageInfo->u.changeTrackMessage.trackInfo->album+1);
			r = PlayerGetCurrentTrackCoverArt(visualPluginData->appCookie, visualPluginData->appProc, &albumArt, &albumArtType);
			if(albumArt)
				sendAlbumArt(server, albumArt[0], GetHandleSize(albumArt));
		break;

		case kVisualPluginEnableMessage:
		break;

		default:
			status = unimpErr;

	}

	return status;
}

