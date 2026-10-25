// BattleBuddy prototype for the LilyGO T-Display-S3 Touch.
//
// Landscape layout: tap the left third for -, the right third for +, and the
// middle to switch the step between 1 and 5. Holding +/- repeats.
// KEY button (GPIO14): undo. BOOT button (GPIO0): short press = next game,
// long press = restart the current game.

#include <Arduino.h>

#include "bb/game.h"
#include "bb/profile.h"
#include "board.h"
#include "display.h"
#include "ui.h"

namespace {

constexpr uint32_t kRepeatDelayMs = 500;
constexpr uint32_t kRepeatIntervalMs = 150;
constexpr uint32_t kLongPressMs = 1000;
constexpr uint32_t kDimAfterMs = 30000;
constexpr uint32_t kBatteryPollMs = 5000;
constexpr uint8_t kBrightness = 200;
constexpr uint8_t kDimBrightness = 20;

Display lcd;
LGFX_Sprite frame(&lcd);

bb_game_t game;
size_t profile_index = 0;
int16_t step = 1;
bool dirty = true;

uint32_t last_input_ms = 0;
bool dimmed = false;
int battery_percent = -1; // -1 = on USB / unknown

struct Button {
    explicit Button(uint8_t p) : pin(p) {}

    uint8_t pin;
    bool down = false;
    bool long_fired = false;
    uint32_t pressed_ms = 0;
};

Button boot_button{PIN_BUTTON_BOOT};
Button key_button{PIN_BUTTON_KEY};

enum class Zone { None, Minus, Step, Plus };
Zone touch_zone = Zone::None;
uint32_t touch_start_ms = 0;
uint32_t last_repeat_ms = 0;

void start_game(size_t index)
{
    profile_index = index % bb_profile_count();
    bb_game_init(&game, bb_profile_at(profile_index));
    dirty = true;
}

// Returns true if this input should be ignored because it only woke the screen.
bool register_input(uint32_t now)
{
    last_input_ms = now;
    if (dimmed) {
        dimmed = false;
        lcd.setBrightness(kBrightness);
        return true;
    }
    return false;
}

void adjust(int16_t delta, uint32_t now)
{
    if (bb_game_adjust(&game, delta, now) != 0) {
        dirty = true;
    }
}

Zone zone_at(int x)
{
    if (x < kWidth / 3) {
        return Zone::Minus;
    }
    if (x >= kWidth * 2 / 3) {
        return Zone::Plus;
    }
    return Zone::Step;
}

void handle_touch(uint32_t now)
{
    lgfx::touch_point_t tp;
    bool touching = lcd.getTouch(&tp) > 0;

    if (!touching) {
        touch_zone = Zone::None;
        return;
    }

    if (touch_zone == Zone::None) {
        // New press.
        if (register_input(now)) {
            touch_zone = Zone::Step; // swallow this press without acting
            return;
        }
        touch_zone = zone_at(tp.x);
        touch_start_ms = now;
        last_repeat_ms = now;
        switch (touch_zone) {
        case Zone::Minus:
            adjust(-step, now);
            break;
        case Zone::Plus:
            adjust(step, now);
            break;
        case Zone::Step:
            step = step == 1 ? 5 : 1;
            dirty = true;
            break;
        case Zone::None:
            break;
        }
        return;
    }

    // Held press: auto-repeat on +/-.
    last_input_ms = now;
    if ((touch_zone == Zone::Minus || touch_zone == Zone::Plus) &&
        now - touch_start_ms >= kRepeatDelayMs && now - last_repeat_ms >= kRepeatIntervalMs) {
        last_repeat_ms = now;
        adjust(touch_zone == Zone::Minus ? -step : step, now);
    }
}

// Calls on_short when released before kLongPressMs, on_long once when held that long.
template <typename Short, typename Long>
void poll_button(Button &b, uint32_t now, Short on_short, Long on_long)
{
    bool down = digitalRead(b.pin) == LOW;

    if (down && !b.down) {
        b.down = true;
        b.long_fired = false;
        b.pressed_ms = now;
    } else if (down && !b.long_fired && now - b.pressed_ms >= kLongPressMs) {
        b.long_fired = true;
        if (!register_input(now)) {
            on_long();
        }
    } else if (!down && b.down) {
        b.down = false;
        if (!b.long_fired && now - b.pressed_ms >= 30 && !register_input(now)) {
            on_short();
        }
    }
}

void poll_battery(uint32_t now)
{
    static uint32_t last_poll = 0;
    if (last_poll != 0 && now - last_poll < kBatteryPollMs) {
        return;
    }
    last_poll = now;

    uint32_t mv = analogReadMilliVolts(PIN_BAT_VOLT) * 2;
    int percent = mv > 4300 ? -1 : constrain(((int)mv - 3300) * 100 / 900, 0, 100);
    if (percent != battery_percent) {
        battery_percent = percent;
        dirty = true;
    }
}

} // namespace

void setup()
{
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    pinMode(PIN_BUTTON_BOOT, INPUT_PULLUP);
    pinMode(PIN_BUTTON_KEY, INPUT_PULLUP);
    Serial.begin(115200);

    lcd.init();
    lcd.setRotation(1); // landscape, USB port on the right
    lcd.setBrightness(kBrightness);

    frame.setColorDepth(16);
    frame.setPsram(psramFound());
    frame.createSprite(kWidth, kHeight);

    start_game(0);
    last_input_ms = millis();
}

void loop()
{
    uint32_t now = millis();

    handle_touch(now);
    poll_button(
        key_button, now, [now] { dirty |= bb_game_undo(&game); }, [] {});
    poll_button(
        boot_button, now, [] { start_game(profile_index + 1); },
        [] {
            bb_game_reset(&game);
            dirty = true;
        });
    poll_battery(now);

    if (bb_game_tick(&game, now)) {
        dirty = true;
    }

    if (!dimmed && now - last_input_ms >= kDimAfterMs) {
        dimmed = true;
        lcd.setBrightness(kDimBrightness);
    }

    if (dirty) {
        dirty = false;
        ui_draw(frame, game, step, battery_percent);
        frame.pushSprite(0, 0);
    }

    delay(10);
}
