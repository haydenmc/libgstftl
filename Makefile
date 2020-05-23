CC=gcc
RM=rm -f
CFLAGS=-Wall -fPIC -DVERSION=\"1.14.5\" -DVERSION_MAJOR=1 -DVERSION_MINOR=14 -DPACKAGE_NAME=\"gstftl\" -DPACKAGE=\"gstftl\" -DPACKAGE_ORIGIN=\"https://github.com/heftig\" $(shell pkg-config --cflags gstreamer-1.0 gstreamer-base-1.0 libftl)
LDFLAGS=-fPIC
LDLIBS=$(shell pkg-config --libs gstreamer-1.0 gstreamer-base-1.0 libftl)

SRCS=gstftl.c gstftlaudiosink.c gstftlenums.c gstftlsink.c gstftlvideosink.c
OBJS=$(subst .c,.o,$(SRCS))

ifeq ($(PREFIX),)
    PREFIX := /usr
endif

ifeq ($(ARCH),)
    ARCH := /aarch64-linux-gnu
endif

all: libgstftl.so

libgstftl.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o libgstftl.so $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) libgstftl.so

install: libgstftl.so
	install -d $(DESTDIR)$(PREFIX)/lib$(ARCH)/gstreamer-1.0/
	install -m 644 libgstftl.so $(DESTDIR)$(PREFIX)/lib$(ARCH)/gstreamer-1.0/