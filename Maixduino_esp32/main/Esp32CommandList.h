#pragma once

//Команды, для общения с сервером.
//После каждой команды идёт длинна ответа
//Если длинна ответа неизвестна, то её не пишем.
//У всех команд должны быть ответы. Длинна ответа кратна 4-м байтам.
//Команды начинаются с CESP_
//Их размеры начинаются с CESP_RESP_
enum ESP32_COMMANDS
{
    CESP_INVERT_BYTES = 0,
    CESP_SET_DEBUG = 1, CESP_RESP_SET_DEBUG = 4,
    CESP_GET_FW_VERSION = 2, CESP_RESP_FW_VERSION = 16,

    CESP_SET_SSID_AND_PASS = 0x10, CESP_RESP_SET_SSID_AND_PASS = 4,
    CESP_SCAN_NETWORKS = 0x12, CESP_RESP_SCAN_NETWORKS = 4,
    CESP_SCAN_NETWORKS_RESULT = 0x13,

    CESP_GET_CONN_STATUS = 0x20, CESP_RESP_GET_CONN_STATUS = 4,

    CESP_COMMAND_MAX = 3,
};