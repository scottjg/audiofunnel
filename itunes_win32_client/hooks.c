#include "dsound.h"
#include "hotpatch.h"
#include <stdio.h>
#include <FLAC/all.h>
#include "sockets.h"
#include "hooks.h"

FLAC__StreamEncoder *enc;
static unsigned int bps, channels, bytes_per_sample, hz;
char *server = "127.0.0.1";

HRESULT (STDMETHODCALLTYPE *directsound_unlock)(IDirectSoundBuffer *FAR This, LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2);

FLAC__StreamEncoderWriteStatus send_music(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void *client_data)
{
	//when music is encoded, send it over the wire
	if(!datasend((void *)buffer, bytes))
	   return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
	return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

HRESULT STDMETHODCALLTYPE unlock_hook(IDirectSoundBuffer *FAR This, LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2)
{
	unsigned int *audio1, *audio2;
	unsigned int i, j;

	//when iTunes is feeding the audio buffer for the first time
	if(!enc)
	{
		//if we need to read audio parameters, do it
		if(bps==0)
		{
			WAVEFORMATEX pcfxFormat;
			This->lpVtbl->GetFormat(This, &pcfxFormat, sizeof(WAVEFORMATEX), NULL);
			bytes_per_sample=pcfxFormat.nAvgBytesPerSec/pcfxFormat.nSamplesPerSec/pcfxFormat.nChannels;

			channels=pcfxFormat.nChannels;
			bps=pcfxFormat.wBitsPerSample;
			hz =pcfxFormat.nSamplesPerSec;
		}

		//initialize the encoder with the audio paramters
		enc = FLAC__stream_encoder_new();
		//the default compression level is 5 (which is adequate)
		//FLAC__stream_encoder_set_compression_level(enc,8);
		FLAC__stream_encoder_set_channels(enc,channels);
		FLAC__stream_encoder_set_bits_per_sample(enc,bps);
		FLAC__stream_encoder_set_sample_rate(enc,hz);
		ipconnect(server);
    	FLAC__stream_encoder_init_stream(enc,send_music,NULL,NULL,NULL,NULL);
		
	}

	//realign audio buffers to 32-bit blocks and encode(& send)
	audio1 = malloc(dwAudioBytes1/bytes_per_sample*channels*4);
	audio2 = malloc(dwAudioBytes2/bytes_per_sample*channels*4);

	for(i=0; i < dwAudioBytes1/bytes_per_sample; i++)
	{
    	audio1[i] = 0;
		for(j=0; j < bytes_per_sample; j++)
			((unsigned char *)audio1)[4*i+j] = ((unsigned char *)pvAudioPtr1)[i*bytes_per_sample+j];
	}

	for(i=0; i < dwAudioBytes2/bytes_per_sample; i++)
	{
    	audio2[i] = 0;
		for(j=0; j < bytes_per_sample; j++)
			((unsigned char *)audio2)[4*i+j] = ((unsigned char *)pvAudioPtr2)[i*bytes_per_sample+j];
	}

	memset(pvAudioPtr1,0,dwAudioBytes1);
	memset(pvAudioPtr2,0,dwAudioBytes2);

	if(!FLAC__stream_encoder_process_interleaved(enc,audio1,dwAudioBytes1/bytes_per_sample/channels)
	|| !FLAC__stream_encoder_process_interleaved(enc,audio2,dwAudioBytes2/bytes_per_sample/channels))
	{
		FLAC__stream_encoder_finish(enc);
		FLAC__stream_encoder_delete(enc);
		disconnect();		
		enc = NULL;
	}

    free(audio1);
    free(audio2);

	return directsound_unlock(This, pvAudioPtr1, dwAudioBytes1, pvAudioPtr2, dwAudioBytes2);
}

DWORD WINAPI hot_patch_thread_func( LPVOID lpParam )
{
	char tmp[256];
	IDirectSound *pds;
	IDirectSoundBuffer *pdsb;
	WAVEFORMATEX fmt = { 0 };
	DSBUFFERDESC bufferDesc = { 0 };
	HRESULT status;
	
	status = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (status != S_OK)
	{
		sprintf(tmp, "CoInitializeEx failed: status=0x%x, last error=0x%x", status, GetLastError());
		MessageBoxA(0, tmp, "Audio Funnel", MB_OK);
		return 1;
	}

	status = DirectSoundCreate(0,&pds,0);
	if (status != S_OK)
	{
		sprintf(tmp, "DirectSoundCreate failed: status=0x%x, last error=0x%x", status, GetLastError());
		MessageBoxA(0, tmp, "Audio Funnel", MB_OK);
		return 1;
	}

	fmt.wFormatTag = WAVE_FORMAT_PCM;
	fmt.nChannels = 2;
	fmt.nSamplesPerSec = 22050;
	fmt.wBitsPerSample = 16;
	fmt.nBlockAlign = fmt.wBitsPerSample/8*fmt.nChannels;
	fmt.nAvgBytesPerSec = fmt.nSamplesPerSec*fmt.nBlockAlign;

	bufferDesc.dwSize = sizeof(bufferDesc);
	bufferDesc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY;
	bufferDesc.dwBufferBytes = 512;
	bufferDesc.lpwfxFormat = &fmt; 

	status = pds->lpVtbl->CreateSoundBuffer(pds,&bufferDesc,&pdsb,NULL);
	if (status != DS_OK)
	{
		sprintf(tmp, "CreateSoundBuffer failed: status=0x%x, last error=0x%x", status, GetLastError());
		MessageBoxA(0, tmp, "Audio Funnel", MB_OK);
		return 1;
	}

	directsound_unlock = pdsb->lpVtbl->Unlock;

	pdsb->lpVtbl->Release(pdsb);
	pds->lpVtbl->Release(pds);
	CoUninitialize();

	hotpatch_api(&(void *)directsound_unlock, unlock_hook);
	bps=0;
	if(enc != NULL)
		FLAC__stream_encoder_delete(enc);
	enc = NULL;
	return 0;
}

//since this might be happening from DllMain, avoid deadlocks by putting
//code in a thread
void do_hotpatch()
{
	HANDLE h = CreateThread(NULL, 0, hot_patch_thread_func, NULL, 0, NULL);
	CloseHandle(h);
}
