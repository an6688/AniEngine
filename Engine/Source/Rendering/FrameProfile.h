#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <chrono>
#include <string>
struct FrameSample {
    double frameMs = 0, updateMs = 0, recordMs = 0, presentMs = 0, fenceMs = 0;
    std::array<double, 3> gpuMs{};
    unsigned shadowDraws = 0, sceneDraws = 0;
};
inline double ProfileNowMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline double ProfilePercentile(std::vector<double> values, double percentile) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    auto index = static_cast<size_t>(std::ceil(percentile * values.size()));
    return values[std::min(values.size() - 1, index ? index - 1 : 0)];
}
struct FrameProfile {
    static constexpr unsigned CaptureFrames = 600;
    std::vector<FrameSample> samples;
    FrameSample latest;
    std::string context;
    unsigned warmup = 0;
    bool capturing = false;
    unsigned generation = 0;
    void Start() { samples.clear(); samples.reserve(CaptureFrames); warmup = 120; capturing = true; ++generation; }
    void Add(const FrameSample& sample, unsigned sampleGeneration) {
        latest = sample;
        if (!capturing || sampleGeneration != generation) return;
        if (warmup) { --warmup; return; }
        samples.push_back(sample);
        if (samples.size() == CaptureFrames) capturing = false;
    }
};
