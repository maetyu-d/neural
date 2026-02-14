#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "MainWindow.h"

#include <memory>

namespace neurons::app {

class Application final : public juce::JUCEApplication {
public:
    ~Application() override;

    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    bool moreThanOneInstanceAllowed() override;

    void initialise(const juce::String& commandLine) override;
    void shutdown() override;

    void systemRequestedQuit() override;
    void anotherInstanceStarted(const juce::String& commandLine) override;

private:
    std::unique_ptr<MainWindow> window_;
};

} // namespace neurons::app
