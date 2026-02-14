#include "Application.h"

#include "MainWindow.h"

namespace neurons::app {

Application::~Application() = default;

const juce::String Application::getApplicationName() {
    return "neural";
}

const juce::String Application::getApplicationVersion() {
    return "0.1.0";
}

bool Application::moreThanOneInstanceAllowed() {
    return true;
}

void Application::initialise(const juce::String&) {
    window_ = std::make_unique<MainWindow>(getApplicationName());
}

void Application::shutdown() {
    window_.reset();
}

void Application::systemRequestedQuit() {
    quit();
}

void Application::anotherInstanceStarted(const juce::String&) {}

} // namespace neurons::app
