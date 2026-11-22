#include <string.h>

#include "src/app.h"

#include <string.h>

int main(int argc, char** argv) {
	int self_test = argc > 1 && strcmp(argv[1], "--self-test") == 0;
	return cair_app_run(self_test);
}
