#ifndef MACHINEGUARD_CONTROLLER_H
#define MACHINEGUARD_CONTROLLER_H

/* 模擬用門檻，不代表真實設備規格。 */
#define OVERHEAT_THRESHOLD_C 80

typedef enum {
    STATE_IDLE,
    STATE_RUNNING,
    STATE_FAULT
} MachineState;

/* 函式宣告：告訴其他 .c 檔案如何使用控制模組。 */
MachineState controller_update_temperature(MachineState state, int temperature_c);
MachineState controller_start(MachineState state, int temperature_c);
MachineState controller_stop(MachineState state);
MachineState controller_reset(MachineState state, int temperature_c);
int controller_motor_is_on(MachineState state);

#endif
