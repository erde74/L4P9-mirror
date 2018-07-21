#include <u.h>
#include <libc.h>
#include <bio.h>

#include "pci.h"
#include "vga.h"

// HK 20090930 begin

typedef struct Vmode
{
	char	name[32];
	char	chan[32];
	int	id;
	int	attr;	/* flags */
	int	bpl;
	int	dx, dy;
	int	depth;
	char	*model;
	int	r, g, b, x;
	int	ro, go, bo, xo;
	int	directcolor;	/* flags */
	ulong	paddr;
} Vmode;

static char *modelstr[] = {
	"text", "cga", "hercules", "planar", "packed", "non-chain4", "direct", "YUV"
};

static void set_qemuvga0reg(int reg, int data)
{
	outportw(0x01ce, reg);
	outportw(0x01cf, data);
	return;
}

static int get_qemuvga0reg(int reg)
{
	outportw(0x01ce, reg);
	return inportw(0x01cf);
}

int is_stdvga(void)
{
	set_qemuvga0reg(0x0000, 0xb0c2);
	return get_qemuvga0reg(0x0000) == 0xb0c2;
}

static struct Modetable {
	int x, y, bpp, bpl;
} modetable[] = {
	/* 0x0100 - 0x0107 */
	{  640,  400,  8, 1 },
	{  640,  480,  8, 1 },
	{    0,    0,  0, 0 },
	{  800,  600,  8, 1 },
	{    0,    0,  0, 0 },
	{ 1024,  768,  8, 1 },
	{    0,    0,  0, 0 },
	{ 1280, 1024,  8, 1 },

	/* 0x0108 - 0x010f */
	{    0,    0,  0, 0 },
	{    0,    0,  0, 0 },
	{    0,    0,  0, 0 },
	{    0,    0,  0, 0 },
	{    0,    0,  0, 0 },
	{  320,  200, 15, 2 },
	{  320,  200, 16, 2 },
	{  320,  200, 32, 4 },

	/* 0x0110 - 0x0117 */
	{  640,  480, 15, 2 },
	{  640,  480, 16, 2 },
	{  640,  480, 32, 4 },
	{  800,  600, 15, 2 },
	{  800,  600, 16, 2 },
	{  800,  600, 32, 4 },
	{ 1024,  768, 15, 2 },
	{ 1024,  768, 16, 2 },

	/* 0x0118 - 0x011f */
	{ 1024,  768, 32, 4 },
	{ 1280, 1024, 15, 2 },
	{ 1280, 1024, 16, 2 },
	{ 1280, 1024, 32, 4 },
	{ 1600, 1200,  8, 1 },
	{ 1600, 1200, 15, 2 },
	{ 1600, 1200, 16, 2 },
	{ 1600, 1200, 32, 4 },

	{   -1,    0,  0, 0 }
};

static void snarf(Vga *vga, Ctlr *ctlr)
{
	if (!is_stdvga())
		error("qemustd: device not found\n");
	vga->apz = 16 * 1024 * 1024;
	return;
}

static void modeinfo(struct Modetable *t, int i, Vmode *m)
{
	m->id = i + 0x0100;
	m->attr = 0x0080;
	m->bpl = t->x * t->bpl;
	m->dx = t->x;
	m->dy = t->y;
	m->depth = t->bpp;
	m->model = modelstr[(t->bpp == 8) ? 0x04 : 0x06];
	m->r = m->g = m->b = m->x = 0;
	m->ro = m->go = m->bo = m->xo = 0;
	if (t->bpp == 15) {
		m->r = m->g = m->b = 5;
		m->x = 1;
	} else if (t->bpp == 16) {
		m->r = m->b = 5;
		m->g = 6;
	} else if (t->bpp == 32) {
		m->r = m->g = m->b = m->x = 8;
	}
	m->directcolor = 0x00;	// dummy
	m->paddr = 0xe0000000;
	snprint(m->name, sizeof m->name, "%dx%dx%d",
	m->dx, m->dy, m->depth);
	if (m->depth <= 8)
		snprint(m->chan, sizeof m->chan, "m%d", m->depth);
	else if (m->x)
		snprint(m->chan, sizeof m->chan, "x%dr%dg%db%d", m->x, m->r, m->g, m->b);
	else
		snprint(m->chan, sizeof m->chan, "r%dg%db%d", m->r, m->g, m->b);
	return;
}

static void dump(Vga *vga, Ctlr *ctlr)
{
	char *vtype = vga->ctlr->type;
	int i;
	Vmode m;

	Bprint(&stdout._Biobufhdr, "is_stdvga(): %d\n", is_stdvga());
	Bprint(&stdout._Biobufhdr, "vga->ctlr->type: %s\n", (vtype == nil) ? "nil" : vtype);
	Bprint(&stdout._Biobufhdr, "vga->ctlr->name: %s\n", vga->ctlr->name);
//	(*generic.dump)(vga, ctlr);
	for (i = 0; modetable[i].x >= 0; i++) {
		if (modetable[i].x == 0)
			Bprint(&stdout._Biobufhdr, "qemustd: cannot get mode 0x%ux\n", i + 0x0100);
		else {
			modeinfo(&modetable[i], i, &m);
			printitem("qemustd", "mode");
			Bprint(&stdout._Biobufhdr, "0x%ux %s %s %s\n", m.id, m.name, m.chan, m.model);
		}
	}
	return;
}

static char *probe(Vga *vga, Ctlr *ctlr)
{
	if (is_stdvga())
		return "0xC0000=ctlr:qemustd";
	return nil;
}

static Mode *dbmode_(char *monitordb, char *type, char *mode)
{
	int i;
	Vmode vm;
	Mode *m;

	for (i = 0; modetable[i].x >= 0; i++) {
		if (modetable[i].x == 0)
			continue;
		modeinfo(&modetable[i], i, &vm);
		if (strcmp(vm.name, mode) == 0)
			goto havemode;
	}
	werrstr("no such qemustd(vesa) mode");
	return nil;

havemode:
	m = alloc(sizeof(Mode));
	strcpy(m->type, "qemustd");
	strcpy(m->size, vm.name);
	strcpy(m->chan, vm.chan);
	m->frequency = 100;
	m->x = vm.dx;
	m->y = vm.dy;
	m->z = vm.depth;
	m->ht = m->x;
	m->shb = m->x;
	m->ehb = m->x;
	m->shs = m->x;
	m->ehs = m->x;
	m->vt = m->y;
	m->vrs = m->y;
	m->vre = m->y;

	m->attr = alloc(sizeof(Attr));
	m->attr->attr = "id";
	m->attr->val = alloc(32);
	sprint(m->attr->val, "0x%x", vm.id);
	return m;
}

Ctlr qemustd = {
	"qemustd",			/* name */
	snarf,				/* snarf */
	0,				/* options */
	0,				/* init */
	0,				/* load */
	dump,				/* dump */
	0,				/* loadtext */
	probe,				/* probe */
	dbmode_,			/* dbmode */
	0,				/* flag */
};

// HK 2090930 end

