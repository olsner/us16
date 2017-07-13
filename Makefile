CFLAGS = `pkg-config --cflags libusb-1.0`
LIBS = `pkg-config --libs libusb-1.0`

all: usctl

clean::
	rm -f usctl

%: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)
