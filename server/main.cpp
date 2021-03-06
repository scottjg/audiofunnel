#include <stdio.h>
#ifdef WIN32
#include <winsock2.h>

typedef int socklen_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef int SOCKET;
typedef unsigned char BOOL;
#define closesocket close
#define INVALID_SOCKET -1
#endif

#include <FLAC/all.h>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_rotozoom.h>
#include <vector>
#include <string>
#include <sstream>
#include "ringb.h"

using std::string;
using std::vector;
using std::istringstream;

#define PORT 54321
#define X_RES 1024
#define Y_RES 768

//#define printf(n, ...) ;
FILE * pFile;
FLAC__StreamDecoder *dec;
Uint32 bps, channels, hz; //bytes per sample, # of channels, frequency
SOCKET client_sd;
BOOL need_to_rebuffer;

string track_info;
SDL_Surface *album_art;

int server_thread(void *unused);
int art_thread(void *unused);
vector<string> split(const string &str, char delimitor);
string strclip(const string &str);
void blit_album_art(SDL_Surface *screen, TTF_Font *font1, TTF_Font *font2);

BOOL fill_ringb()
{
	//printf("filling...\n");
	while(ringb_length() < (ringb_max()/2))
	{
	    if(!FLAC__stream_decoder_process_single(dec))
	    {
		//printf("Failed to fill.\n");
	        return false;
		}
		else if(FLAC__stream_decoder_get_state(dec) == FLAC__STREAM_DECODER_ABORTED)
			return false;
	}
	//printf("filled!\n");
	

	return true;
}

void fill_audio(void *udata, Uint8 *stream, int len)
{
	if (len < 0)
		return;

	if((unsigned)len <= ringb_length())
	{
		ringb_deq(stream,len);
		printf("::%d %d %d %d\n", stream[0], stream[1], stream[2], stream[3]);
		//fwrite (stream , sizeof(stream[0]) , len , pFile );
	}
	else
	{
	    need_to_rebuffer=true;
	    SDL_PauseAudio(1);
	}
}

FLAC__StreamDecoderWriteStatus decodeCallback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
{
	Uint32 blocks = frame->header.blocksize;
	unsigned i,j;

	bps = frame->header.bits_per_sample/8; //bytes per sample
	channels = frame->header.channels;
	hz = frame->header.sample_rate;

	for(i=0; i < blocks; i++)
	    for(j=0; j < channels; j++)
	    {
		    SDL_LockAudio();
			ringb_append((Uint8 *)&(buffer[j][i]), bps);
		    SDL_UnlockAudio();
		}
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

FLAC__StreamDecoderReadStatus read_music(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	int len_so_far = 0;
	do
	{
		//printf("recving...\n");
		int s = recv(client_sd, (char *)&(buffer[len_so_far]), (*bytes)-len_so_far,0);
		//printf("read %d bytes.\n",s);
		if(s > 0)
			len_so_far += s;
		else
		    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	} while(len_so_far < (*bytes));
	printf("%x %x %x %x %x %x\n",buffer[0], buffer[1], buffer[2],buffer[3], buffer[4], buffer[5]);
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

void errorCallback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	fprintf(stderr, "ERROR decoding: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
	//printf("error! %d\n", status);
}

int main(int argc, char *argv[])
{
	SDL_Event event;
	SDL_Rect rect;
	TTF_Font *font1, *font2;
	//init winsock
	#ifdef WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD( 2, 2 ),&wsaData);
	#endif
	
	if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0)
	{
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
		return -1;
	}

	if (TTF_Init() == -1)  
	{
		fprintf(stderr, "Unable to initialize SDL_ttf: %s \n", TTF_GetError());
		return -1;
	}

	font1 = TTF_OpenFont("c:/windows/fonts/arial.ttf", 54);
	font2 = TTF_OpenFont("c:/windows/fonts/arial.ttf", 44);
	if (font1 == NULL || font2 == NULL){
		printf("Unable to load font: %s \n", TTF_GetError());
		return -1;
	}

	//SDL_Surface *screen = SDL_SetVideoMode ( X_RES, Y_RES, 0, SDL_FULLSCREEN | SDL_ANYFORMAT) ;
	SDL_Surface *screen = SDL_SetVideoMode ( X_RES, Y_RES, 0, SDL_ANYFORMAT) ;
	if(screen == NULL)
	{
		fprintf(stderr, "Couldn't set video mode: %s\n", SDL_GetError());
		return -1;
	}

	/* Blit the logo. */
	SDL_Surface *logo = SDL_LoadBMP("audioFunnelLogo.bmp");
	if (!logo){
		printf("Unable to load logo: %s \n", SDL_GetError());
		return -1;
	}
	rect.x = (X_RES-logo->w)/2;
	rect.y = (Y_RES-logo->h)/2;
	rect.w = logo->w;
	rect.h = logo->h;
	SDL_BlitSurface(logo, NULL, screen, &rect);
	SDL_FreeSurface(logo);
	SDL_Flip(screen);

	// XXX need to cleanup threads
	SDL_CreateThread(server_thread, NULL);
	SDL_CreateThread(art_thread, NULL);

	while(SDL_WaitEvent(&event)) {
			switch(event.type) {
				case SDL_QUIT:
					goto end;
					break;
				case SDL_USEREVENT:
					blit_album_art(screen, font1, font2);
					break;
				default:
					break;
			}
	}
end:
	TTF_CloseFont(font1);
	TTF_CloseFont(font2);

	SDL_Quit();
	return 0;

}

int server_thread(void *unused)
{
	struct sockaddr_in server_addr;	// Internet address struct
	struct sockaddr_in client_addr;
	unsigned int ca_size;	//socket descriptor
	SOCKET sd;

	unsigned long val1;
	unsigned long val;
	SDL_AudioSpec spec;

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if(sd == INVALID_SOCKET)
		return sd;

   	//set socket as non-blocking
	//val1 = 1;
	//if(fcntl(sd, F_SETFL, fcntl(sd, F_GETFD, 0)|O_NONBLOCK) < 0)
	//if(ioctlsocket(sd, FIONBIO, &val1 ) < 0)
	//	return -1;


	//attempt to bind the socket to the addr and listen on the port
	//return -1 on error
	val1 = 1;
	if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (const char *)&val1, sizeof(val1))<0)
	{
		perror("setsockopt REUSEADDR");
		return 1;
	}

	if( bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) )
		return -1;

	if( listen(sd, 1) < 0)
		return -1;
		
	ca_size = sizeof(client_addr);
	while(1)
	{
		printf("waiting for connection...");
   		client_sd = accept(sd, (struct sockaddr *)&client_addr, (socklen_t *)&ca_size);
	   	
   		val = 5000;
		setsockopt(client_sd, SOL_SOCKET,SO_RCVTIMEO, (const char *)&val, sizeof(val));
		printf("got connection!\n");
		

		need_to_rebuffer = false;
		hz = 0;
		ringb_init();
		dec = FLAC__stream_decoder_new();
		FLAC__stream_decoder_init_stream(dec, read_music,NULL,NULL,NULL,NULL,decodeCallback,NULL,errorCallback,NULL);


		//decode until we get the first audio frame which gives us format info
		while(hz == 0)
			if(!FLAC__stream_decoder_process_single(dec))
			{
				closesocket(client_sd);
				client_sd = INVALID_SOCKET;
				break;
			}
		if(client_sd == INVALID_SOCKET)
			continue;
		printf("got first audio frame!\n");

		spec.freq = hz;
		spec.format = bps==1?AUDIO_S8:AUDIO_S16SYS;
		spec.channels = channels;
		spec.samples = FLAC_MAX_BLOCKSIZE;
		spec.callback = fill_audio;


		if(SDL_OpenAudio(&spec, NULL) < 0)
 		{
			fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
			return(-1);
		}
		//resize ringbuffer to respect the given buffer size
		ringb_resize(spec.size*100);
		printf("spec.size=%d\n",spec.size);
		
		fill_ringb();
		
		SDL_PauseAudio(0);

		while(fill_ringb())
		{

			if(need_to_rebuffer)
			{
				printf("rebuffering...");
				fflush(stdout);
				if(!fill_ringb())
					break;
				need_to_rebuffer=false;
				SDL_PauseAudio(0);
				printf("done\n");
			}
			SDL_Delay(100);
		}
		SDL_PauseAudio(1);
		SDL_CloseAudio();
		closesocket(client_sd);
		ringb_reset();
	}

	return 0;
}

int art_thread(void *unused)
{
	struct sockaddr_in server_addr;	// Internet address struct
	struct sockaddr_in client_addr;
	unsigned int ca_size;	//socket descriptor
	unsigned int len=0;
	int s;
	char *buffer;
	SOCKET sd;
	SOCKET client_sd;
	char code;

	unsigned long val1;
	unsigned long val;
	SDL_ShowCursor(0);

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT+1);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if(sd == INVALID_SOCKET)
		return sd;

   	//set socket as non-blocking
	//val1 = 1;
	//if(fcntl(sd, F_SETFL, fcntl(sd, F_GETFD, 0)|O_NONBLOCK) < 0)
	//if(ioctlsocket(sd, FIONBIO, &val1 ) < 0)
	//	return -1;


	//attempt to bind the socket to the addr and listen on the port
	//return -1 on error
	val1 = 1;
	if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (const char *)&val1, sizeof(val1))<0)
	{
		perror("setsockopt REUSEADDR");
		return 1;
	}

	if( bind(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) )
		return -1;

	if( listen(sd, 1) < 0)
		return -1;
		
	while(1)
	{
		printf("waiting for metadata connection...");
		ca_size = sizeof(client_addr);
   		client_sd = accept(sd, (struct sockaddr *)&client_addr, (socklen_t *)&ca_size);
   		val = 5000;
		setsockopt(client_sd,SOL_SOCKET,SO_RCVTIMEO, (const char *)&val, sizeof(val));
		printf("got metadata connection!\n");
		

		if(recv(client_sd, &code, 1, 0) < 1)
		{
			closesocket(client_sd);
			continue;
		}
			
		if(recv(client_sd, (char *)&ca_size, sizeof(ca_size), 0) < (int)sizeof(ca_size))
		{
			closesocket(client_sd);
			continue;
		}

		ca_size = ntohl(ca_size);
		len=0;
		buffer = new char[ca_size+1];
		while(len < ca_size)
		{
			s = recv(client_sd, &buffer[len], ca_size-len, 0);
			if(s <= 0)
				break;
			len += s;
		}
		
		if(len < ca_size)
		{
			closesocket(client_sd);
			break;
		}

		buffer[len] = 0;

		if(code == 'T')
		{
			track_info = string(buffer);
			printf("got track info: %s\n", track_info.c_str());
		}
		else if(code == 'A')
		{
			SDL_RWops *rwops = SDL_RWFromMem(buffer, len);
			SDL_Surface *surface = IMG_Load_RW(rwops, 1);
			if (album_art) {
				SDL_FreeSurface(album_art);
			}
			album_art = rotozoomSurface(surface, 0, 600.0/surface->h, 1);
			SDL_FreeSurface(surface);
			printf("got album art\n");
		}
		SDL_Event e;
		e.type = SDL_USEREVENT;
		SDL_PushEvent(&e);
		delete[] buffer;
		closesocket(client_sd);
	}

	return 0;
}

void blit_album_art(SDL_Surface *screen, TTF_Font *font1, TTF_Font *font2)
{
	/* first, process and blit the track information text */
	string title, artist, album;
	vector<string> fields = split(track_info, '\n');
	if(fields.size() > 0)
		title = strclip(fields[0]);
	else
		title = "";

	if(fields.size() > 1)
		artist = strclip(fields[1]);
	else
		artist = "";

	if(fields.size() > 2)
		album = strclip(fields[2]);
	else
		album = "";

	SDL_Color color = { 0xFF, 0xFF, 0xFF, 0xFF};
	SDL_Surface *titlesurface = TTF_RenderText_Blended(font1, title.c_str(), color);
	SDL_Surface *artistsurface = TTF_RenderText_Blended(font2, artist.c_str(), color);
	SDL_Surface *albumsurface = TTF_RenderText_Blended(font2, album.c_str(), color);

	SDL_FillRect(screen, NULL, 0);
	SDL_Rect r = {80, 730, 0, 0};
	if(titlesurface)
	{
		SDL_BlitSurface(titlesurface, NULL, screen, &r);
		r.y +=titlesurface->h -4;
		if(artistsurface)
		{
			SDL_BlitSurface(artistsurface, NULL, screen, &r);
			r.y +=artistsurface->h ;
			SDL_BlitSurface(albumsurface, NULL, screen, &r);
		}
	}
	SDL_FreeSurface(titlesurface);
	SDL_FreeSurface(artistsurface);
	SDL_FreeSurface(albumsurface);

	if (album_art)
	{
		/* then, blit the actual album art */
		r.x = X_RES-album_art->w>0?(X_RES-album_art->w)/2:0;
		r.y = 90;
		r.h = album_art->h;
		r.w = album_art->w;
		SDL_BlitSurface(album_art, NULL, screen, &r);
		SDL_Flip(screen);

		SDL_FreeSurface(album_art);
		album_art = NULL;
	}
	SDL_Flip(screen);

}

vector<string> split(const string &str, char delimiter)
{
	vector<string> arr;
	
	istringstream ss(str);
	string tmp;
	while(getline(ss, tmp, delimiter))
		arr.push_back(tmp);

	return arr;
}

string strclip(const string &str)
{
	if(str.size() > 40)
		return str.substr(0, 40) + "...";
	else
		return str;
}
