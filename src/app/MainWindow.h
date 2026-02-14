#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace neurons::app {

class MainWindow final : public juce::DocumentWindow {
public:
    explicit MainWindow(juce::String name);

    void closeButtonPressed() override;
};

} // namespace neurons::app
