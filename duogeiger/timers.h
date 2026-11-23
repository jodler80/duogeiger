// Timer-Setup für Recharge- und Audio-Interrupts.
void setup_recharge_timer(void (*isr_recharge)(), int period_us);
// Audio-Timer: periodischer ISR für Sound/LED; period_us [µs].
void setup_audio_timer(void (*isr_audio)(), int period_us);

