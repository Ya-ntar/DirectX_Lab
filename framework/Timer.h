#pragma once

#include <windows.h>
#include "Exports.h"

namespace gfw {
    class GAMEFRAMEWORK_API Timer {
    private:
        LARGE_INTEGER frequency_;
        LARGE_INTEGER start_time_;
        LARGE_INTEGER current_time_;
        LARGE_INTEGER previous_time_;
        double delta_time_ = 0.0;
        double total_time_ = 0.0;

    public:
        Timer();

        void Reset();

        void Tick();

        [[nodiscard]] double GetDeltaTime() const { return delta_time_; }


        [[nodiscard]] double GetTotalTime() const { return total_time_; }
    };
}
