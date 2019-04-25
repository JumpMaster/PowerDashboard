#ifndef __PUBLISHQUEUE_H__
#define __PUBLISHQUEUE_H__

#include "application.h"
#include <queue>

#define canPublish() nextPublishTime < millis() && Particle.connected()

typedef struct Event {
    char name[64];
    char data[256];
    int ttl;
    PublishFlag flags = PRIVATE;
} Event;

typedef struct Buffer {
    Event events[10];
    size_t head = 0;
    size_t tail = 0;
    size_t size = 10; //of the buffer
} Buffer;

class PublishQueue {
public:
    bool publish(const char *name, const char *data, int ttl=60, PublishFlag flags=PRIVATE) {
        
        bool result = true;
        
        if (canPublish()) {
            result = Particle.publish(name, data, ttl, flags);
        } else
            result = false;
        
        if (!result) {
            Event event;
            event.ttl = ttl;
            strcpy(event.name, name);
            strcpy(event.data, data);
            event.flags = flags;
            if (buffer.head != buffer.tail || bufferEmpty) { //((buffer.head + 1) % buffer.size) != buffer.tail) {
                buffer.events[buffer.head] = event;
                buffer.head = (buffer.head + 1) % buffer.size;
                if (bufferEmpty)
                    bufferEmpty = false;
            }
            nextPublishTime = millis() + 1000;
        }
        
        return result;
    };
    void process() {
        if (!bufferEmpty && canPublish()) {
            Event event = buffer.events[buffer.tail];
            bool success = Particle.publish(event.name, event.data, event.ttl, event.flags);
            if (success) {
                buffer.tail = (buffer.tail + 1) % buffer.size;
                if (buffer.head == buffer.tail)
                    bufferEmpty = true;
            } else
                nextPublishTime = millis() + 1000;
        }
    }
private:
    Buffer buffer;
    bool bufferEmpty = true;
    unsigned long nextPublishTime = 0;
};

#endif  // End of __PUBLISHQUEUE_H__ definition check