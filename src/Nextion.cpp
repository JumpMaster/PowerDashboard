#include "Nextion.h"

Nextion::Nextion()
{
}

void Nextion::loop() {
    while (nexSerial.available()) {
        int byte = nexSerial.read();
        // Log.info("%x", byte);
        serialData[serialPosition++] = byte;
        if (byte == 0xff && serialPosition >= 3) {
            if (serialData[serialPosition-1] == 0xff &&
                serialData[serialPosition-2] == 0xff &&
                serialData[serialPosition-3] == 0xff) {
                serialData[serialPosition-3] = '\0';
                checkReturnCode(serialData, serialPosition-3);
                serialPosition = 0;
            }
        }
    }
}
void Nextion::checkReturnCode(const char* data, int length) {
    switch (data[0]) {
        case 0: // Startup
            if (length == 3 && data[0]+data[1]+data[2] == 0) { // TJC Startup
                // tjcStartup = true;
		        Log.info("Startup");
		        return;
		    }
            break;
        case 0x02: // 	Invalid Component ID
            Log.info("Nex Invalid component ID");
            break;
	    case 0x63: // Connect
        {
            char d[length];
            strcpy(d, data);
            char *p = strtok(d, ",");
            for (int i = 0; i < 7; i++) {
                if (p == NULL)
                    break;
                    
                switch (i) {
                    case 0:
                        Log.info("Nex comok - %s\n", p);
                        break;
                    case 1:
                        Log.info("Nex reserved - %s\n", p);
                        break;
                    case 2:
                        Log.info("Nex model - %s\n", p);
                        break;
                    case 3:
                        Log.info("Nex firmware - %s\n", p);
                        break;
                    case 4:
                        Log.info("Nex mcu - %s\n", p);
                        break;
                    case 5:
                        if (strlen(p) == 16) {
                            Log.info("Nex Device Serial=\"%s\"", p);
                            // tjcDownload.setSerialNumber(p);
                        } else {
                            Log.info("Nex serial - invalid length");
                        }
                        break;
                    case 6:
                        Log.info("Nex flash size - %s\n", p);
                        break;
                }
                p = strtok(NULL, ",");
            }
            // tjcConnected = true;
            Log.info("Nex Connected");
            // getVersion();
            break;
        }
        case 0x70: // String data

            // if (deviceVersionRequested) {
                // deviceVersionRequested = false;
                
            char buffer[100];
            memcpy(buffer, &data[1], strlen(data)-1);
            Log.info("\"%s\"", buffer);

                // const size_t capacity = 300;//JSON_OBJECT_SIZE(2) + 30;
                // DynamicJsonDocument doc(capacity);

                // Parse JSON object
                // DeserializationError error = deserializeJson(doc, buffer);

                // if (!error) {
                    // if (strlen(doc["version"]) < 12) {
                        // Log.info("Device Version=\"%s\"", doc["version"].as<const char*>());
                        // tjcDownload.setVersionNumber(doc["version"]);
                        // tjcVerified = true;
                        // Log.info("Verified");
                    // }
                // } else {
                    // Log.info("It's null");
                // }
            // }

		    break;
        case 0x66: // Current Page Number
            currentPage = data[1];
            Log.info("Nex Current page is %d", currentPage);
            break;
        case 0x88: // Ready
            // tjcReady = true;
            currentPage = 0;
            Log.info("Nex Ready");
            // delay(500); // TODO: Add delay system
            sendConnect();
            break;
        case 0x1a: // Invalid Variable name or attribute
            Log.info("Nex Invalid variable name or attribute");
            break;
        default:
            Log.info("Nex Return data length - %d. char[0] - 0x%x", length, data[0]);
            break;
    }
}

void Nextion::execute(const char* command) {
  // if (upgradeState <= CheckInProgress) {
	 nexSerial.print(command);
	 nexSerial.print("\xFF\xFF\xFF");
  // }
}

void Nextion::reset() {
    execute("rest");
}

void Nextion::setPic(const int page, const char* name, const int pic) {
    char buffer[100];
    sprintf(buffer, "page%d.%s.pic=%d", page, name, pic);
    execute(buffer);
}

void Nextion::setText(const int page, const char* name, const char* value) {
    char buffer[100];
    sprintf(buffer, "page%d.%s.txt=\"%s\"", page, name, value);
    execute(buffer);
}

void Nextion::refreshComponent(const char* name) {
    String cmd = "ref ";
    cmd += name;
    execute(cmd);
}

void Nextion::setPage(const int page) {
    execute("page "+String(page, DEC));
    currentPage = page;
}

void Nextion::getPage() {
    execute("sendme");
}

void Nextion::setBrightness(const int brightness) {
    execute("dim="+String(brightness, DEC));
}

void Nextion::setSleep(const bool sleep) {
    execute("sleep="+String(sleep, DEC));
}

void Nextion::stopRefreshing() {
    execute("ref_stop");
}

void Nextion::startRefreshing() {
    execute("ref_star");
}