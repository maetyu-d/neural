cmake_minimum_required(VERSION 3.22)
project(neural VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set -DJUCE_PATH=/path/to/JUCE to use a local JUCE checkout.
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    message(FATAL_ERROR "JUCE_PATH is not set. Configure with -DJUCE_PATH=/absolute/path/to/JUCE")
endif()

juce_add_gui_app(neuralApp
    PRODUCT_NAME "neural"
)

target_sources(neuralApp PRIVATE
    src/app/main.cpp
    src/app/Application.cpp
    src/app/Application.h
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/MainComponent.cpp
    src/app/MainComponent.h
    src/app/PatchCanvas.cpp
    src/app/PatchCanvas.h

    src/engine/core/SignalTypes.h
    src/engine/core/GraphTypes.h
    src/engine/core/GraphModel.cpp
    src/engine/core/GraphModel.h
    src/engine/core/TopologicalScheduler.cpp
    src/engine/core/TopologicalScheduler.h

    src/engine/dsp/AudioBufferView.h
    src/engine/dsp/CycleSolver.cpp
    src/engine/dsp/CycleSolver.h
    src/engine/dsp/EngineConfig.h

    src/engine/nodes/NodeProcessor.h
    src/engine/nodes/AdaptiveThresholdNode.cpp
    src/engine/nodes/AdaptiveThresholdNode.h
    src/engine/nodes/AnalogAndNode.cpp
    src/engine/nodes/AnalogAndNode.h
    src/engine/nodes/AnalogOrNode.cpp
    src/engine/nodes/AnalogOrNode.h
    src/engine/nodes/AnalogXorNode.cpp
    src/engine/nodes/AnalogXorNode.h
    src/engine/nodes/AnalogNandNode.cpp
    src/engine/nodes/AnalogNandNode.h
    src/engine/nodes/AnalogNorNode.cpp
    src/engine/nodes/AnalogNorNode.h
    src/engine/nodes/AddNode.cpp
    src/engine/nodes/AddNode.h
    src/engine/nodes/BurstNeuronNode.cpp
    src/engine/nodes/BurstNeuronNode.h
    src/engine/nodes/BytebeatJsNode.cpp
    src/engine/nodes/BytebeatJsNode.h
    src/engine/nodes/CompareNode.cpp
    src/engine/nodes/CompareNode.h
    src/engine/nodes/ConstantNode.cpp
    src/engine/nodes/ConstantNode.h
    src/engine/nodes/NeuronCoreNode.cpp
    src/engine/nodes/NeuronCoreNode.h
    src/engine/nodes/MixNode.cpp
    src/engine/nodes/MixNode.h
    src/engine/nodes/OscillatorNode.cpp
    src/engine/nodes/OscillatorNode.h
    src/engine/nodes/OutputNode.cpp
    src/engine/nodes/OutputNode.h
    src/engine/nodes/SynapseNode.cpp
    src/engine/nodes/SynapseNode.h
    src/engine/nodes/AllpassNode.cpp
    src/engine/nodes/AllpassNode.h
    src/engine/nodes/AllpassBankNode.cpp
    src/engine/nodes/AllpassBankNode.h
    src/engine/nodes/CombFilterNode.cpp
    src/engine/nodes/CombFilterNode.h
    src/engine/nodes/CounterNode.cpp
    src/engine/nodes/CounterNode.h
    src/engine/nodes/InvertNode.cpp
    src/engine/nodes/InvertNode.h
    src/engine/nodes/ModuloNode.cpp
    src/engine/nodes/ModuloNode.h
    src/engine/nodes/IntegratorNode.cpp
    src/engine/nodes/IntegratorNode.h
    src/engine/nodes/LeakNode.cpp
    src/engine/nodes/LeakNode.h
    src/engine/nodes/ThresholdNode.cpp
    src/engine/nodes/ThresholdNode.h
    src/engine/nodes/PulseNode.cpp
    src/engine/nodes/PulseNode.h
    src/engine/nodes/RandomGateNode.cpp
    src/engine/nodes/RandomGateNode.h
    src/engine/nodes/RefractoryGateNode.cpp
    src/engine/nodes/RefractoryGateNode.h
    src/engine/nodes/GateNode.cpp
    src/engine/nodes/GateNode.h
    src/engine/nodes/SlewNode.cpp
    src/engine/nodes/SlewNode.h
    src/engine/nodes/WaveshaperNode.cpp
    src/engine/nodes/WaveshaperNode.h
    src/engine/nodes/NoiseNode.cpp
    src/engine/nodes/NoiseNode.h
    src/engine/nodes/DriftNode.cpp
    src/engine/nodes/DriftNode.h
    src/engine/nodes/FeedbackTapNode.cpp
    src/engine/nodes/FeedbackTapNode.h
    src/engine/nodes/OscillatorPhaseNode.cpp
    src/engine/nodes/OscillatorPhaseNode.h
    src/engine/nodes/PhaseOpsNode.cpp
    src/engine/nodes/PhaseOpsNode.h
    src/engine/nodes/DelayShortNode.cpp
    src/engine/nodes/DelayShortNode.h
    src/engine/nodes/DendriteNonlinearityNode.cpp
    src/engine/nodes/DendriteNonlinearityNode.h
    src/engine/nodes/DendriteSumNode.cpp
    src/engine/nodes/DendriteSumNode.h
    src/engine/nodes/BiquadCoreNode.cpp
    src/engine/nodes/BiquadCoreNode.h
    src/engine/nodes/SampleHoldNode.cpp
    src/engine/nodes/SampleHoldNode.h
    src/engine/nodes/SampleHoldGatedNode.cpp
    src/engine/nodes/SampleHoldGatedNode.h
    src/engine/nodes/SampleHoldClockedNode.cpp
    src/engine/nodes/SampleHoldClockedNode.h
    src/engine/nodes/SampleHoldSlewNode.cpp
    src/engine/nodes/SampleHoldSlewNode.h
    src/engine/nodes/SamplePlayerWavNode.cpp
    src/engine/nodes/SamplePlayerWavNode.h
    src/engine/nodes/SampleHoldQuantizedNode.cpp
    src/engine/nodes/SampleHoldQuantizedNode.h
    src/engine/nodes/CrossfadeVCANode.cpp
    src/engine/nodes/CrossfadeVCANode.h
    src/engine/nodes/SaturatorNode.cpp
    src/engine/nodes/SaturatorNode.h
    src/engine/nodes/ScopeProbeNode.cpp
    src/engine/nodes/ScopeProbeNode.h
    src/engine/nodes/SlopeDetectNode.cpp
    src/engine/nodes/SlopeDetectNode.h
    src/engine/nodes/SchmittTriggerNode.cpp
    src/engine/nodes/SchmittTriggerNode.h
    src/engine/nodes/SpikeGeneratorNode.cpp
    src/engine/nodes/SpikeGeneratorNode.h
    src/engine/nodes/SwitchNode.cpp
    src/engine/nodes/SwitchNode.h
    src/engine/nodes/UnitConvertNode.cpp
    src/engine/nodes/UnitConvertNode.h
    src/engine/nodes/MultiplyNode.cpp
    src/engine/nodes/MultiplyNode.h
    src/engine/nodes/DivideNode.cpp
    src/engine/nodes/DivideNode.h
    src/engine/nodes/WindowComparatorNode.cpp
    src/engine/nodes/WindowComparatorNode.h
    src/engine/nodes/MembraneLeakCapNode.cpp
    src/engine/nodes/MembraneLeakCapNode.h
    src/engine/nodes/DiffusionBlockNode.cpp
    src/engine/nodes/DiffusionBlockNode.h

    src/engine/rt/AudioEngine.cpp
    src/engine/rt/AudioEngine.h

    src/engine/persistence/PatchDocument.h
)

target_compile_definitions(neuralApp PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(neuralApp PRIVATE
    juce::juce_gui_extra
    juce::juce_audio_utils
    juce::juce_javascript
    juce::juce_osc
)
