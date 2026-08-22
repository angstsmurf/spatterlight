/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Hard-coded map_t / map_view_t for map.xml Main page after REVEAL.
 * No .taf / interpreter — geometry and fog-of-war are fixed tables.
 */

#include <cstring>

#include "mapsvg_fixture.h"

/* DIR_* order matches map_dirs[] / DirectionsEnum. */
enum {
  N = DIR_N, E = DIR_E, S = DIR_S, W = DIR_W,
  UP = DIR_UP, DN = DIR_DOWN, IN = DIR_IN, OUT = DIR_OUT,
  NE = DIR_NE, SE = DIR_SE, SW = DIR_SW, NW = DIR_NW
};

struct link_def {
  int dir;
  int dst_anchor;
  const char *dest;
  int dotted;
  int duplex;
  int n_mids;
  int mid0_x, mid0_y;
  int mid1_x, mid1_y;
};

struct node_def {
  const char *key;
  const char *label;
  int x, y, z, w, h;
  const link_def *links;
  int n_links;
};

/* Midpoint storage for links that need it (Hub North, NoLink South). */
static map_pt_t hub_n_mids[2] = { { 2, -4, 0 }, { -2, -8, 0 } };
static map_pt_t nolink_s_mids[2] = { { -14, 4, 0 }, { -18, 8, 0 } };

static map_link_t hub_links[12];
static map_link_t north_links[2];
static map_link_t skewed_links[1];
static map_link_t cellar_links[4];
static map_link_t attic_links[2];
static map_link_t closet_links[3];
static map_link_t outside_links[1];
static map_link_t nolink_links[2];
static map_link_t sky_links[1];
static map_link_t pit_links[1];
static map_link_t vault_links[1];
static map_link_t disagree_links[1];
static map_link_t wide_links[3];
static map_link_t loop_links[2];
static map_link_t door_links[1];
static map_link_t latch_links[1];

static map_node_t nodes[20];
static map_page_t page;
static map_t g_map;

/* Post-REVEAL Main-page seen set (Hide=1 rooms stay unseen to the map). */
static const char *const seen_keys[] = {
  "LocationHub", "LocationNorth", "LocationSkewed", "LocationCellar",
  "LocationAttic", "LocationCloset", "LocationNoLink", "LocationDisagree",
  "LocationWide", "LocationOneWay", "LocationLoop", "LocationDoor",
  "LocationLatch",
  NULL
};

static const char *const names[][2] = {
  { "LocationHub", "Hub" },
  { "LocationNorth", "North" },
  { "LocationSkewed", "Skewed" },
  { "LocationCellar", "Cellar" },
  { "LocationAttic", "Attic" },
  { "LocationCloset", "Closet" },
  { "LocationOutside", "Outside" },
  { "LocationNoLink", "No Link" },
  { "LocationSky", "Sky" },
  { "LocationPit", "Pit" },
  { "LocationVault", "Vault" },
  { "LocationHiddenSeen", "Hidden Seen" },
  { "LocationHiddenUnseen", "Hidden Unseen" },
  { "LocationDisagree", "Disagree" },
  { "LocationWide", "Wide Room" },
  { "LocationOneWay", "One Way" },
  { "LocationLoop", "Loop" },
  { "LocationDoor", "Door Room" },
  { "LocationLatch", "Latch Room" },
  { "LocationOffgrid", "Offgrid" },
  { "LocationOffPageFree", "Off Page Free" },
  { "LocationOffPagePass", "Off Page Pass" },
  { "LocationOffPageFail", "Off Page Fail" },
  { NULL, NULL }
};

/* Movement graph used by exit_dest (restrictions already applied for REVEAL). */
struct exit_def {
  const char *from;
  int dir;
  const char *dest;           /* NULL = no usable route */
};

static const exit_def exits[] = {
  /* Hub */
  { "LocationHub", N, "LocationNorth" },
  { "LocationHub", E, "LocationSkewed" },
  { "LocationHub", S, "LocationCellar" },
  { "LocationHub", DN, "LocationCellar" },
  { "LocationHub", W, "LocationNoLink" },
  { "LocationHub", UP, "LocationAttic" },
  { "LocationHub", IN, "LocationCloset" },
  { "LocationHub", OUT, "LocationOutside" },
  { "LocationHub", SE, "LocationDisagree" },
  { "LocationHub", NE, "LocationOffPageFree" },
  /* Gate=1: NW open, SW blocked */
  { "LocationHub", NW, "LocationOffPagePass" },
  { "LocationHub", SW, NULL },
  /* North */
  { "LocationNorth", S, "LocationHub" },
  { "LocationNorth", E, "LocationOffgrid" },
  /* Skewed */
  { "LocationSkewed", W, "LocationHub" },
  /* Cellar — door open after REVEAL */
  { "LocationCellar", N, "LocationHub" },
  { "LocationCellar", UP, "LocationHub" },
  { "LocationCellar", DN, "LocationPit" },
  { "LocationCellar", E, "LocationDoor" },
  /* Attic */
  { "LocationAttic", DN, "LocationHub" },
  { "LocationAttic", UP, "LocationSky" },
  /* Closet */
  { "LocationCloset", OUT, "LocationHub" },
  { "LocationCloset", IN, "LocationVault" },
  { "LocationCloset", E, "LocationOneWay" },
  /* Outside (unseen) */
  { "LocationOutside", IN, "LocationHub" },
  /* NoLink */
  { "LocationNoLink", E, "LocationHub" },
  { "LocationNoLink", S, "LocationWide" },
  /* Wide — latch still open, never blocked */
  { "LocationWide", N, "LocationNoLink" },
  { "LocationWide", E, "LocationLoop" },
  { "LocationWide", S, "LocationLatch" },
  /* Loop */
  { "LocationLoop", W, "LocationWide" },
  { "LocationLoop", N, "LocationLoop" },
  /* Door */
  { "LocationDoor", W, "LocationCellar" },
  /* Latch */
  { "LocationLatch", N, "LocationWide" },
  /* Disagree */
  { "LocationDisagree", NW, "LocationHub" },
  { NULL, 0, NULL }
};

/* Directions that have been blocked at least once this session. */
static const exit_def ever_blocked_exits[] = {
  { "LocationCellar", E, NULL },
  { NULL, 0, NULL }
};

static void
fill_link (map_link_t *lk, int dir, int dst_anchor, const char *dest,
           int dotted, int duplex, map_pt_t *mids, int n_mids)
{
  lk->dir = dir;
  lk->dst_anchor = dst_anchor;
  lk->dest = dest;
  lk->dotted = dotted;
  lk->duplex = duplex;
  lk->badge = 0;
  lk->has_compass_twin = 0;
  lk->compass_twin = -1;
  lk->mids = mids;
  lk->n_mids = n_mids;
}

static void
fill_link_twin (map_link_t *lk, int dir, int dst_anchor, const char *dest,
                int dotted, int duplex, int twin)
{
  fill_link (lk, dir, dst_anchor, dest, dotted, duplex, NULL, 0);
  lk->has_compass_twin = 1;
  lk->compass_twin = twin;
}

static void
init_links (void)
{
  /* Hub */
  fill_link (&hub_links[0], N, S, "LocationNorth", 0, 1, hub_n_mids, 2);
  fill_link (&hub_links[1], E, N, "LocationSkewed", 0, 0, NULL, 0);
  fill_link (&hub_links[2], S, N, "LocationCellar", 0, 1, NULL, 0);
  /* Down coincides with South → Cellar: park D on the South port. */
  fill_link_twin (&hub_links[3], DN, UP, "LocationCellar", 0, 1, S);
  fill_link (&hub_links[4], UP, DN, "LocationAttic", 0, 1, NULL, 0);
  fill_link (&hub_links[5], IN, OUT, "LocationCloset", 0, 1, NULL, 0);
  fill_link (&hub_links[6], OUT, IN, "LocationOutside", 0, 1, NULL, 0);
  fill_link (&hub_links[7], SE, E, "LocationDisagree", 0, 0, NULL, 0);
  fill_link (&hub_links[8], NE, SW, "LocationOffPageFree", 0, 0, NULL, 0);
  fill_link (&hub_links[9], SW, SE, "LocationOffPageFail", 0, 0, NULL, 0);
  fill_link (&hub_links[10], NW, NE, "LocationOffPagePass", 0, 0, NULL, 0);

  fill_link (&north_links[0], S, N, "LocationHub", 0, 1, NULL, 0);
  /* Orphan West Map Link: no Movement — dest NULL so it draws nothing. */
  fill_link (&north_links[1], W, E, NULL, 0, 0, NULL, 0);

  fill_link (&skewed_links[0], W, E, "LocationHub", 0, 0, NULL, 0);

  fill_link (&cellar_links[0], N, S, "LocationHub", 0, 1, NULL, 0);
  /* Up coincides with North → Hub: park U on the North port. */
  fill_link_twin (&cellar_links[1], UP, DN, "LocationHub", 0, 1, N);
  fill_link (&cellar_links[2], DN, UP, "LocationPit", 0, 0, NULL, 0);
  fill_link (&cellar_links[3], E, W, "LocationDoor", 1, 1, NULL, 0);

  fill_link (&attic_links[0], DN, UP, "LocationHub", 0, 1, NULL, 0);
  fill_link (&attic_links[1], UP, DN, "LocationSky", 0, 0, NULL, 0);

  fill_link (&closet_links[0], OUT, IN, "LocationHub", 0, 1, NULL, 0);
  fill_link (&closet_links[1], IN, OUT, "LocationVault", 0, 0, NULL, 0);
  fill_link (&closet_links[2], E, W, "LocationOneWay", 0, 0, NULL, 0);

  fill_link (&outside_links[0], IN, OUT, "LocationHub", 0, 1, NULL, 0);

  fill_link (&nolink_links[0], E, W, "LocationHub", 0, 1, NULL, 0);
  fill_link (&nolink_links[1], S, N, "LocationWide", 0, 1, nolink_s_mids, 2);

  fill_link (&sky_links[0], DN, UP, "LocationAttic", 0, 0, NULL, 0);
  fill_link (&pit_links[0], UP, DN, "LocationCellar", 0, 0, NULL, 0);
  fill_link (&vault_links[0], OUT, IN, "LocationCloset", 0, 0, NULL, 0);

  fill_link (&disagree_links[0], NW, SE, "LocationHub", 0, 0, NULL, 0);

  fill_link (&wide_links[0], N, S, "LocationNoLink", 0, 1, NULL, 0);
  fill_link (&wide_links[1], E, W, "LocationLoop", 0, 1, NULL, 0);
  fill_link (&wide_links[2], S, N, "LocationLatch", 1, 1, NULL, 0);

  fill_link (&loop_links[0], W, E, "LocationWide", 0, 1, NULL, 0);
  fill_link (&loop_links[1], N, S, "LocationLoop", 0, 0, NULL, 0);

  fill_link (&door_links[0], W, E, "LocationCellar", 1, 1, NULL, 0);
  fill_link (&latch_links[0], N, S, "LocationWide", 1, 1, NULL, 0);
}

static void
fill_node (map_node_t *n, const char *key, int x, int y, int z, int w, int h,
           map_link_t *links, int n_links,
           int has_in, int has_out, int has_up, int has_down)
{
  n->key = key;
  n->x = x;
  n->y = y;
  n->z = z;
  n->w = w;
  n->h = h;
  n->page = 0;
  n->hidden = 0;
  n->has_badge[MAP_BADGE (DIR_IN)] = (unsigned char) has_in;
  n->has_badge[MAP_BADGE (DIR_OUT)] = (unsigned char) has_out;
  n->has_badge[MAP_BADGE (DIR_UP)] = (unsigned char) has_up;
  n->has_badge[MAP_BADGE (DIR_DOWN)] = (unsigned char) has_down;
  n->links = links;
  n->n_links = n_links;
}

static int
key_in_list (const char *key, const char *const *list)
{
  int i;
  if (key == NULL)
    return 0;
  for (i = 0; list[i] != NULL; i++)
    if (strcmp (key, list[i]) == 0)
      return 1;
  return 0;
}

static int
cb_seen (void *ctx, const char *lockey)
{
  (void) ctx;
  return key_in_list (lockey, seen_keys);
}

static const char *
cb_name (void *ctx, const char *lockey)
{
  int i;
  (void) ctx;
  if (lockey == NULL)
    return NULL;
  for (i = 0; names[i][0] != NULL; i++)
    if (strcmp (lockey, names[i][0]) == 0)
      return names[i][1];
  return lockey;
}

static const char *
cb_exit_dest (void *ctx, const char *lockey, int dir)
{
  int i;
  (void) ctx;
  if (lockey == NULL)
    return NULL;
  for (i = 0; exits[i].from != NULL; i++)
    if (exits[i].dir == dir && strcmp (exits[i].from, lockey) == 0)
      return exits[i].dest;
  return NULL;
}

static int
cb_ever_blocked (void *ctx, const char *lockey, int dir)
{
  int i;
  (void) ctx;
  if (lockey == NULL)
    return 0;
  for (i = 0; ever_blocked_exits[i].from != NULL; i++)
    if (ever_blocked_exits[i].dir == dir
        && strcmp (ever_blocked_exits[i].from, lockey) == 0)
      return 1;
  return 0;
}

void
mapsvg_fixture_build (map_t *map, map_view_t *view)
{
  int i = 0;

  init_links ();

  /* Main-page nodes (include Hide rooms; seen() filters them). Skip
     LocationDoesNotExist. Unseen rooms keep nodes so stubs can resolve dest
     geometry when needed; Outside/Sky/Pit/Vault stay unseen.
     has_* flags mirror FileIO.vb bHasIn/Out/Up/Down from Movements. */
  /* key, x,y,z,w,h, links, n, has_in, has_out, has_up, has_down */
  fill_node (&nodes[i++], "LocationHub", 0, 0, 0, 6, 4, hub_links, 11,
             1, 1, 1, 1);
  fill_node (&nodes[i++], "LocationNorth", 0, -12, 0, 6, 4, north_links, 2,
             0, 0, 0, 0);
  fill_node (&nodes[i++], "LocationSkewed", 16, 20, 0, 6, 4, skewed_links, 1,
             0, 0, 0, 0);
  fill_node (&nodes[i++], "LocationCellar", 0, 12, 0, 6, 4, cellar_links, 4,
             0, 0, 1, 1);
  fill_node (&nodes[i++], "LocationAttic", 16, -12, 1, 6, 4, attic_links, 2,
             0, 0, 1, 1);
  fill_node (&nodes[i++], "LocationCloset", 16, 0, 0, 6, 4, closet_links, 3,
             1, 1, 0, 0);
  fill_node (&nodes[i++], "LocationOutside", 24, 0, 0, 6, 4, outside_links, 1,
             1, 0, 0, 0);
  fill_node (&nodes[i++], "LocationNoLink", -16, 0, 0, 6, 4, nolink_links, 2,
             0, 0, 0, 0);
  fill_node (&nodes[i++], "LocationSky", 16, -20, 1, 6, 4, sky_links, 1,
             0, 0, 0, 1);
  fill_node (&nodes[i++], "LocationPit", 0, 24, 0, 6, 4, pit_links, 1,
             0, 0, 1, 0);
  fill_node (&nodes[i++], "LocationVault", 32, 0, 0, 6, 4, vault_links, 1,
             0, 1, 0, 0);
  fill_node (&nodes[i++], "LocationHiddenSeen", -24, -12, 0, 6, 4, NULL, 0,
             0, 0, 0, 0);
  fill_node (&nodes[i++], "LocationHiddenUnseen", -24, 12, 0, 6, 4, NULL, 0,
             0, 0, 0, 0);
  fill_node (&nodes[i++], "LocationDisagree", -16, -12, 0, 6, 4,
             disagree_links, 1, 0, 0, 0, 0);
  fill_node (&nodes[i++], "LocationWide", -16, 12, 0, 10, 6, wide_links, 3,
             0, 0, 0, 0);
  fill_node (&nodes[i++], "LocationOneWay", 40, 0, 0, 6, 4, NULL, 0,
             0, 0, 0, 0);
  fill_node (&nodes[i++], "LocationLoop", 8, 20, 0, 6, 4, loop_links, 2,
             0, 0, 0, 0);
  fill_node (&nodes[i++], "LocationDoor", 16, 12, 0, 6, 4, door_links, 1,
             0, 0, 0, 0);
  fill_node (&nodes[i++], "LocationLatch", -16, 24, 0, 6, 4, latch_links, 1,
             0, 0, 0, 0);

  page.key = 0;
  page.label = "Main";
  page.nodes = nodes;
  page.n_nodes = i;

  g_map.pages = &page;
  g_map.n_pages = 1;
  g_map.pool = NULL;
  g_map.n_pool = 0;

  *map = g_map;

  view->seen = cb_seen;
  view->name = cb_name;
  view->exit_dest = cb_exit_dest;
  view->ever_blocked = cb_ever_blocked;
  view->ctx = NULL;
}
