# Smoosher

A free audio compressor plugin with character. Smoosher combines aggressive compression, tube-style saturation, and intelligent sibilance detection into a simple, intuitive interface.

## Features

### Core Processing
- **Single "Smoosh" Control**: One knob controls threshold, ratio, attack, release, makeup gain, and saturation
  - 0-60%: Gentle to moderate compression
  - 60-100%: "Hammer Mode" - extreme, aggressive compression with fast attack
- **Input Gain**: 0 to +30 dB drive stage
- **Output Gain**: -12 to +12 dB output trim
- **Mix Control**: 0-100% wet/dry blend for parallel processing

### Signal Processing
- Adaptive compression with musical attack/release curves
- Tube-style soft-clipping saturation using hyperbolic tangent
- Intelligent sibilance detection with high-pass sidechain
- Adaptive low-pass filtering to reduce high-frequency harshness
- Soft limiting in hammer mode to prevent hard clipping

### UI Features
- Compact, clean interface (500x260px)
- Color-gradient knobs (green to red) for visual feedback
- 7 factory presets plus Init preset:
  - **Hell Smarsh**: Balanced warmth (45% smoosh, 15dB input, +1dB gain)
  - **Wenger's 7-String**: Modern tightness (60% smoosh, 10.5dB input, +2dB gain)
  - **That Joe Sound**: Natural dynamics (70% smoosh, 15dB input, 0dB gain)
  - **Gravy River**: Rich and smooth (70% smoosh, 12dB input, +2dB gain)
  - **Dani Time**: Subtle enhancement (30% smoosh, 35dB input, +2dB gain)
  - **Gloopy**: Warm and vintage (40% smoosh, 7.5dB input, +1dB gain)
  - **Limit THIS!**: Maximum aggression (75% smoosh, 24dB input, +3dB gain)
- Mouse wheel parameter control
- Mix control independent of presets

## Installation

### macOS
1. Download the latest release
2. Extract the zip file
3. Copy the plugin to the appropriate folder:
   - **AU**: `~/Library/Audio/Plug-Ins/Components/`
   - **VST3**: `~/Library/Audio/Plug-Ins/VST3/`
4. Rescan plugins in your DAW

### Windows
1. Download the latest release
2. Extract the zip file
3. Copy `Smoosher.vst3` to:
   - `C:\Program Files\Common Files\VST3\`
4. Rescan plugins in your DAW

## Building from Source

### Prerequisites
- CMake 3.15 or higher
- C++ compiler with C++17 support
  - macOS: Xcode Command Line Tools
  - Windows: Visual Studio 2019 or higher
  - Linux: GCC 7+ or Clang 6+
- Git

### Build Instructions

1. Clone the repository:
```bash
git clone https://github.com/yourusername/smoosher.git
cd smoosher
```

2. Create build directory:
```bash
mkdir build
cd build
```

3. Configure with CMake:
```bash
cmake ..
```

4. Build:
```bash
# Release build
cmake --build . --config Release

# Debug build (for development)
cmake --build . --config Debug
```

5. The compiled plugins will be in:
   - `build/Smoosher_artefacts/AU/` (macOS only)
   - `build/Smoosher_artefacts/VST3/`
   - `build/Smoosher_artefacts/Standalone/`

### Installing Built Plugins

macOS:
```bash
# AU
cp -r build/Smoosher_artefacts/AU/Smoosher.component ~/Library/Audio/Plug-Ins/Components/

# VST3
cp -r build/Smoosher_artefacts/VST3/Smoosher.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

Windows:
```powershell
# VST3
copy build\Smoosher_artefacts\VST3\Smoosher.vst3 "C:\Program Files\Common Files\VST3\"
```

## Customization

### Adding/Modifying Presets

Presets are defined in `Source/PluginEditor.cpp` in the `setupPresets()` function:

```cpp
void SmoosherAudioProcessorEditor::setupPresets()
{
    // Format: {"Name", Smoosh %, Input dB (0-30), Output/Gain dB (-12 to +12)}
    factoryPresets.push_back({"Preset Name", 50.0f, 12.0f, 0.0f});
    // Add more presets here...
}
```

**Preset Parameters:**
- **Name**: String (appears in dropdown)
- **Smoosh**: 0.0 to 100.0 (compression amount)
- **Input**: 0.0 to 30.0 (input gain in dB)
- **Output/Gain**: -12.0 to 12.0 (output gain in dB)

### Adjusting Default Parameter Values

Default values are set in `Source/PluginProcessor.cpp` in the `createParameterLayout()` function:

```cpp
// Smoosh default (4th parameter)
layout.add(std::make_unique<juce::AudioParameterFloat>(
    "smoosh", "Smoosh",
    juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
    0.0f,  // <- Change this value (0-100)
    // ...
));

// Input Gain default
layout.add(std::make_unique<juce::AudioParameterFloat>(
    "inputGain", "Input",
    juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f),
    0.0f,  // <- Change this value (0-30)
    // ...
));

// Output Gain default
layout.add(std::make_unique<juce::AudioParameterFloat>(
    "outputGain", "Output",
    juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f),
    0.0f,  // <- Change this value (-12 to +12)
    // ...
));

// Mix default
layout.add(std::make_unique<juce::AudioParameterFloat>(
    "mix", "Mix",
    juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
    100.0f,  // <- Change this value (0-100, 100 = fully wet)
    // ...
));
```

### Adjusting Compression Algorithm

The compression behavior is defined in `Source/PluginProcessor.cpp` in the `processBlock()` function:

**Threshold Curve** (line ~206-218):
```cpp
// Normal mode: 0 to -12 dB
threshold = juce::jmap(remappedSmoosh * remappedSmoosh, 0.0f, -12.0f);

// Hammer mode: -12 dB to -6.4 dB
threshold = juce::jmap(hammerAmount, -12.0f, -6.4f);
```

**Compression Ratio** (line ~220-232):
```cpp
// Normal mode: 1:1 to 10:1
ratio = juce::jmap(remappedSmoosh, 1.0f, 10.0f);

// Hammer mode: 10:1 to 16:1
ratio = juce::jmap(hammerAmount, 10.0f, 16.0f);
```

**Attack Time** (line ~234-245):
```cpp
// Normal mode: 10ms to 3ms
attackMs = juce::jmap(remappedSmoosh, 10.0f, 3.0f);

// Hammer mode: 3ms to 1ms
attackMs = juce::jmap(hammerAmount, 3.0f, 1.0f);
```

**Release Time** (line ~247-258):
```cpp
// Normal mode: 100ms to 200ms
releaseMs = juce::jmap(remappedSmoosh, 100.0f, 200.0f);

// Hammer mode: 200ms to 150ms
releaseMs = juce::jmap(hammerAmount, 200.0f, 150.0f);
```

**Makeup Gain** (line ~267-279):
```cpp
// Normal mode: 1.0 to 2.0
makeupGain = 1.0f + (remappedSmoosh * remappedSmoosh * 1.0f);

// Hammer mode: 2.0 to 2.8
makeupGain = juce::jmap(hammerAmount, 2.0f, 2.8f);
```

**Saturation Amount** (line ~281-292):
```cpp
// Normal mode: 0 to 0.24
saturationAmount = remappedSmoosh * 0.24f;

// Hammer mode: 0.24 to 0.28
saturationAmount = juce::jmap(hammerAmount, 0.24f, 0.28f);
```

### Customizing the UI

**Window Size** - `Source/PluginEditor.cpp` line 56:
```cpp
setSize (500, 260);  // Width, Height
```

**Control Sizes** - `Source/PluginEditor.cpp` in `resized()` function:
```cpp
// Input/Output knobs (line ~201, 213)
auto inputSliderBounds = inputArea.withSizeKeepingCentre(110, 110);
auto outputSliderBounds = outputArea.withSizeKeepingCentre(110, 110);

// Smoosh knob (line ~207)
auto smooshSliderBounds = smooshArea.withSizeKeepingCentre(140, 140);

// Mix knob (line ~219)
auto mixSliderBounds = mixArea.withSizeKeepingCentre(48, 48);
```

**Color Gradient** - `Source/PluginEditor.cpp` line 19-24:
```cpp
juce::Colour fillColour = juce::Colour::fromHSV(
    juce::jmap(sliderPos, 0.33f, 0.0f),  // Hue: 0.33 (green) to 0.0 (red)
    0.8f,  // Saturation
    0.7f,  // Brightness
    1.0f   // Alpha
);
```

## Technical Details

### Built With
- [JUCE Framework 8.0.4](https://juce.com/) - Cross-platform audio plugin framework
- CMake - Build system

### Supported Formats
- AU (Audio Units) - macOS only
- VST3 - macOS, Windows, Linux
- AAX - Coming soon (pending Avid developer approval)

### DSP Overview
1. Input gain stage
2. Tube-style saturation (tanh soft clipping)
3. Adaptive low-pass filtering
4. Sibilance-sensitive envelope detection
5. Dynamic compression with musical curves
6. Soft limiting (hammer mode only)
7. Makeup gain
8. Output gain
9. Wet/dry mixing

### Project Structure
```
Smoosher/
├── Source/
│   ├── PluginProcessor.h      # Audio processor declaration
│   ├── PluginProcessor.cpp    # DSP implementation
│   ├── PluginEditor.h         # UI declaration
│   └── PluginEditor.cpp       # UI implementation
├── CMakeLists.txt             # Build configuration
└── README.md                  # This file
```

## Development

### Running Tests
After building, you can test the plugin:

**Standalone App:**
```bash
open build/Smoosher_artefacts/Standalone/Smoosher.app  # macOS
```

**Audio Unit Validation (macOS):**
```bash
auval -v aufx Smsh YrCm  # Validate AU plugin
```

### Code Style
- C++17 standard
- JUCE coding conventions
- Comments for complex DSP sections

## Usage Tips

1. **Start Simple**: Begin with a preset that matches your source material
2. **Parallel Processing**: Use the Mix control to blend compressed and dry signals for "New York" style compression
3. **Drive It**: Push the Input gain to add saturation character before compression
4. **Hammer Mode**: Beyond 60% Smoosh for aggressive, limiting-style compression
5. **Match Levels**: Use Output gain to match bypassed levels, or add extra punch

## License

This project is free and open source. Feel free to use, modify, and distribute.

## Credits

Built as a need by Seánan McCullough, Helvete Sound, LLC, Portland, OR - https://helvetesound.com

Built with JUCE Framework by Roli Ltd.

## Support

For issues, questions, or feature requests, please open an issue on GitHub.

---

**Note:** This plugin is provided as-is with no warranty. Always test thoroughly before using in production.
