#include "Nextion.h"

Nextion::Nextion()
{
}


void Nextion::execute() {
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
}

void Nextion::run(const char *command) {
  nexSerial.write(command);
  execute();
}

void Nextion::reset() {
    nexSerial.print("rest");
    execute();
}

void Nextion::setPic(const int page, const char* name, const int pic) {
  char buffer[100];
  sprintf(buffer, "page%d.%s.pic=%d", page, name, pic);
  nexSerial.print(buffer);
  execute();
}

void Nextion::setText(const int page, const char* name, const char* value) {
  char buffer[100];
  sprintf(buffer, "page%d.%s.txt=\"%s\"", page, name, value);
  nexSerial.print(buffer);
  execute();
}

void Nextion::refreshComponent(const char* name) {
  String cmd = "ref ";
  cmd += name;
  nexSerial.print(cmd);
  execute();
}

void Nextion::setPage(const int page) {
  nexSerial.print("page "+String(page, DEC));
  execute();
}

void Nextion::setBrightness(const int brightness) {
  nexSerial.print("dim="+String(brightness, DEC));
  execute();
}

void Nextion::setSleep(const bool sleep) {
  nexSerial.print("sleep="+String(sleep, DEC));
  execute();
}

void Nextion::stopRefreshing() {
  nexSerial.print("ref_stop");
  execute();
}

void Nextion::startRefreshing() {
  nexSerial.print("ref_star");
  execute();
}