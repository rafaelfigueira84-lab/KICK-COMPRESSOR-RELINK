# RELINK Kick Enhancer v1.1

VST3 for Cubase 10.5 Pro on Windows 10 (64-bit). Requires a C++17 compiler, CMake 3.22+, internet access to fetch JUCE on first build.

## Build with GitHub Actions

Create a GitHub repository and upload the **contents** of this folder to its root (including `.github/workflows/build-windows.yml`). Open **Actions → Build Windows VST3 → Run workflow**. Download the resulting artifact, extract it and copy the `.vst3` bundle to `C:\Program Files\Common Files\VST3\`. Restart Cubase and find **RELINK Kick Enhancer** in the VST3 effects list.

## Usage

1. Insert on a dedicated kick audio/instrument channel.
2. Choose **Natural** (restrained) or **Club** (stronger).
3. Play kick audio, click **ANALYZE KICK** and continue playback until the sample collection completes (~0.37 s at 44.1 kHz).
4. Inspect suggested frequency and gain values; click **APPLY CORRECTION**.
5. The Smart Dynamics section reports **attack (ms)**, **decay (ms)**, **crest factor (dB)** and the suggested compression ratio/threshold.
6. Compare using **BYPASS**, and dial down **CORRECTION AMOUNT** if needed. Fine-tune **PUNCH** and **TIGHT** manually.

## Dynamics module (v1.1)

- Analyzes attack, crest factor and decay from the captured kick waveform.
- Recommends ratio 1:1 when compression is not indicated by its heuristic. Otherwise applies a conservative peak-envelope compressor with suggested threshold, ratio, attack and release.
- Adds a parallel transient-envelope **Punch** enhancer and a **Tight** sustain reducer. The two sliders remain independently adjustable after pressing Apply.
- Dynamics only processes audio after **Apply**; before then the processor passes audio unchanged.
- This is a first-pass dynamics diagnostic, not a validated objective notion of the correct compression. Judge by listening and metering.

## Honest limitations of v1.0

- The analyzer uses a 16,384-sample FFT capture of the input. It is an initial heuristic, **not** a trained mastering/reference-match engine. It can misidentify fundamentals if the passage includes silence or other instruments. Isolate the kick.
- The analysis captures only ~0.37 s at 44.1 kHz, and may miss the entire tail of a long kick or the start of a transient. For most dependable results, begin playback on repeated isolated kicks before clicking Analyze.
- The proposed EQ uses one high-pass and three peaking filters. The mid and high center frequencies are intentionally fixed in v1.0, while their gains adapt; it does not yet identify arbitrary resonant peaks or perform dynamic EQ.
- Auto output trim is an estimated compensation, not calibrated LUFS matching or a limiter. Always monitor peak meters and compare at matched loudness.
- VST3 plugin processing is applied live and does not destructively rewrite source samples.
