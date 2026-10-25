// BattleBuddy prototype for the LilyGO T-Display-S3 Touch.
//
// Buttons (board held landscape):
//   BOOT (GPIO0), short press: open/close the menu (game, players, layout, flip, restart, sleep)
//   BOOT, hold 1 s:            sleep
//   KEY (GPIO14):              undo (closes the menu when it is open); wakes from sleep
//   Circle below the screen:   open/close the menu
// The game survives sleep (kept in RTC memory) but not a power cut or reset.

#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

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
constexpr uint32_t kSleepAfterMs = 10 * 60 * 1000;
constexpr uint32_t kBatteryPollMs = 5000;
constexpr uint32_t kToastMs = 1500;
constexpr uint8_t kBrightness = 200;
constexpr uint8_t kDimBrightness = 20;

// Raw touch y at or beyond this is the circle key below the visible screen.
constexpr int kRawHomeKeyY = 320;

constexpr uint32_t kSavedMagic = 0xBB0B0002;

struct Saved {
    uint32_t magic;
    uint8_t profile_index;
    int16_t step;
    Layout layout;
    bool flipped;
    bb_game_t game;
};

RTC_DATA_ATTR Saved saved;
bb_game_t &game = saved.game;

Display lcd;
LGFX_Sprite frame(&lcd);

bool menu_open = false;
bool dirty = true;

uint32_t last_input_ms = 0;
bool dimmed = false;
int battery_percent = -1; // -1 = on USB / unknown

char toast_text[40];
uint32_t toast_until = 0;

struct Button {
    explicit Button(uint8_t p) : pin(p) {}

    uint8_t pin;
    bool down = false;
    bool long_fired = false;
    uint32_t pressed_ms = 0;
};

Button boot_button(PIN_BUTTON_BOOT);
Button key_button(PIN_BUTTON_KEY);

bool touch_down = false;
UiAction touch_action;
uint32_t touch_start_ms = 0;
uint32_t last_repeat_ms = 0;

UiState ui_state()
{
    UiState s;
    s.game = &game;
    s.step = saved.step;
    s.battery_percent = battery_percent;
    s.layout = saved.layout;
    s.flipped = saved.flipped;
    s.menu = menu_open;
    s.toast = toast_until ? toast_text : nullptr;
    return s;
}

void show_toast(const char *msg, uint32_t now)
{
    snprintf(toast_text, sizeof(toast_text), "%s", msg);
    toast_until = now + kToastMs;
    if (toast_until == 0) {
        toast_until = 1;
    }
    dirty = true;
}

void start_game(size_t index, uint8_t players)
{
    saved.profile_index = (uint8_t)(index % bb_profile_count());
    bb_game_init(&game, bb_profile_at(saved.profile_index), players);
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

void go_to_sleep()
{
    Serial.println("sleeping");
    bb_game_commit(&game);
    saved.magic = kSavedMagic;

    lcd.setBrightness(0);
    lcd.sleep();
    digitalWrite(PIN_POWER_ON, LOW);

    // Don't wake straight back up if KEY is still held.
    while (digitalRead(PIN_BUTTON_KEY) == LOW) {
        delay(10);
    }
    rtc_gpio_pullup_en((gpio_num_t)PIN_BUTTON_KEY);
    rtc_gpio_pulldown_dis((gpio_num_t)PIN_BUTTON_KEY);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BUTTON_KEY, 0);
    esp_deep_sleep_start();
}

void undo(uint32_t now)
{
    bb_entry_t e;
    if (!bb_game_undo(&game, &e)) {
        show_toast("Nothing to undo", now);
        return;
    }

    char msg[40];
    int len = snprintf(msg, sizeof(msg), "Undo");
    if (game.players > 1) {
        len += snprintf(msg + len, sizeof(msg) - len, " %s", ui_player_label(e.player));
    }
    len += snprintf(msg + len, sizeof(msg) - len, " %+d", e.delta);
    if (game.profile->kind_count > 0) {
        snprintf(msg + len, sizeof(msg) - len, " %s", game.profile->kinds[e.kind]);
    }
    show_toast(msg, now);
}

void perform(const UiAction &a, uint32_t now)
{
    switch (a.type) {
    case UiAction::Adjust:
        if (bb_game_adjust(&game, a.player, a.kind, a.delta, now) != 0) {
            dirty = true;
        }
        break;
    case UiAction::ToggleStep:
        saved.step = saved.step == 1 ? 5 : 1;
        dirty = true;
        break;
    case UiAction::MenuGame:
        start_game(saved.profile_index + 1, game.players);
        break;
    case UiAction::MenuPlayers:
        start_game(saved.profile_index, game.players == 1 ? 2 : 1);
        break;
    case UiAction::MenuLayout:
        saved.layout = (Layout)(((int)saved.layout + 1) % (int)Layout::Count);
        dirty = true;
        break;
    case UiAction::MenuFlip:
        saved.flipped = !saved.flipped;
        lcd.setRotation(saved.flipped ? 3 : 1);
        dirty = true;
        break;
    case UiAction::MenuRestart:
        bb_game_reset(&game);
        menu_open = false;
        show_toast("New game", now);
        break;
    case UiAction::MenuSleep:
        go_to_sleep();
        break;
    case UiAction::MenuClose:
        menu_open = false;
        dirty = true;
        break;
    case UiAction::None:
        break;
    }
}

void toggle_menu()
{
    menu_open = !menu_open;
    dirty = true;
}

void handle_touch(uint32_t now)
{
    lgfx::touch_point_t tp;
    if (lcd.getTouchRaw(&tp, 1) == 0) {
        touch_down = false;
        return;
    }

    if (touch_down) {
        // Held press: auto-repeat where the action allows it.
        last_input_ms = now;
        if (touch_action.repeat && now - touch_start_ms >= kRepeatDelayMs &&
            now - last_repeat_ms >= kRepeatIntervalMs) {
            last_repeat_ms = now;
            perform(touch_action, now);
        }
        return;
    }

    // New press.
    touch_down = true;
    touch_action = UiAction();
    touch_start_ms = now;
    last_repeat_ms = now;
    Serial.printf("touch raw x=%d y=%d\n", tp.x, tp.y);

    if (register_input(now)) {
        return; // this press only woke the screen
    }
    if (tp.y >= kRawHomeKeyY) {
        toggle_menu();
        return;
    }

    lcd.convertRawXY(&tp, 1);
    touch_action = ui_hit(ui_state(), tp.x, tp.y);
    perform(touch_action, now);
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
        Serial.printf("button %d long\n", b.pin);
        if (!register_input(now)) {
            on_long();
        }
    } else if (!down && b.down) {
        b.down = false;
        if (!b.long_fired && now - b.pressed_ms >= 30) {
            Serial.printf("button %d short\n", b.pin);
            if (!register_input(now)) {
                on_short();
            }
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

    bool woke = esp_reset_reason() == ESP_RST_DEEPSLEEP && saved.magic == kSavedMagic &&
                saved.profile_index < bb_profile_count();
    if (woke) {
        rtc_gpio_deinit((gpio_num_t)PIN_BUTTON_KEY);
        pinMode(PIN_BUTTON_KEY, INPUT_PULLUP);
        game.profile = bb_profile_at(saved.profile_index);
        // The KEY press that woke us must not also count as an undo.
        if (digitalRead(PIN_BUTTON_KEY) == LOW) {
            key_button.down = true;
            key_button.long_fired = true;
        }
    } else {
        saved.step = 1;
        saved.layout = Layout::SideYouLeft;
        saved.flipped = false;
        start_game(0, 1);
    }
    saved.magic = 0; // only trusted again after the next sleep

    lcd.init();
    lcd.setRotation(saved.flipped ? 3 : 1); // landscape; unflipped = USB port on the right
    lcd.setBrightness(kBrightness);

    frame.setColorDepth(16);
    frame.setPsram(psramFound());
    frame.createSprite(kWidth, kHeight);

    last_input_ms = millis();
}

void loop()
{
    uint32_t now = millis();

    handle_touch(now);
    poll_button(
        key_button, now,
        [now] {
            if (menu_open) {
                toggle_menu();
            } else {
                undo(now);
            }
        },
        [] {});
    poll_button(boot_button, now, [] { toggle_menu(); }, [] { go_to_sleep(); });
    poll_battery(now);

    if (bb_game_tick(&game, now)) {
        dirty = true;
    }
    if (toast_until && (int32_t)(now - toast_until) >= 0) {
        toast_until = 0;
        dirty = true;
    }

    uint32_t idle = now - last_input_ms;
    if (!dimmed && idle >= kDimAfterMs) {
        dimmed = true;
        lcd.setBrightness(kDimBrightness);
    }
    if (idle >= kSleepAfterMs) {
        go_to_sleep();
    }

    if (dirty) {
        dirty = false;
        ui_draw(frame, ui_state());
        frame.pushSprite(0, 0);
    }

    delay(10);
}
