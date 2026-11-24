#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "model/terminal_process.h"

typedef struct OutputCollector {
	char buffer[2048];
	size_t len;
	CRITICAL_SECTION lock;
} OutputCollector;

static void on_test_output(const char *data, size_t len, void *user_data) {
	OutputCollector *collector = (OutputCollector*)user_data;
	if (!collector || !data || len == 0) return;
	EnterCriticalSection(&collector->lock);
	if (collector->len + len < sizeof(collector->buffer) - 1) {
		memcpy(collector->buffer + collector->len, data, len);
		collector->len += len;
		collector->buffer[collector->len] = '\0';
	}
	LeaveCriticalSection(&collector->lock);
}

static void test_invalid_arguments(void) {
	CairTerminalProcess proc;
	memset(&proc, 0, sizeof(proc));
	assert(!cair_terminal_process_start(NULL, NULL, NULL, 80, 24, on_test_output, NULL));
	assert(!cair_terminal_process_start(&proc, NULL, NULL, 80, 24, NULL, NULL));
	assert(cair_terminal_process_write(NULL, "a", 1) == -1);
	assert(!cair_terminal_process_resize(NULL, 80, 24));
	assert(!cair_terminal_process_is_running(NULL));
	cair_terminal_process_destroy(&proc);
	cair_terminal_process_destroy(NULL);
}

static void test_process_echo(void) {
	CairTerminalProcess proc;
	OutputCollector collector;
	int attempts = 0;

	memset(&proc, 0, sizeof(proc));
	memset(&collector, 0, sizeof(collector));
	InitializeCriticalSection(&collector.lock);
	SetEnvironmentVariableA("CAIR_TERMINAL_FORCE_PIPES", "1");

	assert(cair_terminal_process_start(&proc, NULL, NULL,
	    80, 24, on_test_output, &collector));
	{
		int waited = 0;
		int got_prompt;
		do {
			Sleep(50);
			waited += 50;
			EnterCriticalSection(&collector.lock);
			got_prompt = collector.len > 0;
			LeaveCriticalSection(&collector.lock);
		} while (!got_prompt && waited < 2000);
		assert(got_prompt);
	}
	{
		static const char command[] = "echo cair_term_proc_test\r\n";
		assert(cair_terminal_process_write(&proc, command, sizeof(command) - 1) == (int)(sizeof(command) - 1));
	}

	/* Wait up to 3 seconds for output to arrive */
	while (attempts++ < 30) {
		Sleep(100);
		EnterCriticalSection(&collector.lock);
		int found = strstr(collector.buffer, "cair_term_proc_test") != NULL;
		LeaveCriticalSection(&collector.lock);
		if (found) break;
	}

	EnterCriticalSection(&collector.lock);
	if (!strstr(collector.buffer, "cair_term_proc_test")) {
		fprintf(stderr, "Captured %zu bytes: [%s]\n", collector.len, collector.buffer);
	}
	assert(strstr(collector.buffer, "cair_term_proc_test") != NULL);
	LeaveCriticalSection(&collector.lock);

	assert(cair_terminal_process_resize(&proc, 100, 30));

	cair_terminal_process_destroy(&proc);
	assert(!cair_terminal_process_is_running(&proc));
	DeleteCriticalSection(&collector.lock);
	SetEnvironmentVariableA("CAIR_TERMINAL_FORCE_PIPES", NULL);
}

int main(void) {
	test_invalid_arguments();
	test_process_echo();
	printf("All terminal process unit tests passed!\n");
	return 0;
}
