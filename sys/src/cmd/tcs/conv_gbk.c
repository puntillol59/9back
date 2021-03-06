#include	<u.h>
#include	<libc.h>
#include	<bio.h>
#include	"hdr.h"
#include	"conv.h"
#include	"gbk.h"

static void
gbkproc(int c, Rune **r, long input_loc)
{
	static enum { state0, state1 } state = state0;
	static int lastc;
	long ch, cold = c;

	switch(state)
	{
	case state0:	/* idle state */
		if(c < 0)
			return;
		if(c >= 0x80){
			lastc = c;
			state = state1;
			return;
		}
		emit(c);
		return;

	case state1:	/* seen a font spec */
		ch = -1;
		c = lastc<<8 | c;
		if(c >= GBKMIN && c < GBKMAX)
			ch = tabgbk[c - GBKMIN];
		if(ch < 0){
			nerrors++;
			if(squawk)
				warn("bad gbk glyph %d (from 0x%x,0x%lx) near byte %ld in %s", (c & 0xFF), lastc, cold, input_loc, file);
			if(!clean)
				emit(BADMAP);
			state = state0;
			return;
		}
		emit(ch);
		state = state0;
	}
}

void
gbk_in(int fd, long *, struct convert *out)
{
	Rune ob[N];
	Rune *r, *re;
	uchar ibuf[N];
	int n, i;
	long nin;

	r = ob;
	re = ob+N-3;
	nin = 0;
	while((n = read(fd, ibuf, sizeof ibuf)) > 0){
		for(i = 0; i < n; i++){
			gbkproc(ibuf[i], &r, nin++);
			if(r >= re){
				OUT(out, ob, r-ob);
				r = ob;
			}
		}
		if(r > ob){
			OUT(out, ob, r-ob);
			r = ob;
		}
	}
	gbkproc(-1, &r, nin);
	if(r > ob)
		OUT(out, ob, r-ob);
	OUT(out, ob, 0);
}


void
gbk_out(Rune *base, int n, long *)
{
	char *p;
	int i;
	Rune r;
	static int first = 1;

	if(first){
		first = 0;
		for(i = 0; i < NRUNE; i++)
			tab[i] = -1;
		for(i = GBKMIN; i < GBKMAX; i++)
			tab[tabgbk[i-GBKMIN]] = i;
	}
	nrunes += n;
	p = obuf;
	for(i = 0; i < n; i++){
		r = base[i];
		if(r < 0x80)
			*p++ = r;
		else {
			if(r < NRUNE && tab[r] != -1){
				r = tab[r];
				*p++ = (r>>8) & 0xFF;
				*p++ = r & 0xFF;
				continue;
			}
			if(squawk)
				warn("rune 0x%x not in output cs", r);
			nerrors++;
			if(clean)
				continue;
			*p++ = BYTEBADMAP;
		}
	}
	noutput += p-obuf;
	if(p > obuf)
		write(1, obuf, p-obuf);
}
