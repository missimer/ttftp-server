#          The Trivial Trivial File Transfer Protocol Server
#  Copyright (C) 2013 Eric Mismier
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


CFLAGS = -I./include -MMD -Wall

CFLAGS += -g

OBJS = src/ttftps.o
DFILES = $(patsubst %.o,%.d,$(OBJS))
PROGS = ttftps


all: $(PROGS)

$(PROGS): $(OBJS)
	gcc $^ -o $@

clean:
	rm -rf $(PROGS) $(OBJS) $(DFILES)
	find -name "*~" -exec rm {} \;


-include $(DFILES)

.PHONY: all clean
