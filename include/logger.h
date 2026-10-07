#ifndef MACHINEGUARD_LOGGER_H
#define MACHINEGUARD_LOGGER_H

#include <stdio.h>
#include "controller.h"

/* 追加紀錄；新檔案自動建立欄位名稱，已有檔案則檢查格式。 */
FILE *logger_open(const char *path);

/* 每次執行從 sequence=1 開始。event 僅允許英文大寫與底線。
   成功回傳 1；失敗回傳 0。只記錄資料，不改變設備狀態。 */
int logger_write(FILE *file, size_t sequence, const char *event,
                 MachineState previous_state, MachineState current_state,
                 int temperature_c);

#endif
