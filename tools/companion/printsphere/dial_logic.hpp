#pragma once
#include <cstdint>

// The decoder runs under the GPIO ISR lock; the router runs under the LVGL lock.
// Timing and direction match Orb's knob.cpp / input_router.cpp.
namespace printsphere::dial {
struct Snapshot {
  int32_t position = 0;
  uint32_t turn_at = 0;
  uint32_t rock_sequence = 0, rock_at = 0;
  int32_t rock_position = 0;
  uint32_t press_sequence = 0, press_at = 0;
};

class Decoder {
 public:
  void seed(uint8_t ab, bool button_high, uint32_t now) {
    previous_ab_ = ab;
    button_down_ = !button_high;
    last_button_edge_ = now;
  }

  inline __attribute__((always_inline)) void edge(uint8_t ab, uint32_t now) {
    const unsigned transition = (previous_ab_ << 2) | ab;
    previous_ab_ = ab;
    // Explicit comparisons avoid a lookup table in flash from an IRAM ISR.
    const int step = (transition == 2 || transition == 4 || transition == 11 || transition == 13) ? 1 :
                     (transition == 1 || transition == 7 || transition == 8 || transition == 14) ? -1 : 0;
    travel_ += step;
    if (travel_ >= 4) { travel_ -= 4; detent(1, now); }
    else if (travel_ <= -4) { travel_ += 4; detent(-1, now); }
  }

  inline __attribute__((always_inline)) void button(bool high, uint32_t now) {
    if (high) { button_down_ = false; last_button_edge_ = now; return; }
    if (button_down_ || now - last_button_edge_ < 200) return;
    button_down_ = true;
    last_button_edge_ = now;
    state_.press_at = now;
    ++state_.press_sequence;
  }

  Snapshot snapshot() const { return state_; }

 private:
  inline __attribute__((always_inline)) void detent(int direction, uint32_t now) {
    state_.position += direction;
    state_.turn_at = now;
    const uint32_t gap = now - last_detent_at_;
    const bool fresh = gap > 900;
    if (!fresh && last_direction_ != 0 && direction != last_direction_ && run_length_ <= 2 &&
        gap >= 45 && gap <= 250) {
      state_.rock_at = now;
      state_.rock_position = state_.position;
      ++state_.rock_sequence;
    }
    run_length_ = (!fresh && direction == last_direction_) ? run_length_ + 1 : 1;
    // Long scrolling need not grow the counter without bound.
    if (run_length_ > 3) run_length_ = 3;
    last_direction_ = direction;
    last_detent_at_ = now;
  }
  Snapshot state_{};
  uint8_t previous_ab_ = 3;
  int travel_ = 0, last_direction_ = 0, run_length_ = 0;
  uint32_t last_detent_at_ = 0, last_button_edge_ = 0;
  bool button_down_ = false;
};

struct Action {
  int32_t turn = 0;
  bool opened = false, closed = false, confirmed = false, pressed = false, activity = false;
};

class Router {
 public:
  bool needs_poll(const Snapshot& input) const {
    return !returning_ && (menu_open_ || held_ != 0 || input.position != last_position_ ||
      input.rock_sequence != seen_rock_ || input.press_sequence != seen_press_);
  }
  bool menu_open() const { return menu_open_; }

  Action poll(const Snapshot& input, uint32_t now) {
    Action result;
    if (returning_) return result;
    int32_t delta = input.position - last_position_;
    last_position_ = input.position;
    const bool pressed = input.press_sequence != seen_press_;
    seen_press_ = input.press_sequence;
    result.activity = delta != 0 || pressed;
    if (menu_open_) {
      seen_rock_ = input.rock_sequence;
      if (now - opened_at_ >= 8000 || delta != 0) {
        menu_open_ = false;
        result.closed = true;
      } else if (pressed && static_cast<int32_t>(input.press_at - opened_at_) > 0) {
        returning_ = true;
        result.confirmed = true;
      }
      return result;
    }
    if (input.rock_sequence != seen_rock_) {
      const int32_t after = input.position - input.rock_position;
      const int32_t distance = after < 0 ? -after : after;
      if (distance > 1) {
        seen_rock_ = input.rock_sequence;
        delta += held_;
        held_ = 0;
      } else if (now - input.rock_at < 160) {
        held_ += delta;
        return result;
      } else {
        seen_rock_ = input.rock_sequence;
        held_ = 0;
        menu_open_ = true;
        opened_at_ = now;
        result.opened = true;
        result.activity = true;
        return result;  // A press queued before the menu appeared cannot confirm.
      }
    }
    result.pressed = pressed;
    // Wait out the complete reversal window before showing any page movement.
    // A recognized jig discards both halves above, including its first detent.
    held_ += delta;
    if (now - input.turn_at > 250) {
      result.turn = held_;
      held_ = 0;
    }
    return result;
  }

 private:
  int32_t last_position_ = 0, held_ = 0;
  uint32_t seen_rock_ = 0, seen_press_ = 0, opened_at_ = 0;
  bool menu_open_ = false, returning_ = false;
};
}  // namespace printsphere::dial
