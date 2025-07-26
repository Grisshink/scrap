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

#include "term.h"

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
    HANDLE read_pipe, write_pipe;
    STARTUPINFO start_info = {0};
    PROCESS_INFORMATION proc_info = {0};

    SECURITY_ATTRIBUTES pipe_attrs = {0};
    pipe_attrs.nLength = sizeof(pipe_attrs);
    pipe_attrs.bInheritHandle = TRUE;

    if (!CreatePipe(&read_pipe, &write_pipe, &pipe_attrs, 0)) {
        snprintf(error, error_len, "[LLVM] Failed to create a pipe. Error code: %ld", GetLastError());
        return false;
    }

    start_info.cb = sizeof(start_info);
    start_info.hStdError = write_pipe;
    start_info.hStdOutput = write_pipe;
    start_info.dwFlags = STARTF_USESTDHANDLES;

    char command_line[512];
    size_t i = 0;
    for (char** arg = args; *arg; arg++) {
        command_line[i++] = '"';
        for (char* str = *arg; *str && i < 510; str++) command_line[i++] = *str;
        command_line[i++] = '"';
        command_line[i++] = ' ';
    }
    command_line[i - 1] = 0;

    if(!CreateProcessA(
        NULL, command_line,
        NULL, NULL, // No security attributes
        TRUE, // Allow to inherit handles
        CREATE_NO_WINDOW, // Don't spawn cmd for a process
        NULL, NULL, // Just give me a process
        &start_info, // Give STARTUPINFO
        &proc_info) // Get PROCESS_INFORMATION
    ) {
        snprintf(error, error_len, "[LLVM] Failed to create a process. Error code: %ld", GetLastError());
        CloseHandle(write_pipe);
        CloseHandle(read_pipe);
        return false;
    }

    CloseHandle(write_pipe);

    long size = 0;
    char buf[1024];

    for (;;) {
        if (!ReadFile(read_pipe, buf, 1024 - 1 /* Save space for null terminator */, &size, NULL)) {
            long last_error = GetLastError();
            if (last_error == ERROR_BROKEN_PIPE) break;

            snprintf(error, error_len, "[LLVM] Failed to read from pipe. Error code: %ld", last_error);
            CloseHandle(proc_info.hProcess);
            CloseHandle(proc_info.hThread);
            return false;
        }
        buf[size] = 0;
        term_print_str(buf);
    }

    WaitForSingleObject(proc_info.hProcess, INFINITE);

    long exit_code;
    if (!GetExitCodeProcess(proc_info.hProcess, &exit_code)) {
        snprintf(error, error_len, "[LLVM] Failed to get exit code. Error code: %ld", GetLastError());
        CloseHandle(proc_info.hProcess);
        CloseHandle(proc_info.hThread);
        CloseHandle(read_pipe);
        return false;
    }

    if (exit_code) {
        snprintf(error, error_len, "Linker exited with exit code: %ld", exit_code);
        CloseHandle(proc_info.hProcess);
        CloseHandle(proc_info.hThread);
        CloseHandle(read_pipe);
        return false;
    }

    CloseHandle(proc_info.hProcess);
    CloseHandle(proc_info.hThread);
    CloseHandle(read_pipe);
#else
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        snprintf(error, error_len, "[LLVM] Failed to create a pipe: %s", strerror(errno));
        return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
        snprintf(error, error_len, "[LLVM] Failed to fork a process: %s", strerror(errno));
        return false;
    }

    if (pid == 0) {
        // We are newborn

        // Close duplicate read end
        if (close(pipefd[0]) == -1) {
            perror("close");
            exit(1);
        }
        
        // Replace stdout and stderr with pipe
        if (dup2(pipefd[1], 1) == -1) {
            perror("dup2");
            exit(1);
        }

        if (dup2(pipefd[1], 2) == -1) {
            perror("dup2");
            exit(1);
        }

        // Replace da child with linker
        if (execvp(name, args) == -1) {
            perror("execvp");
            exit(1);
        }
    } else {
        // We are parent

        // Close duplicate write end
        if (close(pipefd[1]) == -1) {
            perror("close");
            exit(1);
        }

        ssize_t size = 0;
        char buf[1024];
        while ((size = read(pipefd[0], buf, 1024 - 1 /* Save space for null terminator */))) {
            if (size == -1) {
                perror("read");
                exit(1);
            }
            buf[size] = 0;   
            term_print_str(buf);
        }

        if (close(pipefd[0]) == -1) {
            perror("close");
            exit(1);
        }

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
