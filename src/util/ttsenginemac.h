#pragma once

#include <QList>
#include <memory>

#include "util/ttsengine.h"

// Native macOS backend, implemented in ttsenginemac.mm using AVSpeechSynthesizer
// (AVFAudio). Kept out of ttsengine.cpp because it requires Objective-C++.
std::unique_ptr<TtsEngine> createMacTtsEngine();
QList<TtsEngine::Voice> enumerateMacTtsVoices();
