#include <stdio.h>
#include <string.h>
#include "logger.h"

static int checks = 0;
static int failures = 0;

static void check(const char *name, int passed)
{
    ++checks;
    if (!passed) {
        ++failures;
    }
    printf("%s: %s\n", passed ? "PASS" : "FAIL", name);
}

int main(void)
{
    FILE *file = tmpfile();
    if (file == NULL) {
        fprintf(stderr, "Cannot create temporary test file.\n");
        return 1;
    }

    check("write overheat transition",
          logger_write(file, 1, "OVERHEAT", STATE_RUNNING, STATE_FAULT, 85));
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    char line[128];
    check("CSV records states and motor off",
          fgets(line, sizeof line, file) != NULL &&
          strcmp(line, "1,OVERHEAT,RUNNING,FAULT,85,0\r\n") == 0);
    check("reject event with comma",
          !logger_write(file, 2, "BAD,EVENT", STATE_IDLE, STATE_IDLE, 30));
    check("reject event with newline",
          !logger_write(file, 2, "BAD\nEVENT", STATE_IDLE, STATE_IDLE, 30));
    check("reject empty event",
          !logger_write(file, 2, "", STATE_IDLE, STATE_IDLE, 30));
    check("reject null event",
          !logger_write(file, 2, NULL, STATE_IDLE, STATE_IDLE, 30));
    check("reject invalid state",
          !logger_write(file, 2, "STATUS", (MachineState)99, STATE_IDLE, 30));
    check("reject zero sequence",
          !logger_write(file, 0, "STATUS", STATE_IDLE, STATE_IDLE, 30));
    check("reject missing file",
          !logger_write(NULL, 1, "STATUS", STATE_IDLE, STATE_IDLE, 30));
    check("reject missing path", logger_open(NULL) == NULL);
    check("reject empty path", logger_open("") == NULL);

    if (fclose(file) != 0) {
        return 1;
    }
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
