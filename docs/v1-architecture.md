# V1 Architecture (First Pass)

## Modules

- `src/engine/core`: graph IR, node/edge typing, scheduling.
- `src/engine/dsp`: sample processing helpers and cycle iteration logic.
- `src/engine/nodes`: node DSP processors.
- `src/engine/rt`: audio callback integration.
- `src/engine/persistence`: patch document interfaces.

## Runtime strategy

- Single-threaded DSP.
- Sample-accurate processing loop.
- Fixed execution order from scheduler.
- Cycles processed with bounded per-sample iteration (`8` default, `16` high precision).
- Safety path expected to clamp/limit when non-convergent.
