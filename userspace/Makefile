CC = gcc
AS = as
AR = ar
LD = ld

LIBC_DIR = libc
PROG_DIR = programs
FS_DIR = fs

INC = -I$(LIBC_DIR)/include
CFLAGS = -std=gnu99 -ffreestanding -m32 -O2 -Wall -Wextra -nostdlib \
         -fno-stack-protector -mno-sse -mno-mmx -mno-sse2 -mno-3dnow \
         -fno-pie -no-pie $(INC)
LDFLAGS = -m elf_i386 -nostdlib -static -no-pie

LIBC_OBJS = \
    $(LIBC_DIR)/crt0.o \
    $(LIBC_DIR)/syscall.o \
    $(LIBC_DIR)/unistd.o \
    $(LIBC_DIR)/string.o \
    $(LIBC_DIR)/stdio.o \
    $(LIBC_DIR)/stdlib.o \
    $(LIBC_DIR)/syswait.o

LIBC_A = $(LIBC_DIR)/libc.a

PROGRAMS = \
    $(PROG_DIR)/init.elf \
    $(PROG_DIR)/sh.elf \
    $(PROG_DIR)/echo.elf \
    $(PROG_DIR)/cat.elf \
    $(PROG_DIR)/ls.elf \
    $(PROG_DIR)/clear.elf \
    $(PROG_DIR)/help.elf \
    $(PROG_DIR)/reboot.elf \
    $(PROG_DIR)/rm.elf \
    $(PROG_DIR)/mkdir.elf \
    $(PROG_DIR)/uname.elf

all: $(PROGRAMS) install-fs

$(LIBC_A): $(LIBC_OBJS)
	rm -f $@
	$(AR) rcs $@ $(filter-out $(LIBC_DIR)/crt0.o, $^)

$(LIBC_DIR)/%.o: $(LIBC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIBC_DIR)/%.o: $(LIBC_DIR)/%.S
	$(CC) $(CFLAGS) -c -o $@ $<

$(PROG_DIR)/%.elf: $(PROG_DIR)/%.c $(LIBC_A) $(LIBC_DIR)/crt0.o
	$(CC) $(CFLAGS) -c -o $(PROG_DIR)/$*.o $<
	$(LD) $(LDFLAGS) -o $@ $(LIBC_DIR)/crt0.o $(PROG_DIR)/$*.o $(LIBC_A)

install-fs: $(PROGRAMS)
	mkdir -p $(FS_DIR)/bin
	for f in $(PROGRAMS); do \
	    name=$$(basename $$f .elf); \
	    cp $$f $(FS_DIR)/bin/$$name; \
	done

.PHONY: all clean install-fs

clean:
	rm -f $(LIBC_OBJS) $(LIBC_A) $(PROG_DIR)/*.o $(PROGRAMS)
	rm -rf $(FS_DIR)/bin/*
