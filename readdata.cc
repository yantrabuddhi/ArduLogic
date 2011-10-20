/*
 *  ArduLogic - Low Speed Logic Analyzer using the Arduino Hardware
 *
 *  Copyright (C) 2011  Clifford Wolf <clifford@clifford.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "ardulogic.h"

#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

static int fd;
static struct termios tcattr_old;
static const char *tts_name;

static void sigint_hdl(int dummy)
{
	char ch = 0;
	if (write(fd, &ch, 1) != 1) {
		fprintf(stderr, "I/O Error on tts `%s': %s\n", tts_name, strerror(errno));
		tcsetattr(fd, TCSAFLUSH, &tcattr_old);
		exit(1);
	}
}

static unsigned char serialread()
{
	unsigned char ch;
	int rc = read(fd, &ch, 1);
	if (rc == 1)
		return ch;
	if (rc == 0)
		fprintf(stderr, "I/O Error on tts `%s': EOF\n", tts_name);
	else
		fprintf(stderr, "I/O Error on tts `%s': %s\n", tts_name, strerror(errno));
	tcsetattr(fd, TCSAFLUSH, &tcattr_old);
	exit(1);
}

void readdata(const char *tts)
{
	tts_name = tts;

	fd = open(tts, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Failed to open tts device `%s': %s\n", tts, strerror(errno));
		exit(1);
	}

	tcgetattr(fd, &tcattr_old);
	struct termios tcattr = tcattr_old;
	tcattr.c_iflag = INPCK | PARMRK;
	tcattr.c_oflag = 0;
	tcattr.c_cflag = CS8 | CREAD | CLOCAL;
	tcattr.c_lflag = 0;
	cfsetspeed(&tcattr, B115200);
	tcsetattr(fd, TCSAFLUSH, &tcattr);

	printf("Waiting for Arduino on `%s': Please press the reset button.\n", tts);

	char header[32 + TOTAL_PIN_NUM] = "\377..ARDULOGIC:";
	int hp = strlen(header);
	header[1] = header[2] = 0;
	for (int i = 0; i < TOTAL_PIN_NUM; i++)
		header[hp++] = pins[i] + '0';
	header[hp++] = ':';
	header[hp++] = '\r';
	header[hp++] = '\n';

	int idx = 0;
	while (idx != hp) {
		unsigned char ch = serialread();
		if (header[idx] != ch)
			idx = header[0] == ch ? 1 : 0;
		else
			idx++;
		if (32 < ch && ch < 127)
			printf("<%d:%c> @%d\n", ch, ch, idx);
		else
			printf("<%d> @%d\n", ch, idx);
	}

	printf("Recording. Press Ctrl-C to stop.\n");
	sighandler_t old_hdl = signal(SIGINT, &sigint_hdl);

	for (idx = 0;; idx++)
	{
		unsigned char ch = serialread();
		if (ch == 0377) {
			ch = serialread();
			if (ch == 0)
				break;
		}
		printf("<%d>\n", ch);
	}

	printf("Recording finished. Got %d bytes tts payload.\n", idx);
	signal(SIGINT, old_hdl);

	tcsetattr(fd, TCSAFLUSH, &tcattr_old);
	close(fd);
}

