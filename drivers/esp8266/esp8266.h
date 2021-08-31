#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
    uint32_t capacity;
    uint32_t size;
    uint8_t* data;
    uint8_t link_id;
} WifiReceiveBuffer;

//Преобразует нечитаемые символы в \r\n\xFE
//В конце гарантированно есть 0, но сообщение может быть обрезанно
char* convert_rn(const char* in);

void WifiInit();

//Есть внутренний буффер для ответа на команду.
char* WifiSendCommandAndReceive(const char* cmd, int timeout_ms = -1);

//Сравнивает ответ с result и возвращает true если ответ верный.
bool WifiSendCommandAndCheck(const char* cmd, const char* result, int timeout_ms = -1);

//Возвращает true, если в конце сообщения есть OK.
bool WifiSendCommandAndOk(const char* cmd, char*& result, int timeout_ms = -1);

typedef enum
{
    WifiCWMode_Station = 1,
    WifiCWMode_SoftAP = 2,
    WifiCWMode_SoftAP_Station = 3,
} WifiCWMode;
bool WifiSetCWModeDef(WifiCWMode mode);
bool WifiGetCWModeDef(WifiCWMode* mode);

//cur - только на время этой сессии, иначе будет def вариант, который навсегда
void WifiConnect(const char* wifiname, const char* wifi_password, bool cur = true);
bool WifiTestConnected(bool cur = true);

bool WifiStartUdpServer();
bool WifiStartTcpServer();
bool WifiStopServer();
bool WifiSend(const char* data, int size, uint8_t link_id);

//Возвращает принятый пакет с данными.
//Каждый пакет возвращается только один раз.
//Пакет надо успеть обработать достаточно быстро,
//пока новый не пришёл.
//WifiReceiveBuffer* WifiNextRecivedData();

#ifdef __cplusplus
}
#endif
