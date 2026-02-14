# neural (v0 Skeleton)

Initial JUCE standalone skeleton with first-pass DSP engine architecture.

## Configure

```bash
cmake -S . -B build -DJUCE_PATH=/absolute/path/to/JUCE
cmake --build build
```

## Current status

- Standalone JUCE app shell with a status UI.
- Engine core with typed signals and graph model.
- Topological scheduler with cycle-aware partitioning.
- Cycle solver with default cap and high precision cap.
- First utility nodes: Mix, Saturator, UnitConvert.
- Audio callback wrapper for future graph execution.

This is a first pass focused on scaffolding and compile-time structure.
