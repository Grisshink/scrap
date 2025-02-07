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
