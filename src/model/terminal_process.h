#ifndef CAIR_TERMINAL_PROCESS_H
#define CAIR_TERMINAL_PROCESS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*CairTerminalOutputCallback)(const char *data, size_t len, void *user_data);

typedef struct CairTerminalProcess {
	void *process_handle;       /* HANDLE hProcess */
	void *thread_handle;        /* HANDLE hThread */
	void *in_write_handle;      /* HANDLE hInWrite (stdin pipe write) */
	void *out_read_handle;      /* HANDLE hOutRead (stdout pipe read) */
	void *reader_thread;        /* HANDLE hReaderThread */
	void *hpc;                  /* HPCON pseudo-console handle */
	CairTerminalOutputCallback on_output;
	void *user_data;
	int running;
} CairTerminalProcess;

/**
 * Start a shell process in the background.
 * If command is NULL, defaults to cmd.exe or powershell.exe.
 * working_dir can be NULL for current directory.
 * Returns 1 on success, 0 on failure.
 */
int cair_terminal_process_start(CairTerminalProcess *proc,
                                const char *command,
                                const char *working_dir,
                                uint16_t cols,
                                uint16_t rows,
                                CairTerminalOutputCallback on_output,
                                void *user_data);

/**
 * Terminate the process and clean up all resources.
 */
void cair_terminal_process_destroy(CairTerminalProcess *proc);

/**
 * Write bytes to the process stdin.
 * Returns bytes written, or -1 on error.
 */
int cair_terminal_process_write(CairTerminalProcess *proc, const char *data, size_t len);

/**
 * Resize the pseudo console window.
 * Returns 1 on success, 0 on failure.
 */
int cair_terminal_process_resize(CairTerminalProcess *proc, uint16_t cols, uint16_t rows);

/**
 * Check if the child process is still running.
 * Returns 1 if running, 0 if terminated.
 */
int cair_terminal_process_is_running(CairTerminalProcess *proc);

#ifdef __cplusplus
}
#endif

#endif /* CAIR_TERMINAL_PROCESS_H */
