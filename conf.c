#include "kmap.h"

/* access mode of new files */
const int conf_mode = 0600;

/* right-to-left characters */
#define CR2L		"ء-يپچژکگی‌-‍؛،»«؟ً-ْٔ"
/* neutral characters */
#define CNEUT		"\x1- !-/:-@[-`{-\x7f"

struct dircontext dctxs[] = {
	{"^[" CR2L "]", -1},
	{"^[a-zA-Z_0-9]", +1},
};
const int dctxlen = LEN(dctxs);

struct dirmark dmarks[] = {
	{"[" CR2L "][" CNEUT CR2L "]*[" CR2L "]", +1, {-1}},
	{"^([ \t]+)?([" CNEUT "]*[^" CR2L "]*[^" CR2L CNEUT "](?:[" CNEUT "]+$)?)", -1, {0, 1, -1}},
	{"[^" CR2L CNEUT "][^" CR2L "]*[^" CR2L CNEUT "](?:[" CNEUT "]+$)?", -1, {-1}},
};
const int dmarkslen = LEN(dmarks);

struct placeholder _ph[2] = {
	{{0x0,0x1f}, "^", 1, 1},
	{{0x200c,0x200d}, "-", 1, 3},
};
struct placeholder *ph = _ph;
int phlen = LEN(_ph);

char **conf_kmap(int id)
{
	return kmaps[id];
}

int conf_kmapfind(char *name)
{
	for (int i = 0; i < LEN(kmaps); i++)
		if (name && kmaps[i][0] && !strcmp(name, kmaps[i][0]))
			return i;
	return 0;
}

char *conf_digraph(int c1, int c2)
{
	for (int i = 0; i < LEN(digraphs); i++)
		if (digraphs[i][0][0] == c1 && digraphs[i][0][1] == c2)
			return digraphs[i][1];
	return NULL;
}
