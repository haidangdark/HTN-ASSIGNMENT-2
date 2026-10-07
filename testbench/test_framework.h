/**
 * @file    test_framework.h
 * @brief   Tiny self-contained unit test framework (no external dependency).
 *
 * Usage:
 *     static void tc_example(void) { TEST_ASSERT_EQ_INT(4, 2 + 2); }
 *     int main(int argc, char **argv) {
 *         TF_BEGIN(argc, argv);
 *         RUN_TEST("TC01", tc_example, "description");
 *         return TF_END();
 *     }
 *
 * Command line of the produced executable:
 *     test_wm.exe            run every test
 *     test_wm.exe TC12       run only tests whose ID contains "TC12"
 *     test_wm.exe --no-color disable ANSI colours (useful for report files)
 */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

typedef struct {
    int         total;
    int         passed;
    int         failed;
    int         current_failed;
    long        assertions;
    const char *filter;          /* run only test IDs containing this text */
    int         color;
} tf_state_t;

extern tf_state_t g_tf;

#define TF_GREEN   (g_tf.color ? "\x1b[32m" : "")
#define TF_RED     (g_tf.color ? "\x1b[31m" : "")
#define TF_YELLOW  (g_tf.color ? "\x1b[33m" : "")
#define TF_RESET   (g_tf.color ? "\x1b[0m"  : "")

/* ---------------------------------------------------------------------- */
#define TF_FAIL_MSG(...)                                                  \
    do {                                                                  \
        printf("\n        %sASSERT FAILED%s %s:%d: ",                     \
               TF_RED, TF_RESET, __FILE__, __LINE__);                     \
        printf(__VA_ARGS__);                                              \
        g_tf.current_failed = 1;                                          \
        return;                                                           \
    } while (0)

#define TEST_ASSERT(cond)                                                 \
    do {                                                                  \
        g_tf.assertions++;                                                \
        if (!(cond)) { TF_FAIL_MSG("%s", #cond); }                        \
    } while (0)

#define TEST_ASSERT_TRUE(cond)   TEST_ASSERT(cond)
#define TEST_ASSERT_FALSE(cond)  TEST_ASSERT(!(cond))

#define TEST_ASSERT_EQ_INT(expected, actual)                              \
    do {                                                                  \
        long long e_ = (long long)(expected);                             \
        long long a_ = (long long)(actual);                               \
        g_tf.assertions++;                                                \
        if (e_ != a_) {                                                   \
            TF_FAIL_MSG("%s == %s (expected %lld, got %lld)",             \
                        #expected, #actual, e_, a_);                      \
        }                                                                 \
    } while (0)

/** Compare two enum values and print their names on failure. */
#define TEST_ASSERT_EQ_ENUM(expected, actual, name_fn)                    \
    do {                                                                  \
        int e_ = (int)(expected);                                         \
        int a_ = (int)(actual);                                           \
        g_tf.assertions++;                                                \
        if (e_ != a_) {                                                   \
            TF_FAIL_MSG("%s (expected %s, got %s)", #actual,              \
                        name_fn(e_), name_fn(a_));                        \
        }                                                                 \
    } while (0)

/* ---------------------------------------------------------------------- */
#define TF_BEGIN(argc_, argv_)                                            \
    do {                                                                  \
        int i_;                                                           \
        memset(&g_tf, 0, sizeof(g_tf));                                   \
        g_tf.color = 1;                                                   \
        for (i_ = 1; i_ < (argc_); i_++) {                                \
            if (strcmp((argv_)[i_], "--no-color") == 0) g_tf.color = 0;   \
            else g_tf.filter = (argv_)[i_];                               \
        }                                                                 \
    } while (0)

#define RUN_TEST(id, fn, desc)                                            \
    do {                                                                  \
        if (g_tf.filter && !strstr((id), g_tf.filter)) break;             \
        g_tf.total++;                                                     \
        g_tf.current_failed = 0;                                          \
        printf("  [%-5s] %-64s ", (id), (desc));                          \
        fflush(stdout);                                                   \
        fn();                                                             \
        if (g_tf.current_failed) {                                        \
            g_tf.failed++;                                                \
            printf("\n          => %sFAIL%s\n", TF_RED, TF_RESET);        \
        } else {                                                          \
            g_tf.passed++;                                                \
            printf("%sPASS%s\n", TF_GREEN, TF_RESET);                     \
        }                                                                 \
    } while (0)

#define TF_SECTION(title)                                                 \
    do {                                                                  \
        if (!g_tf.filter)                                                 \
            printf("\n%s== %s ==%s\n", TF_YELLOW, (title), TF_RESET);     \
    } while (0)

static inline int tf_end(void)
{
    printf("\n------------------------------------------------------------------------------\n");
    printf(" TOTAL: %d   PASSED: %d   FAILED: %d   (assertions checked: %ld)\n",
           g_tf.total, g_tf.passed, g_tf.failed, g_tf.assertions);
    printf("------------------------------------------------------------------------------\n");
    if (g_tf.failed) {
        printf("%s>>> SOME TESTS FAILED <<<%s\n", TF_RED, TF_RESET);
    } else {
        printf("%s>>> ALL TESTS PASSED <<<%s\n", TF_GREEN, TF_RESET);
    }
    return g_tf.failed ? 1 : 0;
}
#define TF_END() tf_end()

#endif /* TEST_FRAMEWORK_H */
