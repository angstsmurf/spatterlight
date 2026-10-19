/* UTF-16 on the app pipe: connect.c's print, unprint, line preload and
   line input reply, checked against a fake app on two pipes. Built by
   run_stream_test.sh; connect.c's other dependencies resolve lazily
   (-undefined dynamic_lookup) and are never called. */
#include "connect.c"

void (*gli_autosave_request_hook)(void) = NULL;
char *gli_story_title = NULL;
void (*gli_unregister_arr)(void *array, glui32 len, char *typecode, gidispatch_rock_t objrock) = NULL;

static window_t stubwin;
window_t *gli_window_for_peer(int peer) { return &stubwin; }

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static int to_app[2], from_app[2];

/* Read back one message glkimp sent; payload into p */
static void take(struct message *m, uint16_t *p)
{
    read_fully(to_app[0], m, sizeof *m);
    if (m->len) read_fully(to_app[0], p, m->len);
}
static void reply(int a1)
{
    struct message m = {0}; m.cmd = OKAY; m.a1 = a1;
    write_fully(from_app[1], &m, sizeof m);
}

int main(void)
{
    uint16_t e[2]; struct message m; uint16_t p[64];
    pipe(to_app); pipe(from_app); sendfd = to_app[1]; readfd = from_app[0];

    CHECK(utf16_encode(0x41, e) == 1 && e[0] == 0x41);
    CHECK(utf16_encode(0x1F600, e) == 2 && e[0] == 0xD83D && e[1] == 0xDE00);
    CHECK(utf16_encode(0x10FFFF, e) == 2 && e[0] == 0xDBFF && e[1] == 0xDFFF);
    CHECK(utf16_encode(0xD800, e) == 1 && e[0] == 0xFFFD);
    CHECK(utf16_encode(0x110000, e) == 1 && e[0] == 0xFFFD);

    /* line input back from the app */
    uint16_t in1[] = {0xD83D, 0xDE00, 'A', 0xE9, 0xDC00, 0xD83D};
    glui32 u[8]; unsigned char c[8];
    CHECK(store_line_input(in1, 6, 1, u, 8) == 5);
    CHECK(u[0] == 0x1F600 && u[1] == 'A' && u[2] == 0xE9 && u[3] == 0xFFFD && u[4] == 0xFFFD);
    CHECK(store_line_input(in1, 6, 0, c, 8) == 5);
    CHECK(c[0] == '?' && c[1] == 'A' && c[2] == 0xE9 && c[3] == '?' && c[4] == '?');
    CHECK(store_line_input(in1, 6, 1, u, 1) == 1 && u[0] == 0x1F600);

    /* print: a pair travels in one PRINT message */
    win_print(3, 'x', 0); win_print(3, 0x1F600, 0); win_flush();
    take(&m, p);
    CHECK(m.cmd == PRINT && m.len == 6 && p[0] == 'x' && p[1] == 0xD83D && p[2] == 0xDE00);

    /* unprint: UTF-16 on the wire, characters back */
    glui32 s[] = {0x1F600, 'b'};
    reply(3); CHECK(win_unprint(3, s, 2) == 2);
    take(&m, p);
    CHECK(m.cmd == UNPRINT && m.len == 6 && p[0] == 0xD83D && p[1] == 0xDE00 && p[2] == 'b');
    reply(0); CHECK(win_unprint(3, s, 2) == 0); take(&m, p);

    /* preloaded line text */
    glui32 pre[] = {'h', 0x1F600, 0xD800};
    stubwin.line_request_uni = TRUE;
    win_initline(3, 10, 3, pre); take(&m, p);
    CHECK(m.cmd == INITLINE && m.a2 == 10 && m.len == 8 && p[0] == 'h' && p[1] == 0xD83D && p[2] == 0xDE00 && p[3] == 0xFFFD);
    char npre[] = { (char)0x68, (char)0xE9, 0 };
    stubwin.line_request_uni = FALSE;
    win_initline(3, 10, 2, npre); take(&m, p);
    CHECK(m.len == 4 && p[0] == 'h' && p[1] == 0xE9);

    printf(fails ? "connect_utf16_test: %d failed\n" : "connect_utf16_test: ok\n", fails);
    return fails != 0;
}
