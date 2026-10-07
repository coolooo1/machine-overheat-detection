#include <stdio.h>
#include "controller.h"
#include "logger.h"

/* 狀態與模擬馬達輸出都根據 state 判斷。 */
static void print_status(MachineState state, int temperature_c)
{
    printf("temperature=%dC state=", temperature_c);
    if (state == STATE_IDLE) {
        printf("IDLE");
    } else if (state == STATE_RUNNING) {
        printf("RUNNING");
    } else {
        printf("FAULT");
    }
    printf(" motor=%s\n", controller_motor_is_on(state) ? "ON" : "OFF");
}

int main(void)
{
    MachineState state = STATE_IDLE;
    int temperature_c = 30;
    char command;
    size_t sequence = 1;
    const char *exit_event = "INPUT_END";
    FILE *log_file = logger_open("events.csv");
    if (log_file == NULL) {
        fprintf(stderr, "Cannot open events.csv: check permissions and CSV format.\n");
        return 1;
    }
    if (!logger_write(log_file, sequence++, "SESSION_START", state, state,
                      temperature_c)) {
        fprintf(stderr, "Cannot write initial event; machine remains IDLE.\n");
        fclose(log_file);
        return 1;
    }

    puts("MachineGuard - simulated equipment controller");
    puts("Events are appended to events.csv in the current folder.");
    puts("1=start  2=overheat(85C)  3=cool(40C)");
    puts("4=reset  5=status  6=stop  0=quit");
    print_status(state, temperature_c);
    puts("Enter one command character, then press Enter:");

    /* %c 讀一個字元；前面的空白略過換行與空格。
       &command 是儲存位置；回傳 1 表示成功讀入一個值。
       多個非空白字元會依序視為多個指令。 */
    while (scanf(" %c", &command) == 1) {
        if (command == '0') { /* 結束：離開輸入迴圈，接著停止模擬運轉並結束程式。 */
            exit_event = "QUIT";
            break;
        }

        /* 先保存操作前的狀態，處理後再把前後狀態一起寫入 CSV。 */
        MachineState previous_state = state;
        const char *event = "UNKNOWN_COMMAND";

        switch (command) {
        case '1': /* 啟動：僅允許待機且溫度低於門檻的設備進入運轉。 */
            if (state == STATE_IDLE) {
                state = controller_start(state, temperature_c);
                if (state == STATE_RUNNING) {
                    event = "START";
                    puts("Started.");
                } else {
                    event = "START_REJECTED_HOT";
                    puts("Start rejected: temperature is too high.");
                }
            } else {
                event = "START_REJECTED_NOT_IDLE";
                puts("Start rejected: machine must be IDLE.");
            }
            break;

        case '2': /* 模擬過熱：將溫度設為 85°C，更新狀態，使設備進入故障。 */
            event = "OVERHEAT";
            temperature_c = 85;
            state = controller_update_temperature(state, temperature_c);
            puts("Injected simulated overheat.");
            break;

        case '3': /* 模擬降溫：將溫度設為 40°C；既有故障仍保持，需另外重置。 */
            event = "COOL";
            temperature_c = 40;
            state = controller_update_temperature(state, temperature_c);
            puts("Cooled; an existing fault remains latched.");
            break;

        case '4': /* 重置：僅在故障且溫度低於門檻時回到待機，不會自動啟動。 */
            if (state != STATE_FAULT) {
                event = "RESET_REJECTED_NO_FAULT";
                puts("Reset rejected: no fault to reset.");
            } else if (temperature_c >= OVERHEAT_THRESHOLD_C) {
                event = "RESET_REJECTED_HOT";
                puts("Reset rejected: temperature is still too high.");
            } else {
                event = "RESET";
                state = controller_reset(state, temperature_c);
                puts("Reset accepted: IDLE; start separately.");
            }
            break;

        case '5': /* 查詢狀態：不改變設備資料，由 switch 後的 print_status 顯示結果。 */
            event = "STATUS";
            /* 狀態會在 switch 結束後印出。 */
            break;

        case '6': /* 正常停止：運轉中回到待機；若已有故障，仍保持故障。 */
            event = "STOP";
            state = controller_stop(state);
            if (state == STATE_FAULT) {
                puts("Stopped; fault remains latched.");
            } else {
                puts("Stopped: IDLE.");
            }
            break;

        default: /* 未知指令：顯示提示，保持目前狀態與溫度。 */
            puts("Unknown command; state unchanged.");
            break;
        }

        if (!logger_write(log_file, sequence++, event, previous_state, state,
                          temperature_c)) {
            state = controller_stop(state);
            print_status(state, temperature_c);
            fprintf(stderr, "Event logging failed; simulated motor stopped.\n");
            fclose(log_file);
            return 1;
        }
        print_status(state, temperature_c);
        puts("Next command (1/2/3/4/5/6/0):");
    }

    MachineState previous_state = state;
    state = controller_stop(state);
    print_status(state, temperature_c);
    int exit_code = 0;
    if (!logger_write(log_file, sequence, exit_event, previous_state, state,
                      temperature_c)) {
        fprintf(stderr, "Cannot write final event.\n");
        exit_code = 1;
    }
    if (fclose(log_file) != 0) {
        fprintf(stderr, "Cannot close event log successfully.\n");
        exit_code = 1;
    }
    puts("Program ended; simulated motor OFF.");

    return exit_code;
}
