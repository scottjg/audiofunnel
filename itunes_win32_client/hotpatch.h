#ifndef __HOTPATCH__H_
#define __HOTPATCH__H_
#include <windows.h>

BOOL hotpatch_api(void **api_function, void *handler);
#endif
