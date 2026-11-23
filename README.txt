Chord Forge - VST3 for FL Studio
================================
Paste chords (e.g.  Dm7 G7 Cmaj7 Am7/E  C6/9  %), pick Soul / Jazz / Funk / R&B, press RANDOMIZE.
Play preview = built-in sound. Drag the purple bar into the FL piano roll (or Save .mid... and drag the file in).

Build on GitHub (all files go in the repo ROOT, no folders):
  1. New repo, upload ChordEngine.cpp/.h, PluginProcessor.cpp/.h, PluginEditor.cpp/.h, CMakeLists.txt
  2. Add file > Create new file > name:  .github/workflows/build.yml  > paste build.yml contents > Commit
  3. Actions tab > wait for green check > download artifact "ChordForge-VST3"
  4. Unzip: copy the folder "Chord Forge.vst3" (as a folder) to C:\Program Files\Common Files\VST3\
  5. FL Studio: Options > Manage plugins > Find installed plugins
