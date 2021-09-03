#pragma once
#include <stdint.h>

class CommandHandlerClass {
public:
  CommandHandlerClass();

  void begin();
  int handle(const uint8_t command[], uint8_t response[]);
private:
};

extern CommandHandlerClass CommandHandler;

extern bool debug;
extern void setDebug(bool debug);
