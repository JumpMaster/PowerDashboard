#ifndef Nextion_h
#define Nextion_h

#include "application.h"

#define nexSerial Serial1

class Nextion
{
public:
  Nextion(void);

  void execute();
  void run(String command);
  void setPic(const int page, const char* name, const int pic);
  void setText(const int page, const char* name, const char* value);
  void refreshComponent(const char* name);
  void setPage(const int page);
  void setBrightness(const int brightness);
  void setSleep(const bool sleep);

protected:
};

#endif
