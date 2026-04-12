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
