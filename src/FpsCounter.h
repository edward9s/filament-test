#pragma once
#include <cmath>
#include <stdexcept>

namespace forest {
// Counts submitted frames over wall time, including gaps when beginFrame skips.
class FpsCounter {
public:
    void reset() { start_ = -1; previous_ = -1; frames_ = 0; value_ = -1; }
    bool record(double seconds) {
        if (!std::isfinite(seconds) || seconds < 0 || seconds < previous_)
            throw std::invalid_argument("FPS clock must be finite and monotonic");
        previous_ = seconds;
        if (start_ < 0) { start_ = seconds; return false; }
        ++frames_;
        const double elapsed = seconds - start_;
        if (elapsed < 0.5) return false;
        value_ = float(frames_ / elapsed);
        start_ = seconds;
        frames_ = 0;
        return true;
    }
    float value() const { return value_; }
private:
    double start_ = -1, previous_ = -1;
    unsigned frames_ = 0;
    float value_ = -1;
};
}
