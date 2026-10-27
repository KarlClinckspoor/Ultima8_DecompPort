// mingw builds of libmatoya: mingw-w64's headers define these interface ids themselves once
// initguid.h is in, as libmatoya's audio.c also does; reading the headers first, without it,
// leaves only libmatoya's definitions. The one property key it relies on the headers for is
// defined after.
#define COBJMACROS
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include "windows/audio.c"

const PROPERTYKEY PKEY_AudioEngine_DeviceFormat =
	{{0xf19f064d, 0x082c, 0x4e27, {0xbc, 0x73, 0x68, 0x82, 0xa1, 0xbb, 0x8e, 0x4c}}, 0};
