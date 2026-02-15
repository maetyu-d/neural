# neural Tutorial Patches

Load these from the app with `Load`:

- `tutorial_01_basic_synth.json`: dual oscillator -> mix -> filter -> stereo out
- `tutorial_02_filter_modulation.json`: audio-rate modulation driving filter cutoff
- `tutorial_03_texture_chain.json`: noise through spatial/filter chain
- `tutorial_04_osc_io_loop.json`: OSC input + output loopback test patch
- `tutorial_05_bit_lab.json`: bit-crush/quantize/per-bit-delay chain

Notes:
- OSC patch expects:
  - receive on `/neural/in/<nodeId>` to UDP `9000`
  - send on `/neural/out/<nodeId>` to UDP `9001`
