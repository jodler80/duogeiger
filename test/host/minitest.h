// Minimal test framework for host (PC) tests of the DuoGeiger pure logic.
// Usage:
//   TEST(name) { CHECK(x == 1); CHECK_EQ(a, b); CHECK_STR_EQ(s, "abc"); CHECK_CONTAINS(s, "ab"); }
// main() is provided when MINITEST_MAIN is defined in exactly one .cpp file.

#ifndef MINITEST_H
#define MINITEST_H

#include <stdio.h>
#include <string.h>

namespace minitest {
typedef void (*TestFn)();
struct Entry { const char *name; TestFn fn; };
inline Entry *entries() { static Entry e[512]; return e; }
inline int &entry_count() { static int n = 0; return n; }
inline int &failures() { static int n = 0; return n; }
inline const char *&current() { static const char *c = ""; return c; }
struct Registrar {
  Registrar(const char *name, TestFn fn) { entries()[entry_count()++] = Entry{name, fn}; }
};
inline void fail(const char *file, int line, const char *msg) {
  printf("FAIL %s (%s:%d): %s\n", current(), file, line, msg);
  failures()++;
}
}  // namespace minitest

#define TEST(name) \
  static void name(); \
  static minitest::Registrar minitest_reg_##name(#name, name); \
  static void name()

#define CHECK(cond) \
  do { if (!(cond)) minitest::fail(__FILE__, __LINE__, #cond); } while (0)

#define CHECK_EQ(a, b) \
  do { if (!((a) == (b))) minitest::fail(__FILE__, __LINE__, #a " == " #b); } while (0)

#define CHECK_STR_EQ(a, b) \
  do { const char *sa_ = (a), *sb_ = (b); \
       if (strcmp(sa_, sb_) != 0) { \
         char m_[600]; snprintf(m_, sizeof(m_), "%s == %s  (got \"%s\", expected \"%s\")", #a, #b, sa_, sb_); \
         minitest::fail(__FILE__, __LINE__, m_); } } while (0)

#define CHECK_CONTAINS(hay, needle) \
  do { const char *h_ = (hay), *n_ = (needle); \
       if (strstr(h_, n_) == NULL) { \
         char m_[600]; snprintf(m_, sizeof(m_), "\"%s\" contains \"%s\"", h_, n_); \
         minitest::fail(__FILE__, __LINE__, m_); } } while (0)

#ifdef MINITEST_MAIN
int main() {
  int before;
  for (int i = 0; i < minitest::entry_count(); i++) {
    minitest::current() = minitest::entries()[i].name;
    before = minitest::failures();
    minitest::entries()[i].fn();
    if (minitest::failures() == before)
      printf("ok   %s\n", minitest::entries()[i].name);
  }
  printf("\n%d tests, %d failed checks\n", minitest::entry_count(), minitest::failures());
  return minitest::failures() ? 1 : 0;
}
#endif

#endif  // MINITEST_H
