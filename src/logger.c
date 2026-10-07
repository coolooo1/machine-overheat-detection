#include <string.h>
#include "logger.h"

static const char CSV_HEADER[] =
    "sequence,event,previous_state,current_state,temperature_c,motor_on";

static const char *state_name(MachineState state)
{
    switch (state) {
    case STATE_IDLE:    return "IDLE";
    case STATE_RUNNING: return "RUNNING";
    case STATE_FAULT:   return "FAULT";
    default:           return NULL;
    }
}

FILE *logger_open(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    /* a+b：追加，不覆蓋；二進位模式讓換行與位置檢查一致。 */
    FILE *file = fopen(path, "a+b");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        return NULL;
    }

    if (length == 0) {
        if (fprintf(file, "%s\r\n", CSV_HEADER) < 0 || fflush(file) != 0) {
            fclose(file);
            return NULL;
        }
    } else {
        char header[128];
        if (fseek(file, 0, SEEK_SET) != 0 ||
            fgets(header, sizeof header, file) == NULL) {
            fclose(file);
            return NULL;
        }
        header[strcspn(header, "\r\n")] = '\0';
        if (strcmp(header, CSV_HEADER) != 0) {
            fclose(file);
            return NULL;
        }

        /* 舊紀錄必須以換行結束，避免新事件接在未完成的一行。 */
        if (fseek(file, -1, SEEK_END) != 0 || fgetc(file) != '\n') {
            fclose(file);
            return NULL;
        }
        if (fseek(file, 0, SEEK_END) != 0) {
            fclose(file);
            return NULL;
        }
    }

    return file;
}

int logger_write(FILE *file, size_t sequence, const char *event,
                 MachineState previous_state, MachineState current_state,
                 int temperature_c)
{
    if (file == NULL || sequence == 0 || event == NULL || event[0] == '\0') {
        return 0;
    }
    /* 限定事件名稱，避免逗號與換行破壞 CSV 欄位。 */
    for (const char *p = event; *p != '\0'; ++p) {
        if (!((*p >= 'A' && *p <= 'Z') || *p == '_')) {
            return 0;
        }
    }
    const char *previous_name = state_name(previous_state);
    const char *current_name = state_name(current_state);
    if (previous_name == NULL || current_name == NULL) {
        return 0;
    }

    if (fprintf(file, "%zu,%s,%s,%s,%d,%d\r\n", sequence, event,
                previous_name, current_name, temperature_c,
                controller_motor_is_on(current_state)) < 0) {
        return 0;
    }
    return fflush(file) == 0;
}
