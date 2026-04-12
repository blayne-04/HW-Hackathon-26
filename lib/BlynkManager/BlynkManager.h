#ifndef BLYNK_MANAGER_H
#define BLYNK_MANAGER_H

#include <Arduino.h>

// BlynkManager does NOT touch hardware.
// It delegates:
//  - initialization to an optional init hook
//  - periodic Blynk processing to an optional run hook
//  - outgoing messages to a transport publish function
//  - incoming messages are delivered to a registered handler
class BlynkManager {
public:
    using InitHook       = bool(*)(void);                      // return true if init ok
    using RunHook        = void(*)(void);                      // called from loop()
    using TransportPub   = void(*)(const char* key, const char* value);
    using MessageHandler = void(*)(const char* key, const char* value);

    BlynkManager();

    // Lightweight begin: calls init hook if provided. No hardware policy here.
    bool begin();

    // Register helpers provided by other modules:
    void setInitHook(InitHook init);
    void setRunHook(RunHook run);
    void setTransportPublish(TransportPub pub);
    void setMessageHandler(MessageHandler handler);

    // Publish helpers (simple key/value API). Transport must be provided by setTransportPublish().
    void publish(const char* key, const char* value);
    void publishInt(const char* key, int value);
    void publishFloat(const char* key, float value);

    // Called from main loop() to let Blynk processing run (if a run hook is set).
    void loop();

private:
    InitHook       m_init;
    RunHook        m_run;
    TransportPub   m_pub;
    MessageHandler m_handler;
    bool           m_initialized;
};

#endif // BLYNK_MANAGER_H


// ...existing code...
#include "BlynkManager.h"
#include <cstdio>

BlynkManager::BlynkManager()
  : m_init(nullptr),
    m_run(nullptr),
    m_pub(nullptr),
    m_handler(nullptr),
    m_initialized(false)
{}

bool BlynkManager::begin() {
    if (m_init) {
        m_initialized = m_init();
    } else {
        // no init hook provided — treat as successful (manager stays passive)
        m_initialized = true;
    }
    return m_initialized;
}

void BlynkManager::setInitHook(InitHook init) { m_init = init; }
void BlynkManager::setRunHook(RunHook run) { m_run = run; }
void BlynkManager::setTransportPublish(TransportPub pub) { m_pub = pub; }
void BlynkManager::setMessageHandler(MessageHandler handler) { m_handler = handler; }

void BlynkManager::publish(const char* key, const char* value) {
    if (!m_initialized) return;
    if (m_pub) m_pub(key, value);
}

void BlynkManager::publishInt(const char* key, int value) {
    if (!m_initialized) return;
    if (m_pub) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", value);
        m_pub(key, buf);
    }
}

void BlynkManager::publishFloat(const char* key, float value) {
    if (!m_initialized) return;
    if (m_pub) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%g", value);
        m_pub(key, buf);
    }
}

void BlynkManager::loop() {
    if (!m_initialized) return;
    if (m_run) m_run();
}