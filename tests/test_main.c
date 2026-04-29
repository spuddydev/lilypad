#include "exec.h"
#include "hosts.h"
#include "state.h"

#include <stdio.h>
#include <string.h>

static int failed = 0;

#define CHECK(cond) do {                                              \
    if (!(cond)) {                                                    \
        fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failed++;                                                     \
    }                                                                 \
} while (0)

static void test_state_encode_round_trip(void) {
    static const char *cases[] = {
        "simple",
        "with space",
        "comma,delim",
        "equals=sign",
        "hash#mark",
        "percent%char",
        "all the,things=in #one%line",
        "",
    };
    char enc[256], dec[256];
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        state_encode(enc, sizeof enc, cases[i]);
        state_decode(dec, sizeof dec, enc);
        CHECK(strcmp(cases[i], dec) == 0);
    }
}

static void test_state_encode_special_chars(void) {
    char enc[64];
    state_encode(enc, sizeof enc, " ,=#%");
    CHECK(strcmp(enc, "%20%2C%3D%23%25") == 0);
}

static void test_state_decode_passthrough(void) {
    char dst[64];
    state_decode(dst, sizeof dst, "no-encoding-here");
    CHECK(strcmp(dst, "no-encoding-here") == 0);
}

static void test_is_reserved_nick(void) {
    CHECK(is_reserved_nick("add"));
    CHECK(is_reserved_nick("config"));
    CHECK(is_reserved_nick("_complete"));
    CHECK(is_reserved_nick("-anything"));
    CHECK(is_reserved_nick(NULL));
    CHECK(!is_reserved_nick("bob"));
    CHECK(!is_reserved_nick("server1"));
    CHECK(!is_reserved_nick("addd"));
}

static void test_auto_session_name(void) {
    char sessions[16][128];
    char out[128];

    auto_session_name(sessions, 0, out, sizeof out);
    CHECK(strcmp(out, "untitled") == 0);

    strcpy(sessions[0], "untitled");
    auto_session_name(sessions, 1, out, sizeof out);
    CHECK(strcmp(out, "untitled-1") == 0);

    strcpy(sessions[0], "untitled");
    strcpy(sessions[1], "untitled-1");
    auto_session_name(sessions, 2, out, sizeof out);
    CHECK(strcmp(out, "untitled-2") == 0);

    strcpy(sessions[0], "main");
    auto_session_name(sessions, 1, out, sizeof out);
    CHECK(strcmp(out, "untitled") == 0);
}

static void test_shell_sq_escape(void) {
    char out[64];
    shell_sq_escape(out, sizeof out, "plain");
    CHECK(strcmp(out, "plain") == 0);

    shell_sq_escape(out, sizeof out, "it's");
    CHECK(strcmp(out, "it'\\''s") == 0);

    shell_sq_escape(out, sizeof out, "''");
    CHECK(strcmp(out, "'\\'''\\''") == 0);
}

int main(void) {
    test_state_encode_round_trip();
    test_state_encode_special_chars();
    test_state_decode_passthrough();
    test_is_reserved_nick();
    test_auto_session_name();
    test_shell_sq_escape();

    if (failed) {
        fprintf(stderr, "%d test(s) failed\n", failed);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
