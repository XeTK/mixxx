#pragma once

#include <memory>

#include <QString>

class TtsEngine {
  public:
    virtual ~TtsEngine() = default;

    // Speak text asynchronously, interrupting any current speech.
    virtual void say(const QString& text) = 0;

    static std::unique_ptr<TtsEngine> create();
};
