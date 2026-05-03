static struct termios termios;
sbuf *term_sbuf;
int term_record;
int term_winch;
int term_resized;
int xrows, xcols;
unsigned int ibuf_pos, ibuf_cnt, ibuf_sz = 128, icmd_pos;
unsigned char *ibuf, icmd[4096];
unsigned int texec, tn;

void term_init(void)
{
	struct winsize win;
	struct termios newtermios;
	char *s;
	term_winch = 0;
	term_resized++;
	sbuf_make(term_sbuf, 2048)
	tcgetattr(0, &termios);
	newtermios = termios;
	newtermios.c_lflag &= ~(ICANON | ISIG | ECHO);
	tcsetattr(0, TCSAFLUSH, &newtermios);
	if (!ioctl(0, TIOCGWINSZ, &win)) {
		xcols = win.ws_col;
		xrows = win.ws_row;
	} else {
		if ((s = getenv("LINES")))
			xrows = atoi(s);
		if ((s = getenv("COLUMNS")))
			xcols = atoi(s);
	}
	xcols = xcols ? xcols : 80;
	xrows = xrows ? xrows : 25;
}

void term_done(void)
{
	if (!term_sbuf)
		return;
	term_commit();
	sbuf_free(term_sbuf)
	tcsetattr(0, 0, &termios);
}

void term_clean(void)
{
	term_write("\x1b[2J", 4)	/* clear screen */
	term_write("\x1b[H", 3)		/* cursor topleft */
}

void term_commit(void)
{
	term_write(term_sbuf->s, term_sbuf->s_n)
	sbuf_cut(term_sbuf, 0)
	term_record = 0;
}

static void term_out(char *s)
{
	if (term_record)
		sbufn_str(term_sbuf, s)
	else
		term_write(s, strlen(s))
}

void term_chr(int ch)
{
	char s[4] = {ch};
	term_out(s);
}

void term_kill(void)
{
	term_out("\33[K");
}

void term_room(int n)
{
	char cmd[64] = "\33[";
	if (!n)
		return;
	char *s = itoa(abs(n), cmd+2);
	s[0] = n < 0 ? 'M' : 'L';
	s[1] = '\0';
	term_out(cmd);
}

void term_pos(int r, int c)
{
	char buf[64] = "\r\33[", *s;
	if (r < 0) {
		memcpy(itoa(MAX(0, c), buf+3), c > 0 ? "C" : "D", 2);
		term_out(buf);
	} else {
		s = itoa(r + 1, buf+3);
		if (c > 0) {
			*s++ = ';';
			s = itoa(c + 1, s);
		}
		memcpy(s, "H", 2);
		term_out(buf+1);
	}
}

/* read s before reading from the terminal */
void term_push(char *s, unsigned int n)
{
	static unsigned int tibuf_pos, tibuf_cnt;
	if (texec == '@' && xquit > 0) {
		xquit = 0;
		tn = 0;
		ibuf_cnt = tibuf_cnt;
		ibuf_pos = tibuf_cnt;
	}
	if (ibuf_cnt + n >= ibuf_sz || ibuf_sz - ibuf_cnt + n > 128) {
		ibuf_sz = ibuf_cnt + n + 128;
		ibuf = erealloc(ibuf, ibuf_sz);
	}
	if (texec) {
		if (tibuf_pos != ibuf_pos)
			tn = 0;
		memmove(ibuf + ibuf_pos + n + tn,
			ibuf + ibuf_pos + tn, ibuf_cnt - ibuf_pos - tn);
		memcpy(ibuf + ibuf_pos + tn, s, n);
		tn += n;
		tibuf_pos = ibuf_pos;
	} else
		memcpy(ibuf + ibuf_cnt, s, n);
	tibuf_cnt = ibuf_cnt;
	ibuf_cnt += n;
}

void term_back(int c)
{
	char s[1] = {c};
	term_push(s, 1);
}

int term_read(int winch)
{
	static struct pollfd ufd = {STDIN_FILENO, POLLIN};
	int cw;
	if (ibuf_pos >= ibuf_cnt) {
		if (texec) {
			xquit = !xquit ? 1 : xquit;
			if (texec == '&')
				goto err;
		}
		if (term_winch && winch) {
			*ibuf = winch;	/* yield until term_winch is cleared */
			goto ret;
		}
		cw = 0;
		re:
		/* read a single input character */
		if (xquit < 0 || poll(&ufd, 1, -1) <= 0 ||
				read(STDIN_FILENO, ibuf, 1) <= 0) {
			xquit = !isatty(STDIN_FILENO) ? -1 : xquit;
			if (term_winch && winch && xquit >= 0) {
				*ibuf = winch;
				goto ret;
			} else if (term_winch != cw && !winch && xquit >= 0) {
				cw = term_winch;
				goto re;
			}
			err:
			*ibuf = 0;
		}
		ret:
		ibuf_cnt = 1;
		ibuf_pos = 0;
	}
	if (icmd_pos < sizeof(icmd))
		icmd[icmd_pos++] = ibuf[ibuf_pos];
	return ibuf[ibuf_pos++];
}
