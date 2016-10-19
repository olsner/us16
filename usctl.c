#include <stdio.h>
#include <stdlib.h>

#include <libusb.h>

// the URB request/type to control Tascam mixers
#define SND_US16X08_URB_REQUEST 0x1D
// B7 = 0 = Host-to-Device, B6..5 = 2 = Vendor, B4..0 = 0 = To device
#define SND_US16X08_URB_REQUESTTYPE 0x40

// Common Channel control IDs
#define SND_US16X08_ID_PHASE 0x85
// 0x84
#define SND_US16X08_ID_MUTE 0x83
#define SND_US16X08_ID_FADER 0x81
#define SND_US16X08_ID_PAN 0x82

// EQ level IDs
#define SND_US16X08_ID_EQLOWLEVEL 0x01
#define SND_US16X08_ID_EQLOWMIDLEVEL 0x02
#define SND_US16X08_ID_EQHIGHMIDLEVEL 0x03
#define SND_US16X08_ID_EQHIGHLEVEL 0x04

// EQ frequence IDs
#define SND_US16X08_ID_EQLOWFREQ 0x11
#define SND_US16X08_ID_EQLOWMIDFREQ 0x12
#define SND_US16X08_ID_EQHIGHMIDFREQ 0x13
#define SND_US16X08_ID_EQHIGHFREQ 0x14

// EQ width IDs
#define SND_US16X08_ID_EQLOWMIDWIDTH 0x22
#define SND_US16X08_ID_EQHIGHMIDWIDTH 0x23

#define SND_US16X08_ID_EQENABLE 0x31

// Compressor Ids
#define SND_US16X08_ID_COMP_THRESHOLD 0x01
#define SND_US16X08_ID_COMP_RATIO 0x02
#define SND_US16X08_ID_COMP_ATTACK 0x03
#define SND_US16X08_ID_COMP_RELEASE 0x04
#define SND_US16X08_ID_COMP_GAIN 0x05
#define SND_US16X08_ID_COMP_SWITCH 0x06

// Index 5 is always (?) the channel ID
#define CTLMSGHEAD 0x61, 0x02, 0x04, 0x62, 0x02

/* Theory of message format:
 * byte 0: message/command/field
 * byte 1: total data length (including length byte)
 *   or perhaps data type, 1 = no data / "true"?, 2 = byte data
 * byte 2..: data (if length > 1)
 *
 * commands/fields:
 *   0x61: category/thing, 1 byte data
 *      4 = effects stuff
 *      1,2,3 somehow related to routing
 *   0x62: channel, 1 byte data
 *
 *   0x4x: routing related somehow? (no data)
 *  
 *   0x31
 *   0x71: seem to be sent all the time when the control app is opened
 */

/*
// Out-channel 1, set to Master L (I think)
	CTLMSGHEAD with 0x03 instead of 0x04
61 02 03 62 02 01 41 01
               ^ channel = 1
                  ^ controller = 41 (probably 41..48?)
                     ^ dummy? input (master l?)


61 02 01 62 02 01 42 01
                  43 01
                  00 00
// End of message 1

// Set to computer 1 (I think)
61 02 02
62 02 01
41 01
61 02 01
62 02 01
42 01
43 01
00 00
// End of message

// Set output channel 8 to master L?
61 02 03
62 02 08
41 01
61 02 01
62 02 08
42 01
43 01 00 00
//

*/

static libusb_context* ctx;
static libusb_device_handle *h;
static libusb_device *dev;
static int was_kernel_active;

#define LIBUSB_ERROR(err, msg) \
	do { \
		fprintf(stderr, "libusb error %d: %s in %s\n", err, libusb_error_name(err), msg); \
	} while (0)
#define CHECK_LIBUSB(expr) \
	do { \
		int e__ = (expr); \
		if (e__) { \
			LIBUSB_ERROR(e__, #expr); \
            abort(); \
		} \
	} while (0)

#define LIBUSB_(func, ...) \
	CHECK_LIBUSB(libusb_##func(__VA_ARGS__))

const int CTLMSG_TIMEOUT = 1000;

void send_urb(const char* buf, int size) {
	int res = libusb_control_transfer(h,
			SND_US16X08_URB_REQUEST,
			SND_US16X08_URB_REQUESTTYPE,
			0, 0,
			(char *)buf,
			size,
			CTLMSG_TIMEOUT);
    if (res != size) {
        LIBUSB_ERROR(res, "send_urb");
    }
}

// Channel: one-based,
// controller: mixer constant above,
// value: 0..255 or 0..1 or -128..127
void mix(int channel, int controller, int value) {
	char buf[] = {
		CTLMSGHEAD,
		channel,
		controller,
		0x02, //                    0x07: unknown
		value,
		0x00, 
		0x00
	};

	send_urb(buf, sizeof(buf));
}

int main() {
	LIBUSB_(init, &ctx);
	libusb_set_debug(ctx, 3);

	h = libusb_open_device_with_vid_pid(ctx, 0x0644, 0x8047);
	if (!h) {
		fprintf(stderr, "No US-16x08 found.\n");
		abort();
	}
	dev = libusb_get_device(h);
	if (!dev) {
	}
	printf("Found US-16x08 at %d:%d\n",
			libusb_get_bus_number(dev),
			libusb_get_device_address(dev));

	// US16x08 URBs are send to "control pipe" (linux usb thing?) for endpoint
	// 0. But which interface?

	const int IFACENUM = 0;

	was_kernel_active = libusb_kernel_driver_active(h, IFACENUM);
	if (!(was_kernel_active == 0 || was_kernel_active == 1)) {
		LIBUSB_ERROR(was_kernel_active, "driver_active");
        abort();
	}
	if (was_kernel_active) {
		LIBUSB_(detach_kernel_driver, h, IFACENUM);
		printf("Detached kernel driver.\n");
	}

	mix(9, SND_US16X08_ID_FADER, 255);
	mix(9, SND_US16X08_ID_MUTE, 0);

#define MSGCAT 0x61, 0x02,
#define MSGCH 0x62, 0x02,
#define EOM 0x00, 0x00

    // I wonder what selects left/right here, because only the channel number
    // differs...
#define set_computer_l(ch) \
    MSGCAT 2, MSGCH ch, 0x41, 1, \
    MSGCAT 1, MSGCH ch, 0x42, 1, 0x43, 1, \
    EOM
#define set_master_l(ch) \
    MSGCAT 3, MSGCH ch, 0x41, 1, \
    MSGCAT 1, MSGCH ch, 0x42, 1, 0x43, 1, \
    EOM

    static const uint8_t msg[] = { set_master_l(1), };

    send_urb(msg, sizeof(msg));

	if (was_kernel_active) {
		printf("Done, reattaching kernel driver.\n");
		LIBUSB_(attach_kernel_driver, h, IFACENUM);
	}

	libusb_close(h);
	libusb_exit(ctx);
}