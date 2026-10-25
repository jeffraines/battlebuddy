// BattleBuddy prototype for the LilyGO T-Display-S3 Touch.
//
// Buttons (board held landscape):
//   BOOT (GPIO0), short press: open/close the menu (closes the dice)
//   BOOT, hold 1 s:            sleep
//   KEY (GPIO14):              undo; rolls on the dice screen; closes the menu; wakes from sleep
//   Circle below the screen:   open/close the dice
// Menu: game, players (1-4, or two teams), layout, flip, best-of match, timer,
// restart, dice. The game, match and timer survive sleep (RTC memory and the
// RTC clock) but not a power cut or reset.

#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <time.h>

#include "bb/dice.h"
#include "bb/game.h"
#include "bb/match.h"
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
constexpr uint32_t kRollMs = 900;      // tumble before showing a roll
constexpr uint32_t kPopMs = 300;       // landing effect after a roll
constexpr uint32_t kAnimFrameMs = 25;  // redraw rate while animating

// Raw touch y at or beyond this is the circle key below the visible screen.
constexpr int kRawHomeKeyY = 320;

constexpr uint32_t kSavedMagic = 0xBB0B0004;

constexpr uint8_t kBestOfChoices[] = {0, 1, 3, 5}; // 0 = off
constexpr uint8_t kTimerChoices[] = {0, 30, 50, 60}; // minutes, 0 = off

struct Saved {
    uint32_t magic;
    uint8_t profile_index;
    int16_t step;
    Layout layout;
    bool flipped;
    uint8_t best_of;       // 0 = match tracking off
    uint8_t timer_minutes; // 0 = no timer
    uint32_t match_start;  // RTC seconds when the timer started; 0 = not yet
    bb_match_t match;
    bb_game_t game;
};

RTC_DATA_ATTR Saved saved;
bb_game_t &game = saved.game;

Display lcd;
LGFX_Sprite frame(&lcd);

bool menu_open = false;
bool dirty = true;

bool dice_open = false;
DiceState dice;
uint32_t roll_start_ms = 0;
uint32_t roll_face_ms = 0;  // when the shown face last changed
uint32_t land_ms = 0;
uint32_t anim_frame_ms = 0;

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

// Seconds on the RTC clock, which keeps running through deep sleep.
uint32_t now_s()
{
    return (uint32_t)time(nullptr);
}

int32_t timer_left_s()
{
    int32_t total = saved.timer_minutes * 60;
    if (saved.match_start == 0) {
        return total;
    }
    return total - (int32_t)(now_s() - saved.match_start);
}

void new_match()
{
    bb_match_init(&saved.match, saved.best_of ? saved.best_of : 1);
    saved.match_start = 0;
}

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
    s.match = saved.best_of && game.players <= 2 ? &saved.match : nullptr;
    s.best_of = saved.best_of;
    s.timer_minutes = saved.timer_minutes;
    s.timer_running = saved.timer_minutes > 0;
    s.timer_left_s = timer_left_s();
    s.dice = dice_open ? &dice : nullptr;
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
    new_match();
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

uint32_t hardware_random(void *)
{
    return esp_random();
}

void start_roll(uint32_t now)
{
    if (dice.rolling) {
        return;
    }
    dice.rolling = true;
    dice.anim = 0;
    roll_start_ms = now;
    roll_face_ms = 0;
    anim_frame_ms = 0;
    dirty = true;
}

// Tumbles with random faces that change more and more slowly, then lands on
// the real result and plays a short pop.
void update_roll(uint32_t now)
{
    if (!dice.rolling) {
        if (dice.pop < 1000) {
            uint32_t e = now - land_ms;
            dice.pop = e >= kPopMs ? 1000 : (uint16_t)(e * 1000 / kPopMs);
            if (now - anim_frame_ms >= kAnimFrameMs || dice.pop == 1000) {
                anim_frame_ms = now;
                dirty = true;
            }
        }
        return;
    }
    uint8_t sides = bb_die_at(dice.die)->sides;
    uint32_t elapsed = now - roll_start_ms;
    if (elapsed >= kRollMs) {
        dice.value = bb_dice_roll(sides, hardware_random, nullptr);
        dice.rolling = false;
        dice.anim = 1000;
        dice.pop = 0;
        land_ms = now;
        memmove(dice.history + 1, dice.history, DiceState::kHistory - 1);
        memmove(dice.history_die + 1, dice.history_die, DiceState::kHistory - 1);
        dice.history[0] = dice.value;
        dice.history_die[0] = dice.die;
        if (dice.history_count < DiceState::kHistory) {
            dice.history_count++;
        }
        dirty = true;
        return;
    }

    dice.anim = (uint16_t)(elapsed * 1000 / kRollMs);
    // Faces change every 40 ms at first, slowing to every ~200 ms near the end.
    uint32_t face_interval = 40 + 160 * dice.anim / 1000 * dice.anim / 1000;
    if (roll_face_ms == 0 || now - roll_face_ms >= face_interval) {
        roll_face_ms = now;
        dice.value = bb_dice_roll(sides, hardware_random, nullptr);
    }
    if (now - anim_frame_ms >= kAnimFrameMs) {
        anim_frame_ms = now;
        dirty = true;
    }
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
        len += snprintf(msg + len, sizeof(msg) - len, " %s", ui_player_label(game, e.player));
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
            if (saved.match_start == 0) {
                saved.match_start = now_s(); // the timer starts with the first change
            }
        }
        break;
    case UiAction::NextGame: {
        int winner = bb_game_winner(&game);
        if (winner < 0) {
            break;
        }
        bb_match_record(&saved.match, (uint8_t)winner);
        if (bb_match_winner(&saved.match) >= 0) {
            new_match();
        }
        bb_game_reset(&game);
        dirty = true;
        break;
    }
    case UiAction::MenuMatch: {
        size_t i = 0;
        while (i < sizeof(kBestOfChoices) && kBestOfChoices[i] != saved.best_of) {
            i++;
        }
        saved.best_of = kBestOfChoices[(i + 1) % sizeof(kBestOfChoices)];
        bb_match_init(&saved.match, saved.best_of ? saved.best_of : 1); // keeps the timer
        dirty = true;
        break;
    }
    case UiAction::MenuTimer: {
        size_t i = 0;
        while (i < sizeof(kTimerChoices) && kTimerChoices[i] != saved.timer_minutes) {
            i++;
        }
        saved.timer_minutes = kTimerChoices[(i + 1) % sizeof(kTimerChoices)];
        saved.match_start = 0; // restarts with the next change
        dirty = true;
        break;
    }
    case UiAction::ToggleStep:
        saved.step = saved.step == 1 ? 5 : 1;
        dirty = true;
        break;
    case UiAction::MenuGame:
        start_game(saved.profile_index + 1, game.players); // clamped to the new game's limit
        break;
    case UiAction::MenuPlayers:
        start_game(saved.profile_index, game.players < game.profile->max_players ? game.players + 1 : 1);
        break;
    case UiAction::MenuLayout:
        saved.layout = ui_next_layout(game, saved.layout);
        dirty = true;
        break;
    case UiAction::MenuFlip:
        saved.flipped = !saved.flipped;
        lcd.setRotation(saved.flipped ? 3 : 1);
        dirty = true;
        break;
    case UiAction::MenuRestart:
        bb_game_reset(&game);
        new_match();
        menu_open = false;
        show_toast("New match", now);
        break;
    case UiAction::MenuDice:
        menu_open = false;
        dice_open = true;
        dirty = true;
        break;
    case UiAction::DiceSelect:
        if (!dice.rolling && a.kind != dice.die) {
            dice.die = a.kind;
            dice.value = 0;
            dirty = true;
        }
        break;
    case UiAction::DiceRoll:
        start_roll(now);
        break;
    case UiAction::DiceClose:
        dice_open = false;
        dirty = true;
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
    if (dice_open) {
        dice_open = false; // BOOT backs out of the dice to the counter
    } else {
        menu_open = !menu_open;
    }
    dirty = true;
}

void toggle_dice()
{
    menu_open = false;
    dice_open = !dice_open;
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
        toggle_dice();
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
        saved.best_of = 1;
        saved.timer_minutes = 0;
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
            } else if (dice_open) {
                start_roll(now);
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
    update_roll(now);
    if (toast_until && (int32_t)(now - toast_until) >= 0) {
        toast_until = 0;
        dirty = true;
    }

    // Redraw once a second while the timer is counting.
    static int32_t shown_timer_s = 0;
    if (saved.timer_minutes && saved.match_start) {
        int32_t left = timer_left_s();
        if (left != shown_timer_s) {
            shown_timer_s = left;
            dirty = true;
        }
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
