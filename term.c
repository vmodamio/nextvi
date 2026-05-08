#ifndef NEXTVI_NOTERM
static struct termios termios;
#endif
sbuf *term_sbuf;
int term_record;
int term_winch;
int term_resized;
int xrows, xcols;
unsigned int ibuf_pos, ibuf_cnt, ibuf_sz = 128, icmd_pos;
unsigned char *ibuf, icmd[4096];
unsigned int texec, tn;

#ifdef NEXTVI_NOTERM
static char term_screen[NEXTVI_DISPLAY_ROWS + 1][NEXTVI_DISPLAY_COLS + 1];
static unsigned char term_dirty[NEXTVI_DISPLAY_ROWS + 1];
static int term_row, term_col, term_cursor_on = 1;
static unsigned char kq[128];
static unsigned int kq_r, kq_w;
static int key_shift, key_ctrl, key_alt, key_win, key_caps;

static const unsigned char key_normal[64] = {
	TK_ESC, '1', '2', '3', '4', '5', '6', '7',
	'8', '9', '0', '-', '=', 127, '\t', 'q',
	'w', 'e', 'r', 't', 'y', 'u', 'i', 'o',
	'p', '[', ']', '\\', 'a', 's', 'd', 'f',
	'g', 'h', 'j', 'k', 'l', ';', '\'', '\n',
	'z', 'x', 'c', 'v', 'b', 'n', 'm', ',',
	'.', '/', ' ', '`', TK_CTL('p'), TK_CTL('n'), TK_CTL('h'), ' ',
	127, 0, 0, 0, 0, 0, 0, 0,
};

static const unsigned char key_shifted[64] = {
	TK_ESC, '!', '@', '#', '$', '%', '^', '&',
	'*', '(', ')', '_', '+', 127, '\t', 'Q',
	'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O',
	'P', '{', '}', '|', 'A', 'S', 'D', 'F',
	'G', 'H', 'J', 'K', 'L', ':', '"', '\n',
	'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<',
	'>', '?', ' ', '~', TK_CTL('b'), TK_CTL('f'), 127, ' ',
	127, 0, 0, 0, 0, 0, 0, 0,
};

static void noterm_dirty(int row)
{
	if (row >= 0 && row <= NEXTVI_DISPLAY_ROWS)
		term_dirty[row] = 1;
}

static void noterm_refresh_dirty(void)
{
	for (int r = 0; r <= NEXTVI_DISPLAY_ROWS; r++) {
		if (!term_dirty[r])
			continue;
		nextvi_display_refresh_line(r, term_screen[r], NEXTVI_DISPLAY_COLS);
		term_dirty[r] = 0;
	}
}

static void noterm_clear_line(int row, int col)
{
	if (row < 0 || row > NEXTVI_DISPLAY_ROWS)
		return;
	col = MAX(0, MIN(col, NEXTVI_DISPLAY_COLS));
	memset(term_screen[row] + col, ' ', NEXTVI_DISPLAY_COLS - col);
	term_screen[row][NEXTVI_DISPLAY_COLS] = '\0';
	noterm_dirty(row);
}

static void noterm_put(int ch)
{
	if (ch == '\033')
		return;
	if (ch == '\r') {
		term_col = 0;
		return;
	}
	if (ch == '\n') {
		term_row = MIN(term_row + 1, NEXTVI_DISPLAY_ROWS);
		term_col = 0;
		return;
	}
	if (term_row < 0 || term_row > NEXTVI_DISPLAY_ROWS)
		return;
	if (term_col >= 0 && term_col < NEXTVI_DISPLAY_COLS) {
		term_screen[term_row][term_col] = ch ? ch : ' ';
		noterm_dirty(term_row);
	}
	term_col++;
}

static void noterm_write(char *s)
{
	for (; *s; s++) {
		if (*s == '\033' && s[1] == '[') {
			s += 2;
			while (*s && !isalpha((unsigned char)*s))
				s++;
			continue;
		}
		noterm_put((unsigned char)*s);
	}
}

static int noterm_modifier_bit(unsigned char code)
{
	switch (code & NEXTVI_MOD_CODE_MASK) {
	case NEXTVI_MOD_SHIFT: return NEXTVI_MOD_SHIFT;
	case NEXTVI_MOD_CTRL: return NEXTVI_MOD_CTRL;
	case NEXTVI_MOD_ALT: return NEXTVI_MOD_ALT;
	case NEXTVI_MOD_WIN: return NEXTVI_MOD_WIN;
	case NEXTVI_MOD_CAPS: return NEXTVI_MOD_CAPS;
	default: return 0;
	}
}

static void noterm_modifier(unsigned char ev)
{
	int *state = NULL, bit = noterm_modifier_bit(ev);
	if (!bit)
		return;
	if (bit == NEXTVI_MOD_CAPS) {
		if (ev & NEXTVI_KEY_PRESS)
			key_caps = !key_caps;
		return;
	}
	if (bit == NEXTVI_MOD_SHIFT)
		state = &key_shift;
	else if (bit == NEXTVI_MOD_CTRL)
		state = &key_ctrl;
	else if (bit == NEXTVI_MOD_ALT)
		state = &key_alt;
	else if (bit == NEXTVI_MOD_WIN)
		state = &key_win;
	*state = !!(ev & NEXTVI_KEY_PRESS);
}

static int noterm_key_event(void)
{
	unsigned char ev, code, ch;
	while (nextvi_keyboard_read(&ev) > 0) {
		if (ev & NEXTVI_KEY_MODIFIER) {
			noterm_modifier(ev);
			continue;
		}
		if (!(ev & NEXTVI_KEY_PRESS))
			continue;
		code = ev & NEXTVI_KEY_CODE_MASK;
		ch = (key_shift ^ (key_caps && key_normal[code] >= 'a' &&
			key_normal[code] <= 'z')) ? key_shifted[code] : key_normal[code];
		if (key_ctrl && ch >= 'a' && ch <= 'z')
			ch = TK_CTL(ch);
		else if (key_ctrl && ch >= 'A' && ch <= 'Z')
			ch = ((ch - 'A') + 'a') & 037;
		(void)key_alt;
		(void)key_win;
		if (ch)
			return ch;
	}
	return 0;
}

__attribute__((weak)) void nextvi_display_refresh_line(int row, const char *text, int cols)
{
	(void)row;
	(void)text;
	(void)cols;
}

int nextvi_keyboard_queue_push(unsigned char event)
{
	unsigned int next = (kq_w + 1) % LEN(kq);
	if (next == kq_r)
		return 0;
	kq[kq_w] = event;
	kq_w = next;
	return 1;
}

int nextvi_keyboard_queue_pop(unsigned char *event)
{
	if (kq_r == kq_w)
		return 0;
	*event = kq[kq_r];
	kq_r = (kq_r + 1) % LEN(kq);
	return 1;
}

__attribute__((weak)) int nextvi_keyboard_read(unsigned char *event)
{
	return nextvi_keyboard_queue_pop(event);
}
#endif

void term_init(void)
{
#ifndef NEXTVI_NOTERM
	struct winsize win;
	struct termios newtermios;
	char *s;
#endif
	term_winch = 0;
	term_resized++;
	sbuf_make(term_sbuf, 2048)
#ifdef NEXTVI_NOTERM
	xcols = NEXTVI_DISPLAY_COLS;
	xrows = NEXTVI_DISPLAY_ROWS;
	term_row = term_col = 0;
	for (int r = 0; r <= NEXTVI_DISPLAY_ROWS; r++) {
		memset(term_screen[r], ' ', NEXTVI_DISPLAY_COLS);
		term_screen[r][NEXTVI_DISPLAY_COLS] = '\0';
		term_dirty[r] = 1;
	}
	noterm_refresh_dirty();
#else
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
#endif
}

void term_done(void)
{
	if (!term_sbuf)
		return;
	term_cursor(1);
	term_commit();
	sbuf_free(term_sbuf)
#ifndef NEXTVI_NOTERM
	tcsetattr(0, 0, &termios);
#endif
}

void term_clean(void)
{
#ifdef NEXTVI_NOTERM
	for (int r = 0; r <= NEXTVI_DISPLAY_ROWS; r++)
		noterm_clear_line(r, 0);
	term_row = term_col = 0;
	noterm_refresh_dirty();
#else
	term_write("\x1b[2J", 4)	/* clear screen */
	term_write("\x1b[H", 3)		/* cursor topleft */
#endif
}

void term_commit(void)
{
#ifdef NEXTVI_NOTERM
	noterm_write(term_sbuf->s);
	noterm_refresh_dirty();
#else
	term_write(term_sbuf->s, term_sbuf->s_n)
#endif
	sbuf_cut(term_sbuf, 0)
	term_record = 0;
}

static void term_out(char *s)
{
	if (term_record)
		sbufn_str(term_sbuf, s)
	else {
#ifdef NEXTVI_NOTERM
		noterm_write(s);
		noterm_refresh_dirty();
#else
		term_write(s, strlen(s))
#endif
	}
}

void term_chr(int ch)
{
	char s[4] = {ch};
	term_out(s);
}

void term_kill(void)
{
#ifdef NEXTVI_NOTERM
	noterm_clear_line(term_row, term_col);
#else
	term_out("\33[K");
#endif
}

void term_room(int n)
{
#ifdef NEXTVI_NOTERM
	int count = abs(n);
	if (!n || term_row < 0 || term_row > NEXTVI_DISPLAY_ROWS)
		return;
	count = MIN(count, NEXTVI_DISPLAY_ROWS - term_row + 1);
	if (n > 0) {
		for (int r = NEXTVI_DISPLAY_ROWS; r >= term_row + count; r--) {
			memcpy(term_screen[r], term_screen[r - count],
				NEXTVI_DISPLAY_COLS + 1);
			noterm_dirty(r);
		}
		for (int r = term_row; r < term_row + count; r++)
			noterm_clear_line(r, 0);
	} else {
		for (int r = term_row; r <= NEXTVI_DISPLAY_ROWS - count; r++) {
			memcpy(term_screen[r], term_screen[r + count],
				NEXTVI_DISPLAY_COLS + 1);
			noterm_dirty(r);
		}
		for (int r = NEXTVI_DISPLAY_ROWS - count + 1; r <= NEXTVI_DISPLAY_ROWS; r++)
			noterm_clear_line(r, 0);
	}
#else
	char cmd[64] = "\33[";
	if (!n)
		return;
	char *s = itoa(abs(n), cmd+2);
	s[0] = n < 0 ? 'M' : 'L';
	s[1] = '\0';
	term_out(cmd);
#endif
}

void term_pos(int r, int c)
{
#ifdef NEXTVI_NOTERM
	if (r < 0)
		term_col = MAX(0, MIN(term_col + c, NEXTVI_DISPLAY_COLS));
	else {
		term_row = MAX(0, MIN(r, NEXTVI_DISPLAY_ROWS));
		term_col = MAX(0, MIN(c, NEXTVI_DISPLAY_COLS));
	}
#else
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
#endif
}

void term_cursor(int on)
{
#ifdef NEXTVI_NOTERM
	term_cursor_on = on;
	(void)term_cursor_on;
#else
	term_out(on ? "\33[?25h" : "\33[?25l");
#endif
}

/* read s before reading from the input backend */
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
#ifndef NEXTVI_NOTERM
	static struct pollfd ufd = {STDIN_FILENO, POLLIN};
	int cw;
#endif
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
#ifdef NEXTVI_NOTERM
		if (!(*ibuf = noterm_key_event())) {
			err:
			*ibuf = 0;
		}
#else
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
#endif
		ret:
		ibuf_cnt = 1;
		ibuf_pos = 0;
	}
	if (icmd_pos < sizeof(icmd))
		icmd[icmd_pos++] = ibuf[ibuf_pos];
	return ibuf[ibuf_pos++];
}
