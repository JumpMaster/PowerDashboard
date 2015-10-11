#include "Nextion.h"

Nextion::Nextion()
{
}


void Nextion::execute() {
//  nexSerial.println();
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
}

void Nextion::run(String command) {
  nexSerial.write(command);
  execute();
}

void Nextion::setPic(const int page, const char* name, const int pic) {
  char buffer[100];
  sprintf(buffer, "page%d.%s.pic=%d", page, name, pic);
  nexSerial.write(buffer);
  execute();
}

void Nextion::setText(const int page, const char* name, const char* value) {
  char buffer[100];
  sprintf(buffer, "page%d.%s.txt=\"%s\"", page, name, value);
  nexSerial.write(buffer);
  execute();
}

void Nextion::refreshComponent(const char* name) {
  String cmd = "ref ";
  cmd += name;
  nexSerial.write(cmd);
  execute();
}

void Nextion::setPage(const int page) {
  nexSerial.write("page "+String(page));
  execute();
}

void Nextion::setBrightness(const int brightness) {
  nexSerial.write("dim="+String(brightness));
  execute();
}

void Nextion::setSleep(const bool sleep) {
  nexSerial.write("sleep="+String(sleep));
  execute();
}
