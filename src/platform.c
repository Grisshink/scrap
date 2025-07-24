#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

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
#endif // defined(RAM_OVERLOAD) && defined(_WIN32)

#ifndef USE_INTERPRETER
bool spawn_process(const char* name, char* args[], char* error, size_t error_len) {
#ifdef _WIN32
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    char command_line[512];
    size_t i = 0;
    for (char** arg = args; *arg; arg++) {
        command_line[i++] = '"';
        for (char* str = *arg; *str && i < 510; str++) command_line[i++] = *str;
        command_line[i++] = '"';
        command_line[i++] = ' ';
    }
    command_line[i - 1] = 0;
    printf("Command line: %s\n", command_line);

    // Start the child process. 
    if(!CreateProcessA(NULL,   // No module name (use command line)
        command_line,          // Command line
        NULL,                  // Process handle not inheritable
        NULL,                  // Thread handle not inheritable
        FALSE,                 // Set handle inheritance to FALSE
        0,                     // No creation flags
        NULL,                  // Use parent's environment block
        NULL,                  // Use parent's starting directory 
        &si,                   // Pointer to STARTUPINFO structure
        &pi)                   // Pointer to PROCESS_INFORMATION structure
    ) {
        //snprintf(error, error_len, "[LLVM] Failed to create a process. Error code: %ld", GetLastError());
        snprintf(error, error_len, "[LLVM] Failed to create a process.");
        return false;
    }

    printf("Created new process\n");

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    long exit_code;

    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
        snprintf(error, error_len, "[LLVM] Failed to get exit code. Error code: %ld", GetLastError());
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }

    if (exit_code) {
        snprintf(error, error_len, "Linker exited with exit code: %ld", exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    pid_t pid = fork();
    if (pid == -1) {
        snprintf(error, error_len, "[LLVM] Failed to fork a process: %s", strerror(errno));
        return false;
    }

    if (pid == 0) {
        // We are newborn
        // Replace da child with linker
        if (execvp(name, args) == -1) {
            perror("execvp");
            exit(1);
        }
    } else {
        // We are parent
        // Wait for child to terminate
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code == 0) {
                return true;
            } else {
                snprintf(error, error_len, "Linker exited with exit code: %d", exit_code);
                return false;
            }
        } else if (WIFSIGNALED(status)) {
            snprintf(error, error_len, "Linker signaled with signal number: %d", WTERMSIG(status));
            return false;
        } else {
            snprintf(error, error_len, "Received unknown child status :/");
            return false;
        }
    }
#endif

    return true;
}
#endif // USE_INTERPRETER
