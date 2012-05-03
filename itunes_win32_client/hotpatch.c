#include "hotpatch.h"

/* 
 * 32-bit Windows APIs usually start with:
 *    mov edi, edi
 * and before the function starts, they have 5 bytes of padding. This leaves
 * enough room to inject, at the beginning of the function, a relative short
 * jump to the top of the padding. The padding is big enough to fit a relative
 * long jump. 
 *
 * This means we can hot-patch in a jump to our hook without disturbing the
 * real code in the original function.
 */
BOOL hotpatch_api(void **api_function, void *handler) {
	DWORD old_protection_state;
	BOOL status;
	__int64 original_code, new_code, result;
	unsigned char *new_code_bytes = (char *)&new_code;
	//start at the 5 bytes of padding
	unsigned char *api_function_bytes = (char *)*api_function - 5;

	//make sure we have write access to the code.
	status = VirtualProtect(api_function_bytes, sizeof(original_code),
	                        PAGE_EXECUTE_READWRITE, &old_protection_state);
	if(status == FALSE)
	{
		MessageBoxA(0, "VirtualProtect() failed :(", "Audio Funnel", MB_OK);
		return FALSE;
	}

	//fetch original code to start with.
	original_code = InterlockedCompareExchange64((__int64 *)api_function_bytes, 0, 0);
	new_code = original_code;

	new_code_bytes[0] = 0xe9; //long jump (relative)
	//use the relative offset starting from the end of the jump
	*(unsigned int *)&(new_code_bytes[1]) = 
		(unsigned int)((unsigned int)handler - (unsigned int)*api_function);

	new_code_bytes[5] = 0xeb; //short jump (relative) <--- this is where the api function actually starts
	new_code_bytes[6] = 0xf9; //jump to the beginning of this injected code

	result = InterlockedCompareExchange64((__int64 *)api_function_bytes, new_code, original_code);
	//verify that we swapped with the normal hot patch preamble 
	//(nop; nop; nop; nop; nop; mov edi, edi;)
	if((result & 0x00ffffffffffffff) != 0x00ff8b9090909090)
	{
		MessageBoxA(0, "DirectSound code was already hooked before we started. Audio Funnel will shut down to avoid interfering with another similar audio interception application.",
		            "Audio Funnel", MB_OK | MB_ICONWARNING);
		return FALSE;
	}

	//restore original page protection to the code (probably read+execute only)
	VirtualProtect(api_function_bytes, sizeof(original_code), old_protection_state, 
	               &old_protection_state);

	//to call the original function, start after the jump into our code
	*api_function = (void *)((unsigned int)(*api_function) + 2);

	return TRUE;
}