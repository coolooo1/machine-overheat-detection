#include <stdlib.h>
#include "logger.h"

/* 僅用於整合測試：指定第幾筆寫入失敗，不連結到正式程式。 */
FILE *logger_open(const char *path)
{
    (void)path;
    return tmpfile();
}

int logger_write(FILE *file, size_t sequence, const char *event,
                 MachineState previous_state, MachineState current_state,
                 int temperature_c)
{
    (void)file;
    (void)event;
    (void)previous_state;
    (void)current_state;
    (void)temperature_c;
    const char *value = getenv("MACHINEGUARD_TEST_FAIL_AT");
    size_t fail_at = value == NULL ? 3 : (size_t)strtoul(value, NULL, 10);
    return sequence != fail_at;
}
