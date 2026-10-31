// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace gunblade {
// Observed successful XR submissions and native snapshot cadence. These are not
// compositor FPS or native emulation-speed measurements. Skips are snapshots
// passed between submissions; late intervals are not compositor drop counts.
class FrameCadence {
    std::array<double,600> intervals_{},ages_{};
    uint64_t count_{},age_count_{},updates_{},advanced_{},skipped_{},late_{};
    double first_{},last_{},periods_{},maximum_{},maximum_age_{};
    uint64_t sequence_{};
    bool started_{};

    static double p95(std::array<double,600> values,uint64_t count) noexcept {
        const auto n=static_cast<unsigned>(std::min<uint64_t>(count,values.size()));
        if(!n)return 0;
        std::sort(values.begin(),values.begin()+n);
        return values[static_cast<unsigned>(std::ceil(n*.95))-1]*1000.;
    }
    void anchor(double now,uint64_t sequence) noexcept {
        reset();first_=last_=now;sequence_=sequence;started_=true;
    }
public:
    struct Summary {
        bool valid{};
        double seconds{},submitted_hz{},target_hz{};
        // Distinct snapshots observed, versus total producer sequence progress.
        double native_updates_hz{},native_advanced_hz{};
        double p95_interval_ms{},maximum_interval_ms{};
        uint64_t submitted_intervals{},native_updates{},skipped_native{},late_intervals{};
        double source_age_p95_ms{},source_age_max_ms{};
        uint64_t source_age_samples{};
    };
    void reset() noexcept {
        count_=age_count_=updates_=advanced_=skipped_=late_=0;
        first_=last_=periods_=maximum_=maximum_age_=0;
        sequence_=0;started_=false;
    }
    // Call only after a successful rendered submission. Set eligible=false for
    // null sources, focus loss, pauses or frames without a projection layer.
    // now and source_capture_seconds must share the steady-clock epoch and be
    // seconds; emulated NativeFrame::time is not a source-age timestamp. Zero,
    // nonfinite or future capture times leave age unavailable for that sample.
    void submitted(double now,double period,uint64_t sequence,
                   double source_capture_seconds=0,bool eligible=true) noexcept {
        if(!eligible||!std::isfinite(now)||!std::isfinite(period)||period<=0) {
            reset();return;
        }
        if(!started_||now<=last_||sequence<sequence_) {
            anchor(now,sequence);return;
        }
        const double delta=now-last_;
        if(!std::isfinite(delta)||!std::isfinite(periods_+period)) {
            anchor(now,sequence);return;
        }
        intervals_[count_%intervals_.size()]=delta;++count_;
        periods_+=period;maximum_=std::max(maximum_,delta);
        late_+=delta>period*1.5;
        if(sequence>sequence_) {
            ++updates_;advanced_+=sequence-sequence_;
            skipped_+=sequence-sequence_-1;
        }
        if(std::isfinite(source_capture_seconds)&&source_capture_seconds>0&&source_capture_seconds<=now) {
            const double age=now-source_capture_seconds;
            ages_[age_count_%ages_.size()]=age;++age_count_;
            maximum_age_=std::max(maximum_age_,age);
        }
        last_=now;sequence_=sequence;
    }
    bool ready(double seconds=5) const noexcept {
        return count_&&std::isfinite(seconds)&&seconds>=0&&std::isfinite(last_-first_)&&last_-first_>=seconds;
    }
    Summary summary() const noexcept {
        const double elapsed=last_-first_;
        if(!count_||!std::isfinite(elapsed)||elapsed<=0||periods_<=0)return {};
        return {true,elapsed,count_/elapsed,count_/periods_,updates_/elapsed,
                advanced_/elapsed,p95(intervals_,count_),maximum_*1000.,
                count_,updates_,skipped_,late_,p95(ages_,age_count_),
                maximum_age_*1000.,age_count_};
    }
};
}
