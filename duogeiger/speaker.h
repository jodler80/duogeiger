// speaker / sound related code
// also handles the onboard LED, which lights up while speaker ticks.

#ifndef _SPEAKER_H_
#define _SPEAKER_H_

// Initialisiert Piezo-Timer und optional LED-Tick.
void setup_speaker(bool playSound, bool led_tick, bool speaker_tick);
void tick_enable(bool enable);
void tick(bool high);
// Spielt den Alarmton (zwei Töne hoch/tief); die LED bleibt dabei unverändert.
void alarm();

#endif // _SPEAKER_H_
