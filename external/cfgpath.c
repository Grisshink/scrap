#include "cfgpath.h"

#if defined(_MSC_VER) || defined(__MINGW32__)
    #include <direct.h>
    #define mkdir _mkdir
#endif

#ifdef __unix__
#include <sys/param.h>
#endif

#if defined(CFGPATH_LINUX)
    #include <string.h>
    #include <stdlib.h>
    #include <sys/stat.h>
#elif defined(CFGPATH_WINDOWS)
    #include <shlobj.h>
#elif defined(CFGPATH_MAC)
    #include <CoreServices/CoreServices.h>
    #include <sys/stat.h>
#endif

void get_user_config_file(char *out, unsigned int maxlen, const char *appname)
{
#ifdef CFGPATH_LINUX
	const char *out_orig = out;
	char *home = getenv("XDG_CONFIG_HOME");
	unsigned int config_len = 0;
	if (!home) {
		home = getenv("HOME");
		if (!home) {
			// Can't find home directory
			out[0] = 0;
			return;
		}
		config_len = strlen(".config/");
	}

	unsigned int home_len = strlen(home);
	unsigned int appname_len = strlen(appname);
	const int ext_len = strlen(".conf");

	/* first +1 is "/", second is terminating null */
	if (home_len + 1 + config_len + appname_len + ext_len + 1 > maxlen) {
		out[0] = 0;
		return;
	}

	memcpy(out, home, home_len);
	out += home_len;
	*out = '/';
	out++;
	if (config_len) {
		memcpy(out, ".config/", config_len);
		out += config_len;
		/* Make the .config folder if it doesn't already exist */
		*out = '\0';
		mkdir(out_orig, 0755);
	}
	memcpy(out, appname, appname_len);
	out += appname_len;
	memcpy(out, ".conf", ext_len);
	out += ext_len;
	*out = '\0';
#elif defined(CFGPATH_WINDOWS)
	if (maxlen < MAX_PATH) {
		out[0] = 0;
		return;
	}
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, out))) {
		out[0] = 0;
		return;
	}
	/* We don't try to create the AppData folder as it always exists already */
	unsigned int appname_len = strlen(appname);
	if (strlen(out) + 1 + appname_len + strlen(".ini") + 1 > maxlen) {
		out[0] = 0;
		return;
	}
	strcat(out, "\\");
	strcat(out, appname);
	strcat(out, ".ini");
#elif defined(CFGPATH_MAC)
	FSRef ref;
	FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &ref);
	char home[MAX_PATH];
	FSRefMakePath(&ref, (UInt8 *)&home, MAX_PATH);
	/* first +1 is "/", second is terminating null */
	const char *ext = ".conf";
	if (strlen(home) + 1 + strlen(appname) + strlen(ext) + 1 > maxlen) {
		out[0] = 0;
		return;
	}

	strcpy(out, home);
	strcat(out, PATH_SEPARATOR_STRING);
	strcat(out, appname);
	strcat(out, ext);
#endif
}

void get_user_config_folder(char *out, unsigned int maxlen, const char *appname)
{
#ifdef CFGPATH_LINUX
	const char *out_orig = out;
	char *home = getenv("XDG_CONFIG_HOME");
	unsigned int config_len = 0;
	if (!home) {
		home = getenv("HOME");
		if (!home) {
			// Can't find home directory
			out[0] = 0;
			return;
		}
		config_len = strlen(".config/");
	}

	unsigned int home_len = strlen(home);
	unsigned int appname_len = strlen(appname);

	/* first +1 is "/", second is trailing "/", third is terminating null */
	if (home_len + 1 + config_len + appname_len + 1 + 1 > maxlen) {
		out[0] = 0;
		return;
	}

	memcpy(out, home, home_len);
	out += home_len;
	*out = '/';
	out++;
	if (config_len) {
		memcpy(out, ".config/", config_len);
		out += config_len;
		/* Make the .config folder if it doesn't already exist */
		*out = '\0';
		mkdir(out_orig, 0755);
	}
	memcpy(out, appname, appname_len);
	out += appname_len;
	/* Make the .config/appname folder if it doesn't already exist */
	*out = '\0';
	mkdir(out_orig, 0755);
	*out = '/';
	out++;
	*out = '\0';
#elif defined(CFGPATH_WINDOWS)
	if (maxlen < MAX_PATH) {
		out[0] = 0;
		return;
	}
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, out))) {
		out[0] = 0;
		return;
	}
	/* We don't try to create the AppData folder as it always exists already */
	unsigned int appname_len = strlen(appname);
	if (strlen(out) + 1 + appname_len + 1 + 1 > maxlen) {
		out[0] = 0;
		return;
	}
	strcat(out, "\\");
	strcat(out, appname);
	/* Make the AppData\appname folder if it doesn't already exist */
	mkdir(out);
	strcat(out, "\\");
#elif defined(CFGPATH_MAC)
	FSRef ref;
	FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &ref);
	char home[MAX_PATH];
	FSRefMakePath(&ref, (UInt8 *)&home, MAX_PATH);
	/* first +1 is "/", second is trailing "/", third is terminating null */
	if (strlen(home) + 1 + strlen(appname) + 1 + 1 > maxlen) {
		out[0] = 0;
		return;
	}

	strcpy(out, home);
	strcat(out, PATH_SEPARATOR_STRING);
	strcat(out, appname);
	/* Make the .config/appname folder if it doesn't already exist */
	mkdir(out, 0755);
	strcat(out, PATH_SEPARATOR_STRING);
#endif
}

void get_user_data_folder(char *out, unsigned int maxlen, const char *appname)
{
#ifdef CFGPATH_LINUX
	const char *out_orig = out;
	char *home = getenv("XDG_DATA_HOME");
	unsigned int config_len = 0;
	if (!home) {
		home = getenv("HOME");
		if (!home) {
			// Can't find home directory
			out[0] = 0;
			return;
		}
		config_len = strlen(".local/share/");
	}

	unsigned int home_len = strlen(home);
	unsigned int appname_len = strlen(appname);

	/* first +1 is "/", second is trailing "/", third is terminating null */
	if (home_len + 1 + config_len + appname_len + 1 + 1 > maxlen) {
		out[0] = 0;
		return;
	}

	memcpy(out, home, home_len);
	out += home_len;
	*out = '/';
	out++;
	if (config_len) {
		memcpy(out, ".local/share/", config_len);
		out += config_len;
		/* Make the .local/share folder if it doesn't already exist */
		*out = '\0';
		mkdir(out_orig, 0755);
	}
	memcpy(out, appname, appname_len);
	out += appname_len;
	/* Make the .local/share/appname folder if it doesn't already exist */
	*out = '\0';
	mkdir(out_orig, 0755);
	*out = '/';
	out++;
	*out = '\0';
#elif defined(CFGPATH_WINDOWS) || defined(CFGPATH_MAC)
	/* No distinction under Windows or OS X */
	get_user_config_folder(out, maxlen, appname);
#endif
}

void get_user_cache_folder(char *out, unsigned int maxlen, const char *appname)
{
#ifdef CFGPATH_LINUX
	const char *out_orig = out;
	char *home = getenv("XDG_CACHE_HOME");
	unsigned int config_len = 0;
	if (!home) {
		home = getenv("HOME");
		if (!home) {
			// Can't find home directory
			out[0] = 0;
			return;
		}
		config_len = strlen(".cache/");
	}

	unsigned int home_len = strlen(home);
	unsigned int appname_len = strlen(appname);

	/* first +1 is "/", second is trailing "/", third is terminating null */
	if (home_len + 1 + config_len + appname_len + 1 + 1 > maxlen) {
		out[0] = 0;
		return;
	}

	memcpy(out, home, home_len);
	out += home_len;
	*out = '/';
	out++;
	if (config_len) {
		memcpy(out, ".cache/", config_len);
		out += config_len;
		/* Make the .cache folder if it doesn't already exist */
		*out = '\0';
		mkdir(out_orig, 0755);
	}
	memcpy(out, appname, appname_len);
	out += appname_len;
	/* Make the .cache/appname folder if it doesn't already exist */
	*out = '\0';
	mkdir(out_orig, 0755);
	*out = '/';
	out++;
	*out = '\0';
#elif defined(CFGPATH_WINDOWS)
	if (maxlen < MAX_PATH) {
		out[0] = 0;
		return;
	}
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, out))) {
		out[0] = 0;
		return;
	}
	/* We don't try to create the AppData folder as it always exists already */
	unsigned int appname_len = strlen(appname);
	if (strlen(out) + 1 + appname_len + 1 + 1 > maxlen) {
		out[0] = 0;
		return;
	}
	strcat(out, "\\");
	strcat(out, appname);
	/* Make the AppData\appname folder if it doesn't already exist */
	mkdir(out);
	strcat(out, "\\");
#elif defined(CFGPATH_MAC)
	/* No distinction under OS X */
	get_user_config_folder(out, maxlen, appname);
#endif
}
