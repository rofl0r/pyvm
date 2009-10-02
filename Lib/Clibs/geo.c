/* Lookup routines for the compressed, sorted
geolocation db. Code speaks
IPs are in little endian.
*/

typedef unsigned int uint, IP;

struct ent
{
	IP ip;
	char cc [2];
} __attribute__ ((packed));

static struct ent *tab;
static uint tabc;

void inittab (struct ent *x, uint y)
{
	tab = x;
	tabc = y;
}

void lookup (IP ip, char cc [2])
{
	uint lo = 0, hi = tabc - 1, mid = 0;

	while (hi - lo > 1) {
		mid = (lo + hi) / 2;
		if (tab [mid].ip > ip)
			hi = mid;
		else lo = mid;
	}

	if (tab [mid].ip < ip)
		++mid;
	cc [0] = tab [mid].cc [0];
	cc [1] = tab [mid].cc [1];
}

const char ABI [] =
"- inittab si\n"
"- lookup  is\n"
;
