#ifndef _RINGB_H
#define RINGB_H
#include <SDL.h>
#define FLAC_MAX_BLOCKSIZE 4608
#define RINGB_MAXLEN FLAC_MAX_BLOCKSIZE*2 *2/*channels*/

#ifdef __cplusplus
extern "C" {
#endif
void ringb_init();
void ringb_append(Uint8 *data, Uint32 len);
void ringb_deq(Uint8 *data, Uint32 len);
Uint32 ringb_length();
unsigned ringb_max();
void ringb_resize(unsigned size);
void ringb_reset();
#ifdef __cplusplus
}
#endif
#endif
