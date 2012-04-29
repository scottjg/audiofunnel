#include "ringb.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>


static Uint8 *ringb;
static Uint32 ringb_len;
static Uint32 ringb_pos;
static Uint32 ringb_maxsize;
static SDL_mutex *lock;

void ringb_reset()
{
	ringb_len = 0;
	ringb_pos = 0;
}

Uint32 ringb_length()
{
	return ringb_len;
}

void ringb_init()
{
	ringb_len = 0;
	ringb_pos = 0;
	ringb_maxsize = RINGB_MAXLEN;
	ringb = (Uint8 *)malloc(ringb_maxsize);
	lock = SDL_CreateMutex();
}

void ringb_resize(unsigned size)
{
	SDL_LockMutex(lock);
	ringb = (Uint8 *)realloc(ringb, size);
	ringb_maxsize = size;
	SDL_UnlockMutex(lock);
}

unsigned ringb_max()
{
	return ringb_maxsize;
}

void ringb_append(Uint8 *data, Uint32 len)
{
	Uint32 eob;
	Uint32 cpylen2;
	Uint32 cpylen1;

	SDL_LockMutex(lock);
	eob = (ringb_pos + ringb_len) % ringb_maxsize;
	cpylen2 = eob + len;

	cpylen2 = (cpylen2 > ringb_maxsize ? cpylen2 % ringb_maxsize : 0);
	cpylen1 = len - cpylen2;
	memcpy(&(ringb[eob]),data,cpylen1);
	memcpy(ringb,&(data[cpylen1]),cpylen2);
	ringb_len += len;
	SDL_UnlockMutex(lock);
}

void ringb_deq(Uint8 *data, Uint32 len)
{
   	Uint32 len_right;
	Uint32 cpylen_right;
	Uint32 cpylen_left;

	SDL_LockMutex(lock);
   	len_right = ringb_maxsize - ringb_pos;
	cpylen_right = (len > len_right ? len_right : len);
	cpylen_left = len - cpylen_right;

	assert(len <= ringb_len);

	memcpy(data,&(ringb[ringb_pos]),cpylen_right);
	memcpy(&(data[cpylen_right]),ringb, cpylen_left);

	ringb_pos = (ringb_pos+len) % ringb_maxsize;
	ringb_len -= len;
	SDL_UnlockMutex(lock);
}
