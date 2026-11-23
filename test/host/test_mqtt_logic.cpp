#include "minitest.h"
#include "mqtt_logic.h"

TEST(radiation_status_level_zero_and_negative_are_normal) {
    CHECK_EQ(radiation_status_level(0.0f), 0);
    CHECK_EQ(radiation_status_level(-0.0001f), 0);
    CHECK_EQ(radiation_status_level(-1.0f), 0);
    CHECK_EQ(radiation_status_level(-100000.0f), 0);
}

TEST(radiation_status_level_below_each_threshold) {
    CHECK_EQ(radiation_status_level(1499.0f), 0);
    CHECK_EQ(radiation_status_level(9999.0f), 1);
    CHECK_EQ(radiation_status_level(79999.0f), 2);
    CHECK_EQ(radiation_status_level(349999.0f), 3);
}

TEST(radiation_status_level_exact_thresholds_move_to_next_level) {
    CHECK_EQ(radiation_status_level(1500.0f), 1);
    CHECK_EQ(radiation_status_level(10000.0f), 2);
    CHECK_EQ(radiation_status_level(80000.0f), 3);
    CHECK_EQ(radiation_status_level(350000.0f), 4);
}

TEST(radiation_status_level_above_highest_threshold_is_alarm) {
    CHECK_EQ(radiation_status_level(350001.0f), 4);
    CHECK_EQ(radiation_status_level(1000000.0f), 4);
}

TEST(radiation_status_label_plain_has_exact_strings_for_each_level) {
    CHECK_STR_EQ(radiation_status_label(0.0f), "Normal");
    CHECK_STR_EQ(radiation_status_label(1500.0f), "Erhoht");
    CHECK_STR_EQ(radiation_status_label(10000.0f), "Deutlich erhoht");
    CHECK_STR_EQ(radiation_status_label(80000.0f), "Hohe Strahlung");
    CHECK_STR_EQ(radiation_status_label(350000.0f), "STRAHLUNGSALARM");
}

TEST(radiation_status_label_html_has_exact_strings_for_each_level) {
    CHECK_STR_EQ(radiation_status_label_html(0.0f), "Normal");
    CHECK_STR_EQ(radiation_status_label_html(1500.0f), "Erh&ouml;ht");
    CHECK_STR_EQ(radiation_status_label_html(10000.0f), "Deutlich erh&ouml;ht");
    CHECK_STR_EQ(radiation_status_label_html(80000.0f), "Hohe Strahlung");
    CHECK_STR_EQ(radiation_status_label_html(350000.0f), "&#9762; STRAHLUNGSALARM");
}

TEST(radiation_status_color_has_exact_strings_for_each_level) {
    CHECK_STR_EQ(radiation_status_color(0.0f), "#2a7a2a");
    CHECK_STR_EQ(radiation_status_color(1500.0f), "#c8a000");
    CHECK_STR_EQ(radiation_status_color(10000.0f), "#c85000");
    CHECK_STR_EQ(radiation_status_color(80000.0f), "#b00000");
    CHECK_STR_EQ(radiation_status_color(350000.0f), "#5a0000");
}

TEST(ring_buffer_is_empty_initially) {
    RingBuffer<int, 3> rb;
    CHECK(rb.empty());
    CHECK_EQ(rb.count(), 0);
    CHECK_EQ(rb.dropped(), 0);
}

TEST(ring_buffer_push_front_pop_are_fifo) {
    RingBuffer<int, 3> rb;
    CHECK_EQ(rb.push(1), false);
    CHECK_EQ(rb.push(2), false);
    CHECK_EQ(rb.push(3), false);
    CHECK_EQ(rb.count(), 3);

    CHECK_EQ(rb.front(), 1);
    rb.pop();
    CHECK_EQ(rb.front(), 2);
    rb.pop();
    CHECK_EQ(rb.front(), 3);
    rb.pop();
    CHECK(rb.empty());
    CHECK_EQ(rb.count(), 0);
    CHECK_EQ(rb.dropped(), 0);
}

TEST(ring_buffer_full_push_drops_oldest_and_counts_drops) {
    RingBuffer<int, 3> rb;
    CHECK_EQ(rb.push(1), false);
    CHECK_EQ(rb.push(2), false);
    CHECK_EQ(rb.push(3), false);

    CHECK_EQ(rb.push(4), true);
    CHECK_EQ(rb.dropped(), 1);
    CHECK_EQ(rb.count(), 3);
    CHECK_EQ(rb.front(), 2);

    CHECK_EQ(rb.push(5), true);
    CHECK_EQ(rb.dropped(), 2);
    CHECK_EQ(rb.count(), 3);
    CHECK_EQ(rb.front(), 3);

    rb.pop();
    CHECK_EQ(rb.front(), 4);
    rb.pop();
    CHECK_EQ(rb.front(), 5);
    rb.pop();
    CHECK(rb.empty());
}

TEST(ring_buffer_pop_on_empty_buffer_changes_nothing) {
    RingBuffer<int, 3> rb;
    rb.pop();
    CHECK(rb.empty());
    CHECK_EQ(rb.count(), 0);
    CHECK_EQ(rb.dropped(), 0);

    CHECK_EQ(rb.push(7), false);
    rb.pop();
    CHECK(rb.empty());
}

TEST(ring_buffer_pop_empty_does_not_change_state) {
    RingBuffer<int, 3> rb;
    rb.pop();
    CHECK(rb.empty());
    CHECK_EQ(rb.count(), 0);
    CHECK_EQ(rb.dropped(), 0);

    CHECK_EQ(rb.push(7), false);
    rb.pop();
    rb.pop();
    CHECK(rb.empty());
    CHECK_EQ(rb.count(), 0);
    CHECK_EQ(rb.dropped(), 0);
}

TEST(ring_buffer_wraparound_keeps_fifo_without_drops) {
    RingBuffer<int, 3> rb;
    int in = 1;
    int expected_out = 1;

    for (int i = 0; i < 20; ++i) {
        CHECK_EQ(rb.push(in++), false);
        if (rb.count() == 3) {
            CHECK_EQ(rb.front(), expected_out++);
            rb.pop();
        }
    }

    CHECK_EQ(rb.dropped(), 0);
    while (!rb.empty()) {
        CHECK_EQ(rb.front(), expected_out++);
        rb.pop();
    }
}

TEST(format_radiation_record_live_with_set_ts_contains_expected_json_fields) {
    char out[512];
    MqttRecord r{};
    r.seq = 42;
    r.ms = 0;
    r.ts = 1700000000;
    r.cpm = 250;
    r.dose_nsvph = 50.0f;
    r.accu_nsvph = 1.5f;
    r.counts = 1234;
    r.sample_s = 10.0f;
    r.hv_pulses = 7;
    r.have_thp = false;

    int n = format_radiation_record(out, sizeof(out), r, false, 0, 0);
    CHECK(n > 0);
    CHECK(n < (int)sizeof(out));

    CHECK_CONTAINS(out, "\"ts\":\"2023-11-14T22:13:20Z\"");
    CHECK_CONTAINS(out, "\"seq\":42,");
    CHECK_CONTAINS(out, "\"CPM\":250,");
    CHECK_CONTAINS(out, "\"Strahlenstatus\":\"Normal\"");
    CHECK_EQ(out[n - 1], '}');
    CHECK(strstr(out, "alter_s") == nullptr);
}

TEST(format_radiation_record_outputs_ts_null_when_clock_was_not_set) {
    char out[512];
    MqttRecord r{};
    r.seq = 1;
    r.cpm = 10;
    r.dose_nsvph = 50.0f;

    int n = format_radiation_record(out, sizeof(out), r, false, 0, 0);
    CHECK(n > 0);
    CHECK_CONTAINS(out, "\"ts\":null");
}

TEST(format_radiation_record_derives_ts_from_age_when_record_ts_is_zero) {
    char out[512];
    MqttRecord r{};
    r.seq = 99;
    r.ms = 10000UL;
    r.ts = 0;
    r.cpm = 25;
    r.dose_nsvph = 50.0f;

    int n = format_radiation_record(out, sizeof(out), r, false, 70000UL, 1700000060);
    CHECK(n > 0);
    CHECK_CONTAINS(out, "\"ts\":\"2023-11-14T22:13:20Z\"");
}

TEST(format_radiation_record_backlog_contains_age_without_environment_values_when_no_thp) {
    char out[512];
    MqttRecord r{};
    r.seq = 100;
    r.ms = 0UL;
    r.ts = 0;
    r.cpm = 33;
    r.dose_nsvph = 50.0f;
    r.have_thp = false;

    int n = format_radiation_record(out, sizeof(out), r, true, 12000UL, 0);
    CHECK(n > 0);
    CHECK_CONTAINS(out, "\"alter_s\":12");
    CHECK(strstr(out, "Temperatur_C") == nullptr);
    CHECK(strstr(out, "Luftfeuchte_pct") == nullptr);
    CHECK(strstr(out, "Luftdruck_hPa") == nullptr);
}

TEST(format_radiation_record_backlog_with_thp_contains_environment_fields) {
    char out[512];
    MqttRecord r{};
    r.seq = 101;
    r.ms = 0UL;
    r.ts = 0;
    r.cpm = 34;
    r.dose_nsvph = 50.0f;
    r.have_thp = true;
    r.temperature = 1.23f;
    r.humidity = 45.6f;
    r.pressure_hpa = 1013.2f;

    int n = format_radiation_record(out, sizeof(out), r, true, 12000UL, 0);
    CHECK(n > 0);
    CHECK_CONTAINS(out, "\"alter_s\":12");
    CHECK_CONTAINS(out, "\"Temperatur_C\":1.23");
    CHECK_CONTAINS(out, "\"Luftfeuchte_pct\":45.6");
    CHECK_CONTAINS(out, "\"Luftdruck_hPa\":1013.2");
}

TEST(format_radiation_record_millis_overflow_age_is_computed_correctly_on_32_bit_unsigned_long) {
    if (sizeof(unsigned long) == 4) {
        char out[512];
        MqttRecord r{};
        r.seq = 7;
        r.ms = 0xFFFFF000UL;
        r.ts = 0;
        r.cpm = 100;
        r.dose_nsvph = 50.0f;
        r.have_thp = false;

        int n = format_radiation_record(out, sizeof(out), r, true, 0x00001000UL, 0);
        CHECK(n > 0);
        CHECK_CONTAINS(out, "\"alter_s\":8");
    }
}

TEST(format_radiation_record_small_buffer_does_not_overflow_and_returns_truncated_length) {
    char out[20];
    MqttRecord r{};
    r.seq = 123456789;
    r.cpm = 987654321u;
    r.dose_nsvph = 50.0f;

    int n = format_radiation_record(out, sizeof(out), r, false, 0, 0);
    CHECK(n >= (int)sizeof(out));
    CHECK_EQ(out[19], '\0');
}

TEST(build_base_topic_plain_prefix) {
    char out[64];
    const char *expected = "duogeiger/dev/";
    int n = build_base_topic(out, sizeof(out), "duogeiger", "dev");
    CHECK_STR_EQ(out, expected);
    CHECK_EQ(n, (int)strlen(expected));
}

TEST(build_base_topic_strips_one_leading_and_one_trailing_slash) {
    char out[64];
    const char *expected = "duogeiger/dev/";
    int n = build_base_topic(out, sizeof(out), "/duogeiger/", "dev");
    CHECK_STR_EQ(out, expected);
    CHECK_EQ(n, (int)strlen(expected));
}

TEST(build_base_topic_strips_surrounding_whitespace) {
    char out[64];
    const char *expected = "duogeiger/dev/";
    int n = build_base_topic(out, sizeof(out), "  duogeiger  ", "dev");
    CHECK_STR_EQ(out, expected);
    CHECK_EQ(n, (int)strlen(expected));
}

TEST(build_base_topic_empty_prefix) {
    char out[64];
    const char *expected = "dev/";
    int n = build_base_topic(out, sizeof(out), "", "dev");
    CHECK_STR_EQ(out, expected);
    CHECK_EQ(n, (int)strlen(expected));
}

TEST(build_base_topic_null_prefix) {
    char out[64];
    const char *expected = "dev/";
    int n = build_base_topic(out, sizeof(out), nullptr, "dev");
    CHECK_STR_EQ(out, expected);
    CHECK_EQ(n, (int)strlen(expected));
}

TEST(build_base_topic_keeps_inner_slash) {
    char out[64];
    const char *expected = "a/b/dev/";
    int n = build_base_topic(out, sizeof(out), "a/b", "dev");
    CHECK_STR_EQ(out, expected);
    CHECK_EQ(n, (int)strlen(expected));
}
