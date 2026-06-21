// speaker / sound related code
// also handles the onboard LED, which lights up while speaker ticks.

#include <Arduino.h>
#include <driver/mcpwm.h>
#include <soc/gpio_struct.h>
#include <soc/mcpwm_struct.h>

#include "speaker.h"
#include "timers.h"

#define PIN_SPEAKER_OUTPUT_P 12
#define PIN_SPEAKER_OUTPUT_N 0
// Onboard LED — GPIO 25 on Heltec WiFi LoRa 32 V2.
// Do NOT use LED_BUILTIN here: it can be overridden by a library macro to a wrong value,
// and calling gpio_set_level() with an invalid pin from an ISR crashes the device.
#define PIN_LED 25

// shall the speaker / LED "tick"?
static volatile bool speaker_tick, led_tick;  // current state
static bool speaker_tick_wanted, led_tick_wanted;  // state wanted by user

// MUX (mutexes used for mutual exclusive access to isr variables)
portMUX_TYPE mux_audio = portMUX_INITIALIZER_UNLOCKED;

volatile int *isr_audio_sequence = NULL;
volatile int *isr_tick_sequence = NULL;
volatile int *isr_sequence = NULL;  // currently played sequence

static int tick_sequence[8];
static int alarm_sequence[12] = {
  // "high_Pitch"
  3000000, 1, -1, 400,  // frequency_mHz, volume, LED (-1 = don't touch), duration_ms
  // "low_Pitch"
  1000000, 1, -1, 400,
  // "off"
  0, 0, -1, 0 // duration_ms = 0 --> END
};


// hw timer period and microseconds -> periods conversion
#define PERIOD_DURATION_US 1000
#define PERIODS(us) ((us) / PERIOD_DURATION_US)

// Precomputed timer clock (Hz), set in setup_speaker() after mcpwm_init().
// All MCPWM API functions (mcpwm_start, mcpwm_stop, mcpwm_set_frequency, etc.) reside in Flash
// (IROM, 0x4010xxxx) and must NOT be called from an ISR when the flash cache is disabled
// (e.g. during IotWebConf NVS writes). These IRAM_ATTR helpers write the MCPWM hardware
// registers directly — they live in DRAM (memory-mapped peripherals) and are always reachable.
static uint32_t s_timer_clk_hz;

static void IRAM_ATTR mcpwm_start_direct() {
  MCPWM0.timer[0].timer_cfg1.timer_mod   = 1; // up-counter (restores mode after stop)
  MCPWM0.timer[0].timer_cfg1.timer_start = 2; // run continuously
}

static void IRAM_ATTR mcpwm_stop_direct() {
  MCPWM0.timer[0].timer_cfg1.timer_mod = 0; // freeze
}

static void IRAM_ATTR mcpwm_set_freq_direct(uint32_t freq_hz) {
  MCPWM0.timer[0].timer_cfg0.timer_period           = s_timer_clk_hz / freq_hz;
  MCPWM0.timer[0].timer_cfg0.timer_period_upmethod  = 0; // immediate
}

// DUTY_MODE_0 on OPR_A: active-high PWM (high at TEZ, low at TEA)
static void IRAM_ATTR mcpwm_duty_a_normal() {
  MCPWM0.operators[0].gen_force.gen_a_cntuforce_mode = 0; // release force
  MCPWM0.operators[0].generator[0].gen_utez          = 2; // high at TEZ↑
  MCPWM0.operators[0].generator[0].gen_utea          = 1; // low  at TEA↑
}

// DUTY_MODE_1 on OPR_B: active-low PWM / inverted (low at TEZ, high at TEA)
static void IRAM_ATTR mcpwm_duty_b_inverted() {
  MCPWM0.operators[0].gen_force.gen_b_cntuforce_mode = 0; // release force
  MCPWM0.operators[0].generator[1].gen_utez          = 1; // low  at TEZ↑
  MCPWM0.operators[0].generator[1].gen_utea          = 2; // high at TEA↑
}

// Force OPR_A permanently high (piezo idle: no current)
static void IRAM_ATTR mcpwm_force_a_high() {
  MCPWM0.operators[0].gen_force.gen_cntuforce_upmethod = 0; // immediate
  MCPWM0.operators[0].gen_force.gen_a_cntuforce_mode   = 2; // force high
}

// Force OPR_B permanently low (piezo idle: no current)
static void IRAM_ATTR mcpwm_force_b_low() {
  MCPWM0.operators[0].gen_force.gen_cntuforce_upmethod = 0; // immediate
  MCPWM0.operators[0].gen_force.gen_b_cntuforce_mode   = 1; // force low
}

void IRAM_ATTR isr_audio() {
  // this code is periodically called by a timer hw interrupt, always same period.
  // we need to decide internally whether we actually want to do something.
  //
  // note: this is implemented like it is because dynamically reprogramming the hw timer
  // to a different period would require us to call library functions like timerAlarmWrite
  // which are **not** in IRAM (but in flash) and doing that can lead to spurious fatal
  // exceptions like "Cache disabled but cached memory region accessed".
  static unsigned int current = 0;  // current period counter
  static unsigned int next = PERIODS(1000);  // periods to next sequencer execution
  if (++current < next)
    return;  // nothing to do yet

  // we reached "next", so we execute the sequencer:
  current = 0;

  // tone and tick generation, also led blinking
  int frequency_mHz = 0, volume = 0, led = 0, duration_ms = 0;  // init avoids compiler warning
  static bool playing_audio = false, playing_tick = false;

  // fetch next tone / next led state
  portENTER_CRITICAL_ISR(&mux_audio);
  if (!isr_sequence) {
    if (isr_audio_sequence) {
      isr_sequence = isr_audio_sequence;
      playing_audio = true;
    } else if (isr_tick_sequence) {
      isr_sequence = isr_tick_sequence;
      playing_tick = true;
    }
  }
  volatile int *p = isr_sequence;
  if (p) {
    frequency_mHz = *p++;
    volume = *p++;
    led = *p++;
    duration_ms = *p++;
    isr_sequence = p;
  }
  portEXIT_CRITICAL_ISR(&mux_audio);

  if (!p)
    return;  // nothing to do
  // do not access *p below here, p might point to uninitialized memory after the sequence array!

  // Direct register writes replace all mcpwm_*() API calls (which live in Flash/IROM and
  // must not be called from ISR when flash cache is disabled). See helpers above.
  if (frequency_mHz > 0) { // speaker on
    if (volume >= 1) {
      // high volume - MCPWM A/B outputs generate inverted signals
      mcpwm_duty_a_normal();
      mcpwm_duty_b_inverted();
    } else {
      // low volume - do MCPWM on A, keep B permanently low
      mcpwm_duty_a_normal();
      mcpwm_force_b_low();
    }
    mcpwm_set_freq_direct(frequency_mHz / 1000);
    mcpwm_start_direct();
  } else if (frequency_mHz == 0) {  // speaker off
    mcpwm_stop_direct();
    // keep A high and B low (piezo idle: no current flowing)
    mcpwm_force_a_high();
    mcpwm_force_b_low();
  }
  // frequency_mHz == -1 -> don't touch pwm/speaker

  if (led >= 0) {  // led == -1 can be used as "don't touch LED"
    if (led)
      GPIO.out_w1ts = (1UL << PIN_LED);
    else
      GPIO.out_w1tc = (1UL << PIN_LED);
  }

  if (duration_ms > 0) {
    next = PERIODS(duration_ms * 1000);
  } else {
    // duration == 0 marks the end of the sequence to play
    portENTER_CRITICAL_ISR(&mux_audio);
    isr_sequence = NULL;
    if (playing_tick) {
      isr_tick_sequence = NULL;
      playing_tick = false;
    } else if (playing_audio) {
      isr_audio_sequence = NULL;
      playing_audio = false;
    }
    portEXIT_CRITICAL_ISR(&mux_audio);
    next = PERIODS(1000);
  }
}

void IRAM_ATTR tick(bool high) {
  // high true: "tick" -> high frequency tick and LED blink
  // high false: "tock" -> lower frequency tock, no LED
  // called from ISR!
  portENTER_CRITICAL_ISR(&mux_audio);

  int *sequence;

  // we expect speaker_tick or led_tick to change at any time,
  // thus check it here and generate different sequences:
  if (speaker_tick || led_tick) {
    sequence = tick_sequence;
    // "on"
    sequence[0] = speaker_tick ? (high ? 5000000 : 1000000) : -1;  // frequency_mHz
    sequence[1] = 1;  // volume
    sequence[2] = led_tick ? (high ? 1 : -1) : -1;  // LED
    sequence[3] = 4;  // duration_ms
    // "off"
    sequence[4] = speaker_tick ? 0 : -1;
    sequence[5] = 0;
    sequence[6] = led_tick ? (high ? 0 : -1) : -1;
    sequence[7] = 0;  // END
  } else
    sequence = NULL;

  isr_tick_sequence = sequence;
  portEXIT_CRITICAL_ISR(&mux_audio);
}

void tick_enable(bool enable) {
  // true -> bring ticking into the state desired by user
  // false -> disable ticking (e.g. when accessing flash)
  if (enable) {
    led_tick = led_tick_wanted;
    speaker_tick = speaker_tick_wanted;
  } else {
    led_tick = false;
    speaker_tick = false;
  }
}

void alarm() {
  // play alarm sound, called from normal code (not ISR)
  portENTER_CRITICAL(&mux_audio);
  isr_audio_sequence = alarm_sequence;
  portEXIT_CRITICAL(&mux_audio);
}

void play(int *sequence) {
  // play a tone sequence, called from normal code (not ISR)
  portENTER_CRITICAL(&mux_audio);
  isr_audio_sequence = sequence;
  portEXIT_CRITICAL(&mux_audio);
}

#define TONE(f, v, led, t) {int(f * 0.75), v, led, int(t * 85)}

void setup_speaker(bool playSound, bool _led_tick, bool _speaker_tick) {
  pinMode(PIN_LED, OUTPUT);

  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PIN_SPEAKER_OUTPUT_P);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, PIN_SPEAKER_OUTPUT_N);

  mcpwm_config_t pwm_config;
  pwm_config.frequency = 1000;
  // set duty cycles to 50% (and never modify them again!)
  pwm_config.cmpr_a = 50.0;
  pwm_config.cmpr_b = 50.0;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;  // active high duty
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);

  // Precompute timer clock for ISR-safe frequency writes (prescales are set by mcpwm_init above).
  s_timer_clk_hz = 160000000UL
    / (MCPWM0.clk_cfg.clk_prescale + 1)
    / (MCPWM0.timer[0].timer_cfg0.timer_prescale + 1);

  setup_audio_timer(isr_audio, PERIOD_DURATION_US);

  tick_enable(false);  // no ticking while we play melody / init sound

  static int init[][4] = {
    TONE(0, 0, 0, 0),        // speaker off, led off, end
  };
  play((int *)init);

  static int melody[][4] = {
    TONE(1174659, 1, -1, 2),  // D
    TONE(0, 0, -1, 2),        // ---
    TONE(1318510, 1, -1, 2),  // E
    TONE(0, 0, -1, 2),        // ---
    TONE(1479978, 1, -1, 2),  // Fis
    TONE(0, 0, -1, 2),        // ---
    TONE(1567982, 1, -1, 4),  // G
    TONE(1174659, 1, -1, 2),  // D
    TONE(1318510, 1, -1, 2),  // E
    TONE(1174659, 1, -1, 4),  // D
    TONE(987767, 1, -1, 2),   // H
    TONE(1046502, 1, -1, 2),  // C
    TONE(987767, 1, -1, 4),   // H
    TONE(987767, 0, -1, 4),   // H
    TONE(0, 0, -1, 2),        // ---
    TONE(0, 0, -1, 0),        // speaker off, end
  };
  if (playSound)
    play((int *)melody);

  led_tick_wanted = _led_tick;
  speaker_tick_wanted = _speaker_tick;
  tick_enable(true);
}
