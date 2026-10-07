#include "controller.h"

/* 降溫不會自動清除故障。 */
MachineState controller_update_temperature(MachineState state, int temperature_c)
{
    if (temperature_c >= OVERHEAT_THRESHOLD_C) {
        return STATE_FAULT;
    }
    return state;
}

MachineState controller_start(MachineState state, int temperature_c)
{
    if (state != STATE_IDLE) {
        return state;
    }
    if (temperature_c >= OVERHEAT_THRESHOLD_C) {
        return STATE_FAULT;
    }
    return STATE_RUNNING;
}

/* 正常停止回到待機；故障中的停止不能清除故障。 */
MachineState controller_stop(MachineState state)
{
    if (state == STATE_RUNNING) {
        return STATE_IDLE;
    }
    return state;
}

MachineState controller_reset(MachineState state, int temperature_c)
{
    if (state == STATE_FAULT && temperature_c < OVERHEAT_THRESHOLD_C) {
        return STATE_IDLE;
    }
    return state;
}

int controller_motor_is_on(MachineState state)
{
    return state == STATE_RUNNING;
}
