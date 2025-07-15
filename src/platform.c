#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

void scrap_set_env(const char* name, const char* value) {
#ifdef _WIN32
    char buf[256];
    snprintf(buf, 256, "%s=%s", name, value);
    putenv(buf);
    SetEnvironmentVariableA(name, value);
#else
    setenv(name, value, false);
#endif // _WIN32
}

#if defined(RAM_OVERLOAD) && defined(_WIN32)
#include <shlobj.h>
#include <shlwapi.h>

#define SCRATCH_PATH "\\Programs\\Scratch 3\\Scratch 3.exe"

bool should_do_ram_overload(void) {
    char out[512];
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, out))) {
		out[0] = 0;
		return false;
	}
    int len = strlen(out);
    if (len + strlen(SCRATCH_PATH) + 1 > 512) {
        out[0] = 0;
        return false;
    }

    strcat(out, SCRATCH_PATH);

    return PathFileExistsA(out);
}
#endif
