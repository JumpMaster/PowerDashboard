#include "HttpClient.h"
#include "application.h"

static const uint16_t kNetworkTimeout = 5*1000; // Allow maximum 5s between data packets.
/**
* Constructor.
*/
HttpClient::HttpClient()
{

}

void HttpClient::out(const char *s) {
    client.write( (const uint8_t*)s, strlen(s) );
    LOGGING_WRITE( (const uint8_t*)s, strlen(s) );
}
/**
* Method to send a header, should only be called from within the class.
*/
void HttpClient::sendRequest(const char* aHttpMethod, const char* aRequestPath)
{
    sprintf(buffer, "%s %s HTTP/1.0\r\n", aHttpMethod, aRequestPath);
    out(buffer);
}

void HttpClient::sendBody(const char* aRequestBody)
{
    out(aRequestBody);
}
void HttpClient::sendHeader(const char* aHeaderName, const char* aHeaderValue)
{
    sprintf(buffer, "%s: %s\r\n", aHeaderName, aHeaderValue);
    out(buffer);
}

void HttpClient::sendHeader(const char* aHeaderName, const int aHeaderValue)
{
    sprintf(buffer, "%s: %d\r\n", aHeaderName, aHeaderValue);
    out(buffer);
}

void HttpClient::sendHeader(const char* aHeaderName)
{
    sprintf(buffer, "%s\r\n", aHeaderName);
    out(buffer);
}

/**
* Method to send an HTTP Request. Allocate variables in your application code
* in the aResponse struct and set the headers and the options in the aRequest
* struct.
*/
void HttpClient::request(http_request_t &aRequest, http_response_t &aResponse, http_header_t headers[], const char* aHttpMethod)
{
    // If a proper response code isn't received it will be set to -1.
    aResponse.status = -1;

    // NOTE: The default port tertiary statement is unpredictable if the request structure is not initialised
    // http_request_t request = {0} or memset(&request, 0, sizeof(http_request_t)) should be used
    // to ensure all fields are zero
    bool connected = false;
    aRequest.port = aRequest.port > 0 ? aRequest.port : 80;

    // If an IP has been defined connect using that, else use hostname.
    if ((aRequest.ip == (uint32_t) 0) == false) {
      LOGGING_PRINT("HttpClient>\tConnecting via IP: ");
      LOGGING_PRINT(aRequest.ip);
      connected = client.connect(aRequest.ip, aRequest.port);
    } else if (aRequest.hostname != NULL) {
      LOGGING_PRINT("HttpClient>\tConnecting via hostname: ");
      LOGGING_PRINT(aRequest.hostname);
      connected = client.connect(aRequest.hostname.c_str(), aRequest.port);
    } else {
      LOGGING_PRINT("HttpClient>\tNo hostname or IP defined: ");
    }

    LOGGING_PRINT(":");
    LOGGING_PRINTLN(aRequest.port);

    if (!connected) {
      LOGGING_PRINTLN("HttpClient>\tConnection failed.");
      client.stop();
      // If TCP Client can't connect to host, exit here.
      return;
    }

    //
    // Send HTTP Headers
    //

    // Send initial headers (only HTTP 1.0 is supported for now).
    LOGGING_PRINTLN("HttpClient>\tStart of HTTP Request.");

    // Send General and Request Headers.
    sendRequest(aHttpMethod, aRequest.path.c_str());

    // To meet HTTP 1.1 spec always send HOST header.
    if(aRequest.hostname!=NULL) {
      sendHeader("HOST", aRequest.hostname.c_str());
    } else {
      String ip = String(aRequest.ip[0]) + "." +
                  String(aRequest.ip[1]) + "." +
                  String(aRequest.ip[2]) + "." +
                  String(aRequest.ip[3]);
      sendHeader("HOST", ip.c_str());
    }

    sendHeader("User-Agent", kUserAgent);

    sendHeader("Connection", "close"); // Not supporting keep-alive for now.

    //Send Entity Headers
    // TODO: Check the standard, currently sending Content-Length : 0 for empty
    // POST requests, and no content-length for other types.
    if (aRequest.body != NULL) {
        sendHeader("Content-Length", (aRequest.body).length());
    } else if (strcmp(aHttpMethod, HTTP_METHOD_POST) == 0) { //Check to see if its a Post method.
        sendHeader("Content-Length", 0);
    }

    if (headers != NULL)
    {
        int i = 0;
        while (headers[i].header != NULL)
        {
            if (headers[i].value != NULL) {
                sendHeader(headers[i].header, headers[i].value);
            } else {
                sendHeader(headers[i].header);
            }
            i++;
        }
    }

    // Empty line to finish headers
    out("\r\n");
    client.flush();

    //
    // Send HTTP Request Body
    //

    if (aRequest.body != NULL) {
        sendBody(aRequest.body.c_str());
    }

    LOGGING_PRINTLN("HttpClient>\tEnd of HTTP Request.");

    //
    // Receive HTTP Response
    //
    // The first value of client.available() might not represent the
    // whole response, so after the first chunk of data is received instead
    // of terminating the connection there is a delay and another attempt
    // to read data.
    // The loop exits when the connection is closed, or if there is a
    // timeout or an error.

    // clear response buffer
    memset(&buffer[0], 0, sizeof(buffer));
    unsigned int bufferPosition = 0;
    unsigned long lastRead = millis();
  #ifdef LOGGING
    unsigned long firstRead = millis();
  #endif
    bool error = false;
    bool timeout = false;

    do {
        #ifdef LOGGING
        int bytes = client.available();
        if(bytes) {
            LOGGING_PRINT("\r\nHttpClient>\tReceiving TCP transaction of ");
            LOGGING_PRINT(bytes);
            LOGGING_PRINTLN(" bytes.");
        }
        #endif

        while (client.available()) {
            char c = client.read();
            LOGGING_PRINT(c);
            lastRead = millis();

            if (c == -1) {
                error = true;
                LOGGING_PRINTLN("HttpClient>\tError: No data available.");
                break;
            }

            // Check that received character fits in buffer before storing.
            if (bufferPosition < sizeof(buffer)-1) {
                buffer[bufferPosition] = c;
            } else if ((bufferPosition == sizeof(buffer)-1)) {
                buffer[bufferPosition] = '\0'; // Null-terminate buffer
                client.stop();
                error = true;

                LOGGING_PRINTLN("HttpClient>\tError: Response body larger than buffer.");
            }
            bufferPosition++;
        }

        #ifdef LOGGING
        if (bytes) {
            LOGGING_PRINT("\r\nHttpClient>\tEnd of TCP transaction.");
        }
        #endif

        // Check that there hasn't been more than 5s since last read.
        timeout = millis() - lastRead > kNetworkTimeout;

        // Unless there has been an error or timeout wait 200ms to allow server
        // to respond or close connection.
        if (!error && !timeout) {
            delay(200);
        }
    } while (client.connected() && !timeout && !error);

    #ifdef LOGGING
    if (timeout) {
        LOGGING_PRINTLN("\r\nHttpClient>\tError: Timeout while reading response.");
    }
    LOGGING_PRINT("\r\nHttpClient>\tEnd of HTTP Response (");
    LOGGING_PRINT(millis() - firstRead);
    LOGGING_PRINTLN("ms).");
    #endif
    client.stop();

    String raw_response(buffer);

    // Not super elegant way of finding the status code, but it works.
    String statusCode = raw_response.substring(9,12);

    LOGGING_PRINT("HttpClient>\tStatus Code: ");
    LOGGING_PRINTLN(statusCode);

    int bodyPos = raw_response.indexOf("\r\n\r\n");
    if (bodyPos == -1) {
        #ifdef LOGGING
        LOGGING_PRINTLN("HttpClient>\tError: Can't find HTTP response body.");
        #endif

        return;
    }
    // Return the entire message body from bodyPos+4 till end.
    aResponse.body = "";
    aResponse.body += raw_response.substring(bodyPos+4);
    aResponse.status = atoi(statusCode.c_str());
}
