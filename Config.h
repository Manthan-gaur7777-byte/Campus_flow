#ifndef CAMPUSFLOW_CONFIG_H
#define CAMPUSFLOW_CONFIG_H

namespace campusflow {

struct Config {
    // Default hysteresis policy: trigger at 80%, clear at 70%.
    // These are project defaults, not measured real-world thresholds.
    double highCongestionThresholdRatio = 0.80;
    double clearCongestionThresholdRatio = 0.70;
};

} // namespace campusflow

#endif // CAMPUSFLOW_CONFIG_H
