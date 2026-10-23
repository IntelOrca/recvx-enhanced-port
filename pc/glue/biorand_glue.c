/* BioRand CVX pack (docs/PC_PORT.md in biorand/classic).
 *
 * The pack lives in mods/biorand (or is the RECVX_MODS folder itself):
 *   biorand.ini, start_inventory.txt, RM_*.RDX, SYSTEM.AFS/<n>, ADV.AFS/<n>
 *
 * The room files are loaded by the normal mods path. This file reads the
 * manifest, applies the data-only fixes the PS2 randomizer patches into the
 * ELF, and provides the per-pickup quantity used by sub1.c GetItem. Without a
 * pack every function is a no-op.
 */
#include "../../decomp/include/ps2/veronica/prog/types.h"
#include "../../decomp/include/ps2/veronica/prog/main.h"
#include "../../decomp/include/ps2/veronica/prog/item.h"
#include "../src/host/fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void host_log(const char* fmt, ...);
void pc_biorand_set_door_skip(int value);   /* host.c: g_config.skip_doors */

unsigned char pc_biorand_pickup_quantity;

static int active;
static int per_item_quantities = 1;
static int no_nosferatu_poison = 1;
static int rifle_stacking = 1;
static int pickup_hack = 1;
static char biorand_dir[1024];

static const char* mods_root(void)
{
    const char* dir = getenv("RECVX_MODS");
    return dir && dir[0] ? dir : "mods";
}

/* <root>/NAME or <root>/biorand/NAME */
static int find_file(const char* name, char* path, size_t npath)
{
    const char* root = mods_root();
    if (snprintf(path, npath, "%s/%s", root, name) >= (int)npath)
        return 0;
    {
        FILE* f = pc_fopen(path, "rb");
        if (f) { fclose(f); return 1; }
    }
    if (snprintf(path, npath, "%s/biorand/%s", root, name) >= (int)npath)
        return 0;
    {
        FILE* f = pc_fopen(path, "rb");
        if (f) { fclose(f); return 1; }
    }
    return 0;
}

static void trim(char* s)
{
    char* p = s;
    size_t n;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

static void read_ini(const char* path)
{
    FILE* f = pc_fopen(path, "r");
    char line[1024];
    if (!f)
        return;

    while (fgets(line, sizeof(line), f))
    {
        char* eq;
        trim(line);
        if (!line[0] || line[0] == '#' || line[0] == ';')
            continue;
        eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq++ = 0;
        trim(line);
        trim(eq);

        if (!strcmp(line, "format")) {
            /* reserved */
        } else if (!strcmp(line, "seed")) {
            host_log("biorand: seed %s\n", eq);
        } else if (!strcmp(line, "per_item_quantities")) {
            per_item_quantities = atoi(eq) != 0;
        } else if (!strcmp(line, "nosferatu_poison")) {
            no_nosferatu_poison = atoi(eq) == 0;
        } else if (!strcmp(line, "rifle_stacking")) {
            rifle_stacking = atoi(eq) != 0;
        } else if (!strcmp(line, "pickup_hack")) {
            pickup_hack = atoi(eq) != 0;
        } else if (!strcmp(line, "door_skip")) {
            if (atoi(eq) != 0)
                pc_biorand_set_door_skip(1);
        }
    }
    fclose(f);
}

int pc_biorand_active(void)
{
    return active;
}

int pc_biorand_no_nosferatu_poison(void)
{
    return active && no_nosferatu_poison;
}

void pc_biorand_set_pickup_quantity(unsigned char quantity)
{
    pc_biorand_pickup_quantity = (active && per_item_quantities) ? quantity : 0;
}

/* AllItemInit: the biorand pack carries the full new-game inventory instead of
   the room-1000 script and the two patched AllItemInit immediates. */
void pc_biorand_apply_start_inventory(void)
{
    char path[1100], line[256];
    FILE* f;
    if (!active || sys->gm_mode == 3)
        return;
    if (snprintf(path, sizeof(path), "%s/start_inventory.txt", biorand_dir) >= (int)sizeof(path))
        return;
    f = pc_fopen(path, "r");
    if (!f)
        return;

    while (fgets(line, sizeof(line), f))
    {
        int slot, type, count;
        trim(line);
        if (sscanf(line, "special %d %d", &type, &count) == 2)
        {
            sys->itm[1] = ((unsigned int)(type & 0xFF) << 16) | (count & 0xFF);
        }
        else if (sscanf(line, "slot %d %d %d", &slot, &type, &count) == 3)
        {
            if (slot >= 0 && slot < 12)
                sys->itm[2 + slot] = ((unsigned int)(type & 0xFF) << 16) | (count & 0xFF);
        }
    }
    fclose(f);
}

static void apply_itemdata(void)
{
    /* itemdata[40] = rifle bullets, itemdata[42] = assault rifle bullets:
       type bit 0x200 makes them stack, as biorand's PS2 itemdata patch. */
    if (!active || !rifle_stacking)
        return;
    if (itemdata[40].type & 0x200) return;
    itemdata[40].type |= 0x200;
    itemdata[42].type |= 0x200;
}

void biorand_load(void)
{
    char path[1100];
    const char* slash;

    if (!find_file("biorand.ini", path, sizeof(path)))
        return;

    active = 1;
    slash = strrchr(path, '/');
    if (slash)
    {
        size_t n = (size_t)(slash - path);
        if (n >= sizeof(biorand_dir)) n = sizeof(biorand_dir) - 1;
        memcpy(biorand_dir, path, n);
        biorand_dir[n] = 0;
    }
    host_log("biorand: mod pack %s\n", biorand_dir);
    read_ini(path);
    apply_itemdata();
    if (pickup_hack)
        host_log("biorand: pickup_hack is not implemented yet\n");
}
