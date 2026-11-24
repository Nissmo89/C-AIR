#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "terminal_process.h"

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

typedef HRESULT (WINAPI *pfnCreatePseudoConsole)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
typedef HRESULT (WINAPI *pfnResizePseudoConsole)(HPCON, COORD);
typedef void    (WINAPI *pfnClosePseudoConsole)(HPCON);

static pfnCreatePseudoConsole s_CreatePseudoConsole = NULL;
static pfnResizePseudoConsole s_ResizePseudoConsole = NULL;
static pfnClosePseudoConsole  s_ClosePseudoConsole = NULL;
static int s_conpty_checked = 0;

static void init_conpty_functions(void) {
	if (s_conpty_checked) return;
	s_conpty_checked = 1;
	HMODULE k32 = GetModuleHandleA("kernel32.dll");
	if (k32) {
		FARPROC symbol = GetProcAddress(k32, "CreatePseudoConsole");
		if (sizeof(symbol) == sizeof(s_CreatePseudoConsole)) memcpy(&s_CreatePseudoConsole, &symbol, sizeof(symbol));
		symbol = GetProcAddress(k32, "ResizePseudoConsole");
		if (sizeof(symbol) == sizeof(s_ResizePseudoConsole)) memcpy(&s_ResizePseudoConsole, &symbol, sizeof(symbol));
		symbol = GetProcAddress(k32, "ClosePseudoConsole");
		if (sizeof(symbol) == sizeof(s_ClosePseudoConsole)) memcpy(&s_ClosePseudoConsole, &symbol, sizeof(symbol));
	}
}

static DWORD WINAPI cair_terminal_reader_thread(LPVOID param) {
	CairTerminalProcess *proc = (CairTerminalProcess*)param;
	char buffer[4096];
	DWORD bytes_read = 0;

	while (proc && proc->running && proc->out_read_handle) {
		if (!ReadFile((HANDLE)proc->out_read_handle, buffer, sizeof(buffer), &bytes_read, NULL) || bytes_read == 0) {
			break;
		}
		if (proc->on_output && bytes_read > 0) {
			proc->on_output(buffer, (size_t)bytes_read, proc->user_data);
		}
	}

	if (proc) proc->running = 0;
	return 0;
}

int cair_terminal_process_start(CairTerminalProcess *proc,
                                const char *command,
                                const char *working_dir,
                                uint16_t cols,
                                uint16_t rows,
                                CairTerminalOutputCallback on_output,
                                void *user_data)
{
	char cmdline_buf[1024];
	HANDLE in_read = INVALID_HANDLE_VALUE;
	HANDLE in_write = INVALID_HANDLE_VALUE;
	HANDLE out_read = INVALID_HANDLE_VALUE;
	HANDLE out_write = INVALID_HANDLE_VALUE;
	HPCON hpc = NULL;
	PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES sa;

	if (!proc || !on_output) return 0;
	memset(proc, 0, sizeof(*proc));
	memset(&pi, 0, sizeof(pi));

	init_conpty_functions();

	if (!command || !command[0]) {
		const char *comspec = getenv("COMSPEC");
		if (!comspec) comspec = "cmd.exe";
		strncpy(cmdline_buf, comspec, sizeof(cmdline_buf) - 1);
		cmdline_buf[sizeof(cmdline_buf) - 1] = '\0';
	} else {
		strncpy(cmdline_buf, command, sizeof(cmdline_buf) - 1);
		cmdline_buf[sizeof(cmdline_buf) - 1] = '\0';
	}

	memset(&sa, 0, sizeof(sa));
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	if (!CreatePipe(&in_read, &in_write, &sa, 0)) return 0;
	if (!CreatePipe(&out_read, &out_write, &sa, 0)) {
		CloseHandle(in_read);
		CloseHandle(in_write);
		return 0;
	}

	/* Try ConPTY if supported */
	/* Tests and constrained hosts can select the portable redirected-pipe path. */
	{
		char _fp[4] = {0};
		int force_pipes = GetEnvironmentVariableA("CAIR_TERMINAL_FORCE_PIPES", _fp, sizeof(_fp)) > 0;
		if (!force_pipes &&
		    s_CreatePseudoConsole && s_ResizePseudoConsole && s_ClosePseudoConsole) {
			COORD size;
			size.X = (SHORT)(cols > 0 ? cols : 80);
			size.Y = (SHORT)(rows > 0 ? rows : 24);

			if (SUCCEEDED(s_CreatePseudoConsole(size, in_read, out_write, 0, &hpc))) {
				STARTUPINFOEXA siEx;
				SIZE_T bytesRequired = 0;
				memset(&siEx, 0, sizeof(siEx));
				siEx.StartupInfo.cb = sizeof(siEx);

				InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);
				siEx.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)malloc(bytesRequired);
				if (siEx.lpAttributeList &&
				    InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &bytesRequired)) {
					BOOL attr_ok = UpdateProcThreadAttribute(siEx.lpAttributeList, 0,
						PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof(HPCON), NULL, NULL);
					/* STARTUPINFOEX requires cb to include the attribute-list field. */
					siEx.StartupInfo.cb = sizeof(siEx);
					BOOL created = attr_ok && CreateProcessA(NULL, cmdline_buf, NULL, NULL, FALSE,
						EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, NULL, working_dir, &siEx.StartupInfo, &pi);

					DeleteProcThreadAttributeList(siEx.lpAttributeList);
					free(siEx.lpAttributeList);

					if (created) {
						CloseHandle(in_read);
						CloseHandle(out_write);
						in_read = INVALID_HANDLE_VALUE;
						out_write = INVALID_HANDLE_VALUE;
						proc->process_handle = (void*)pi.hProcess;
						proc->thread_handle = (void*)pi.hThread;
						proc->in_write_handle = (void*)in_write;
						proc->out_read_handle = (void*)out_read;
						proc->hpc = (void*)hpc;
						proc->on_output = on_output;
						proc->user_data = user_data;
						proc->running = 1;

						proc->reader_thread = (void*)CreateThread(NULL, 0,
							cair_terminal_reader_thread, proc, 0, NULL);
						if (proc->reader_thread) return 1;
						proc->running = 0;
						CloseHandle((HANDLE)proc->in_write_handle);
						CloseHandle((HANDLE)proc->out_read_handle);
						s_ClosePseudoConsole((HPCON)proc->hpc);
						WaitForSingleObject((HANDLE)proc->process_handle, 500);
						TerminateProcess((HANDLE)proc->process_handle, 1);
						CloseHandle((HANDLE)proc->process_handle);
						CloseHandle((HANDLE)proc->thread_handle);
						memset(proc, 0, sizeof(*proc));
						return 0;
					}
				} else {
					if (siEx.lpAttributeList) free(siEx.lpAttributeList);
				}

				s_ClosePseudoConsole(hpc);
				hpc = NULL;
				/* Preserve the redirected-pipe launch as a compatibility fallback. */
			}
		}
	}

	/* Fallback: standard pipe redirection */
	SetHandleInformation(in_write, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOA si;
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = in_read;
	si.hStdOutput = out_write;
	si.hStdError = out_write;

	BOOL created = CreateProcessA(NULL, cmdline_buf, NULL, NULL, TRUE,
		CREATE_NO_WINDOW, NULL, working_dir, &si, &pi);

	CloseHandle(in_read);
	CloseHandle(out_write);

	if (!created) {
		CloseHandle(in_write);
		CloseHandle(out_read);
		return 0;
	}

	proc->process_handle = (void*)pi.hProcess;
	proc->thread_handle = (void*)pi.hThread;
	proc->in_write_handle = (void*)in_write;
	proc->out_read_handle = (void*)out_read;
	proc->hpc = NULL;
	proc->on_output = on_output;
	proc->user_data = user_data;
	proc->running = 1;

	proc->reader_thread = (void*)CreateThread(NULL, 0,
		cair_terminal_reader_thread, proc, 0, NULL);
	if (proc->reader_thread) return 1;
	proc->running = 0;
	CloseHandle(in_write);
	CloseHandle(out_read);
	TerminateProcess(pi.hProcess, 1);
	WaitForSingleObject(pi.hProcess, 500);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	memset(proc, 0, sizeof(*proc));
	return 0;
}

void cair_terminal_process_destroy(CairTerminalProcess *proc) {
	if (!proc) return;
	proc->running = 0;

	/* Stop the interactive client first; closing ConPTY can wait for it to exit. */
	if (proc->process_handle) {
		if (WaitForSingleObject((HANDLE)proc->process_handle, 100) == WAIT_TIMEOUT) {
			TerminateProcess((HANDLE)proc->process_handle, 0);
			WaitForSingleObject((HANDLE)proc->process_handle, 500);
		}
	}

	if (proc->in_write_handle && proc->in_write_handle != INVALID_HANDLE_VALUE) {
		CloseHandle((HANDLE)proc->in_write_handle);
		proc->in_write_handle = INVALID_HANDLE_VALUE;
	}

	if (proc->hpc && s_ClosePseudoConsole) {
		s_ClosePseudoConsole((HPCON)proc->hpc);
		proc->hpc = NULL;
	}

	if (proc->out_read_handle && proc->out_read_handle != INVALID_HANDLE_VALUE) {
		CloseHandle((HANDLE)proc->out_read_handle);
		proc->out_read_handle = INVALID_HANDLE_VALUE;
	}

	if (proc->reader_thread) {
		WaitForSingleObject((HANDLE)proc->reader_thread, 500);
		CloseHandle((HANDLE)proc->reader_thread);
		proc->reader_thread = NULL;
	}

	if (proc->process_handle) {
		CloseHandle((HANDLE)proc->process_handle);
		proc->process_handle = NULL;
	}

	if (proc->thread_handle) {
		CloseHandle((HANDLE)proc->thread_handle);
		proc->thread_handle = NULL;
	}

	proc->on_output = NULL;
	proc->user_data = NULL;
}

int cair_terminal_process_write(CairTerminalProcess *proc, const char *data, size_t len) {
	DWORD written = 0;
	if (!proc || !proc->running || !proc->in_write_handle || !data || len == 0) {
		return -1;
	}
	if (len > (size_t)INT_MAX || len > (size_t)UINT32_MAX) return -1;
	if (!WriteFile((HANDLE)proc->in_write_handle, data, (DWORD)len, &written, NULL)) {
		return -1;
	}
	return (int)written;
}

int cair_terminal_process_resize(CairTerminalProcess *proc, uint16_t cols, uint16_t rows) {
	if (!proc || !proc->running || cols == 0 || rows == 0) return 0;
	if (proc->hpc && s_ResizePseudoConsole) {
		COORD size;
		size.X = (SHORT)cols;
		size.Y = (SHORT)rows;
		return SUCCEEDED(s_ResizePseudoConsole((HPCON)proc->hpc, size)) ? 1 : 0;
	}
	return 1;
}

int cair_terminal_process_is_running(CairTerminalProcess *proc) {
	if (!proc || !proc->running || !proc->process_handle) return 0;
	if (WaitForSingleObject((HANDLE)proc->process_handle, 0) == WAIT_TIMEOUT) {
		return 1;
	}
	proc->running = 0;
	return 0;
}
