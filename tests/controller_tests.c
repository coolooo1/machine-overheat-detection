#include <stdio.h>
#include "controller.h"

static int checks = 0;
static int failures = 0;

/* 每次檢查都輸出結果；失敗時最終回傳非 0。 */
static void check(const char *name, int passed)
{
    ++checks;
    if (passed) {
        printf("PASS: %s\n", name);
    } else {
        ++failures;
        printf("FAIL: %s\n", name);
    }
}

int main(void)
{
    MachineState state = STATE_IDLE;
    check("idle motor off", !controller_motor_is_on(state));

    state = controller_start(state, 30);
    check("normal start", state == STATE_RUNNING);
    check("running motor on", controller_motor_is_on(state));
    check("reset cannot stop normal running",
          controller_reset(state, 30) == STATE_RUNNING);

    state = controller_stop(state);
    check("normal stop returns idle", state == STATE_IDLE);
    check("stopped motor off", !controller_motor_is_on(state));
    check("repeated stop stays idle", controller_stop(state) == STATE_IDLE);

    state = controller_start(state, 79);
    state = controller_update_temperature(state, 79);
    check("79C remains running", state == STATE_RUNNING);

    state = controller_update_temperature(state, 80);
    check("80C faults at exact threshold", state == STATE_FAULT);
    check("fault motor off", !controller_motor_is_on(state));
    check("hot reset rejected", controller_reset(state, 80) == STATE_FAULT);

    state = controller_stop(state);
    check("stop cannot clear fault", state == STATE_FAULT);
    state = controller_update_temperature(state, 40);
    check("cooling cannot clear fault", state == STATE_FAULT);
    state = controller_start(state, 40);
    check("start cannot clear fault", state == STATE_FAULT);

    state = controller_reset(state, 40);
    check("cooled reset returns idle", state == STATE_IDLE);
    check("reset does not turn motor on", !controller_motor_is_on(state));
    state = controller_start(state, 40);
    check("explicit restart succeeds", state == STATE_RUNNING);

    state = controller_start(STATE_IDLE, 80);
    check("start refuses an already hot machine", state == STATE_FAULT);
    state = controller_update_temperature(STATE_IDLE, 85);
    check("idle machine also detects overheat", state == STATE_FAULT);

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
