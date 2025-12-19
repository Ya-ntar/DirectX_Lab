#include "Timer.h"

namespace gfw {
    Timer::Timer() {
        QueryPerformanceFrequency(&frequency_);
        QueryPerformanceCounter(&start_time_);
        previous_time_ = start_time_;
        current_time_ = start_time_;
    }

    void Timer::Reset() {
        QueryPerformanceCounter(&start_time_);
        previous_time_ = start_time_;
        current_time_ = start_time_;
        delta_time_ = 0.0;
        total_time_ = 0.0;
    }

    void Timer::Tick() {
        QueryPerformanceCounter(&current_time_);

        delta_time_ = static_cast<double>(current_time_.QuadPart - previous_time_.QuadPart) /
                      static_cast<double>(frequency_.QuadPart);
        total_time_ = static_cast<double>(current_time_.QuadPart - start_time_.QuadPart) /
                      static_cast<double>(frequency_.QuadPart);

        previous_time_ = current_time_;
    }
}
