#ifndef Nextion_h
#define Nextion_h

#include "Particle.h"

#define nexSerial Serial1

class Nextion
{
public:
  Nextion(void);

  void execute(const char* command);
  void loop();
  void setPic(const int page, const char* name, const int pic);
  void setText(const int page, const char* name, const char* value);
  void refreshComponent(const char* name);
  void setPage(const int page);
  void getPage();
  void setBrightness(const int brightness);
  void setSleep(const bool sleep);
  void stopRefreshing();
  void startRefreshing();
  void reset();

protected:
  void sendConnect() { execute("connect"); }
  void checkReturnCode(const char* data, int length);
  uint8_t currentPage = 0;
  char serialData[100];
  uint8_t serialPosition = 0;
};

#endif