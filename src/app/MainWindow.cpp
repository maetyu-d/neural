#include "MainWindow.h"

#include "MainComponent.h"

namespace neurons::app {

MainWindow::MainWindow(juce::String name)
    : juce::DocumentWindow(name,
                           juce::Desktop::getInstance().getDefaultLookAndFeel()
                               .findColour(juce::ResizableWindow::backgroundColourId),
                           juce::DocumentWindow::allButtons) {
    setUsingNativeTitleBar(true);
    setContentOwned(new MainComponent(), true);
    centreWithSize(1024, 720);
    setResizable(true, true);
    setVisible(true);
}

void MainWindow::closeButtonPressed() {
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

} // namespace neurons::app
