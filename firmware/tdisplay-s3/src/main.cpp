// BattleBuddy prototype for the LilyGO T-Display-S3 Touch.
//
// Buttons (board held landscape):
//   BOOT (GPIO0), short press: open/close the menu (closes the dice)
//   BOOT, hold 1 s:            sleep
//   KEY (GPIO14):              undo; rolls on the dice screen; closes the menu; wakes from sleep
//   Circle below the screen:   open/close the dice
// Menu: game (opens the game picker), players (1-4, or two teams), layout,
// flip, best-of match, timer, restart, dice. Yu-Gi-Oh! opens a number pad on
// tap; Digimon shows a shared memory gauge. The game, match and timer survive sleep (RTC memory and the
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
constexpr uint32_t kTapMaxMs = 700;       // longer presses without repeat don't count as taps
constexpr uint32_t kButtonGuardMs = 250;  // ignore touches this soon after a physical button
constexpr uint32_t kRepeatIntervalMs = 150;
constexpr uint32_t kLongPressMs = 1000;
constexpr uint32_t kDimAfterMs = 30000;
constexpr uint32_t kSleepAfterMs = 10 * 60 * 1000;
constexpr uint32_t kBatteryPollMs = 5000;
constexpr uint32_t kToastMs = 1500;
constexpr uint8_t kBrightness = 200;
constexpr uint8_t kDimBrightness = 20;
constexpr uint32_t kRollMs = 900;      // tumble before showing a roll
constexpr uint32_t kPopMs = 350;       // landing effect after a roll
constexpr uint32_t kCelebrateMs = 1100; // longer effect for the top or bottom face
constexpr uint32_t kAnimFrameMs = 25;  // redraw rate while animating
constexpr uint32_t kBurstMs = 260;     // game picker: formats bursting out of a game tile
constexpr int32_t kMaxEntry = 32000;   // number pad limit (fits the counter's range)
constexpr uint32_t kSpinMs = 380;      // d20 counter: spin to the new face

// Raw touch y at or beyond this is the circle key below the visible screen.
constexpr int kRawHomeKeyY = 320;

constexpr uint32_t kSavedMagic = 0xBB0B0007;

constexpr uint8_t kBestOfChoices[] = {0, 1, 3, 5}; // 0 = off
constexpr uint8_t kTimerChoices[] = {0, 30, 50, 60}; // minutes, 0 = off

struct Saved {
    uint32_t magic;
    uint8_t profile_index;
    int16_t step;
    Layout layout;
    bool flipped;
    bool vivid;
    bool spindown;
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
int menu_page = 0;
bool end_prompt = false;
bool dirty = true;

uint16_t spin[BB_MAX_PLAYERS] = {1000, 1000, 1000, 1000};
int8_t spin_dir[BB_MAX_PLAYERS] = {1, 1, 1, 1};
uint32_t spin_start_ms[BB_MAX_PLAYERS] = {};

bool dice_open = false;
DiceState dice;

bool picker_open = false;
PickerState picker;
uint32_t burst_start_ms = 0;

bool entry_open = false;
EntryState entry;
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
bool touch_fired = false;     // the action already ran (auto-repeat, wake, circle key)
bool touch_cancelled = false; // a physical button was involved; ignore this touch
uint32_t last_button_ms = 0;
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
    s.vivid = saved.vivid;
    s.menu_page = menu_page;
    s.spindown = saved.spindown;
    s.spin = spin;
    s.spin_dir = spin_dir;
    s.end_prompt = end_prompt;
    s.menu = menu_open;
    s.toast = toast_until ? toast_text : nullptr;
    s.match = saved.best_of && game.players <= 2 ? &saved.match : nullptr;
    s.best_of = saved.best_of;
    s.timer_minutes = saved.timer_minutes;
    s.timer_running = saved.timer_minutes > 0;
    s.timer_left_s = timer_left_s();
    s.dice = dice_open ? &dice : nullptr;
    s.picker = picker_open ? &picker : nullptr;
    s.entry = entry_open ? &entry : nullptr;
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
    const bb_profile_t *profile = bb_profile_at(saved.profile_index);
    if (profile->style == BB_STYLE_GAUGE) {
        players = 2; // a shared gauge always has two sides
    }
    bb_game_init(&game, profile, players);
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
            uint8_t sides = bb_die_at(dice.die)->sides;
            bool extreme = sides >= 4 && (dice.value == sides || dice.value == 1);
            uint32_t len = extreme ? kCelebrateMs : kPopMs;
            dice.pop = e >= len ? 1000 : (uint16_t)(e * 1000 / len);
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

void adjust(uint8_t player, uint8_t kind, int16_t delta, uint32_t now)
{
    int16_t applied = bb_game_adjust(&game, player, kind, delta, now);
    if (applied != 0) {
        dirty = true;
        if (player < BB_MAX_PLAYERS) {
            spin[player] = 0;
            spin_dir[player] = applied > 0 ? 1 : -1;
            spin_start_ms[player] = now;
        }
        if (saved.match_start == 0) {
            saved.match_start = now_s(); // the timer starts with the first change
        }
    }
}

void perform(const UiAction &a, uint32_t now)
{
    switch (a.type) {
    case UiAction::Adjust:
        adjust(a.player, a.kind, a.delta, now);
        break;
    case UiAction::PickFamily: {
        size_t idx[4];
        size_t n = bb_family_profiles(a.kind, idx, 4);
        if (n == 1) {
            start_game(idx[0], game.players);
            picker_open = false;
        } else {
            picker.family = a.kind;
            picker.origin = a.kind;
            picker.anim = 0;
            burst_start_ms = now;
        }
        dirty = true;
        break;
    }
    case UiAction::PickProfile:
        start_game(a.kind, game.players); // clamped to the new game's limit
        picker_open = false;
        dirty = true;
        break;
    case UiAction::PickerBack:
        picker.family = -1;
        dirty = true;
        break;
    case UiAction::PickerClose:
        picker_open = false;
        dirty = true;
        break;
    case UiAction::GaugeSet:
        adjust(0, 0, (int16_t)(a.delta - game.value[0]), now);
        break;
    case UiAction::GaugePass: {
        // Passing leaves the opponent 3 memory on their side.
        int16_t target = bb_gauge_owner(&game) == 0 ? 3 : -3;
        adjust(0, 0, (int16_t)(target - game.value[0]), now);
        break;
    }
    case UiAction::OpenEntry:
        entry_open = true;
        entry.player = a.player;
        entry.amount = 0;
        dirty = true;
        break;
    case UiAction::EntryDigit: {
        int32_t next = a.kind == 10 ? entry.amount * 100 : entry.amount * 10 + a.kind;
        if (next <= kMaxEntry) {
            entry.amount = next;
            dirty = true;
        }
        break;
    }
    case UiAction::EntryDelete:
        entry.amount /= 10;
        dirty = true;
        break;
    case UiAction::EntryApply:
        if (entry.amount > 0) {
            adjust(entry.player, 0, (int16_t)(a.delta * entry.amount), now);
            bb_game_commit(&game); // a typed amount is one history entry on its own
        }
        entry_open = false;
        dirty = true;
        break;
    case UiAction::EntryHalve:
        adjust(entry.player, 0, (int16_t)-(game.value[entry.player] / 2), now);
        bb_game_commit(&game);
        entry_open = false;
        dirty = true;
        break;
    case UiAction::EntryClose:
        entry_open = false;
        dirty = true;
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
        menu_open = false;
        picker_open = true;
        picker.family = -1;
        picker.anim = 1000;
        dirty = true;
        break;
    case UiAction::MenuPlayers:
        start_game(saved.profile_index, game.players < game.profile->max_players ? game.players + 1 : 1);
        break;
    case UiAction::MenuLayout:
        saved.layout = ui_next_layout(game, saved.layout);
        dirty = true;
        break;
    case UiAction::MenuMore:
        menu_page = 1;
        dirty = true;
        break;
    case UiAction::MenuBack:
        menu_page = 0;
        dirty = true;
        break;
    case UiAction::MenuCounter:
        saved.spindown = !saved.spindown;
        dirty = true;
        break;
    case UiAction::MenuSleep:
        go_to_sleep();
        break;
    case UiAction::MenuUndo:
        menu_open = false;
        undo(now);
        dirty = true;
        break;
    case UiAction::MenuEnd:
        menu_open = false;
        end_prompt = true;
        dirty = true;
        break;
    case UiAction::DeclareWinner: {
        char msg[40];
        uint8_t game_no = saved.match.games + 1;
        bb_match_record(&saved.match, a.player);
        const char *who = ui_player_label(game, a.player);
        const char *verb = strcmp(who, "YOU") == 0 ? "win" : "wins";
        if (bb_match_winner(&saved.match) >= 0 && saved.match.best_of > 1) {
            snprintf(msg, sizeof(msg), "%s %s the match %d-%d", who, verb, saved.match.wins[0],
                     saved.match.wins[1]);
            new_match();
        } else {
            snprintf(msg, sizeof(msg), "%s %s game %d", who, verb, game_no);
            if (bb_match_winner(&saved.match) >= 0) {
                new_match();
            }
        }
        bb_game_reset(&game);
        end_prompt = false;
        show_toast(msg, now);
        break;
    }
    case UiAction::EndRestart:
        bb_game_reset(&game);
        end_prompt = false;
        show_toast("New game", now);
        break;
    case UiAction::EndCancel:
        end_prompt = false;
        dirty = true;
        break;
    case UiAction::MenuLook:
        saved.vivid = !saved.vivid;
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
        menu_page = 0;
        dirty = true;
        break;
    case UiAction::None:
        break;
    }
}

void toggle_menu()
{
    menu_page = 0;
    if (end_prompt) {
        end_prompt = false;
    } else if (picker_open && picker.family >= 0) {
        picker.family = -1; // BOOT steps back from a game's formats to the games
    } else if (picker_open || entry_open || dice_open) {
        picker_open = entry_open = dice_open = false; // BOOT backs out to the counter
    } else {
        menu_open = !menu_open;
    }
    dirty = true;
}

void toggle_dice()
{
    menu_open = picker_open = entry_open = false;
    dice_open = !dice_open;
    dirty = true;
}

// Taps act when the finger lifts, so a touch can be cancelled: if a physical
// button is pressed during it (a thumb grazing the screen while reaching for
// the button), or it starts right after one, nothing happens. +/- still
// repeat while held.
void handle_touch(uint32_t now)
{
    lgfx::touch_point_t tp;
    if (lcd.getTouchRaw(&tp, 1) == 0) {
        if (touch_down && !touch_fired && !touch_cancelled && now - touch_start_ms <= kTapMaxMs) {
            perform(touch_action, now);
        }
        touch_down = false;
        return;
    }

    if (touch_down) {
        // Held press: auto-repeat where the action allows it.
        last_input_ms = now;
        if (boot_button.down || key_button.down) {
            touch_cancelled = true;
        }
        if (touch_action.repeat && !touch_cancelled && now - touch_start_ms >= kRepeatDelayMs &&
            now - last_repeat_ms >= kRepeatIntervalMs) {
            last_repeat_ms = now;
            touch_fired = true;
            perform(touch_action, now);
        }
        return;
    }

    // New press.
    touch_down = true;
    touch_fired = false;
    touch_cancelled = boot_button.down || key_button.down || now - last_button_ms < kButtonGuardMs;
    touch_action = UiAction();
    touch_start_ms = now;
    last_repeat_ms = now;
    Serial.printf("touch raw x=%d y=%d%s\n", tp.x, tp.y, touch_cancelled ? " (ignored)" : "");

    if (touch_cancelled) {
        return;
    }
    if (register_input(now)) {
        touch_fired = true; // this press only woke the screen
        return;
    }
    if (tp.y >= kRawHomeKeyY) {
        touch_fired = true;
        toggle_dice();
        return;
    }

    lcd.convertRawXY(&tp, 1);
    touch_action = ui_hit(ui_state(), tp.x, tp.y);
}

// Calls on_short when released before kLongPressMs, on_long once when held that long.
template <typename Short, typename Long>
void poll_button(Button &b, uint32_t now, Short on_short, Long on_long)
{
    bool down = digitalRead(b.pin) == LOW;

    if (down != b.down) {
        last_button_ms = now;
        if (touch_down) {
            touch_cancelled = true;
        }
    }

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
        saved.vivid = false;
        saved.spindown = false;
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
            if (menu_open || picker_open || entry_open || end_prompt) {
                menu_open = picker_open = entry_open = end_prompt = false;
                menu_page = 0;
                dirty = true;
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

    // d20 counter: spin each changed player's die to its new face.
    for (int p = 0; p < BB_MAX_PLAYERS; p++) {
        if (spin[p] < 1000) {
            uint32_t e = now - spin_start_ms[p];
            spin[p] = e >= kSpinMs ? 1000 : (uint16_t)(e * 1000 / kSpinMs);
            if (saved.spindown && now - anim_frame_ms >= kAnimFrameMs) {
                dirty = true;
            }
        }
    }
    if (dirty && saved.spindown) {
        anim_frame_ms = now;
    }

    if (picker_open && picker.anim < 1000) {
        uint32_t e = now - burst_start_ms;
        picker.anim = e >= kBurstMs ? 1000 : (uint16_t)(e * 1000 / kBurstMs);
        dirty = true;
    }
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
