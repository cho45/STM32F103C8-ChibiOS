#include "ch.h"
#include "hal.h"

#include "font_6x13.h"

// maybe GBRG or GRBG
//#define RGB565(rgb888)     ( ((( (rgb888)&0xf80000)>>8) | (((rgb888)&0xfc00)>>5) | (((rgb888)&0xf8)>>3)) )
#define RGB565(rgb888)     (( (rgb888) & 0x1c00) << 3)|(( (rgb888) & 0xf8) << 5)|(( (rgb888) & 0xf80000) >> 16)|(((rgb888) & 0xe000) >> 13)
#define FG(rgb) "\e\x01" rgb
#define BG(rgb) "\e\x02" rgb

#define ST7735_NOP        0x00
#define ST7735_SWRESET    0x01
#define ST7735_RDDID      0x04
#define ST7735_RDDST      0x09

#define ST7735_SLPIN      0x10
#define ST7735_SLPOUT     0x11
#define ST7735_PTLON      0x12
#define ST7735_NORON      0x13

#define ST7735_INVOFF     0x20
#define ST7735_INVON      0x21
#define ST7735_DISPOFF    0x28
#define ST7735_DISPON     0x29
#define ST7735_CASET      0x2A
#define ST7735_RASET      0x2B
#define ST7735_RAMWR      0x2C
#define ST7735_RAMRD      0x2E

#define ST7735_PTLAR      0x30
#define ST7735_TEOFF      0x34
#define ST7735_TEON       0x35
#define ST7735_MADCTL     0x36
#define ST7735_COLMOD     0x3A

#define ST7735_MADCTL_MY  0x80
#define ST7735_MADCTL_MX  0x40
#define ST7735_MADCTL_MV  0x20
#define ST7735_MADCTL_ML  0x10
#define ST7735_MADCTL_RGB 0x00

#define ST7735_RDID1      0xDA
#define ST7735_RDID2      0xDB
#define ST7735_RDID3      0xDC
#define ST7735_RDID4      0xDD


#define ST7735_MADCTL_BGR 0x08
#define ST7735_MADCTL_MH  0x04

#define ST7735_FRMCTR1    0xB1
#define ST7735_FRMCTR2    0xB2
#define ST7735_FRMCTR3    0xB3
#define ST7735_INVCTR     0xB4
#define ST7735_DISSET5    0xB6

#define ST7735_PWCTR1     0xC0
#define ST7735_PWCTR2     0xC1
#define ST7735_PWCTR3     0xC2
#define ST7735_PWCTR4     0xC3
#define ST7735_PWCTR5     0xC4
#define ST7735_VMCTR1     0xC5

#define ST7735_PWCTR6     0xFC

#define ST7735_GMCTRP1    0xE0
#define ST7735_GMCTRN1    0xE1

// SPI_REMAP = 1
#define LINE_ST7735_SCK   PAL_LINE(GPIOB, 3)
#define LINE_ST7735_MISO  PAL_LINE(GPIOB, 4)
#define LINE_ST7735_MOSI  PAL_LINE(GPIOB, 5)
#define LINE_ST7735_NSS   PAL_LINE(GPIOA, 15)
#define LINE_ST7735_DC    PAL_LINE(GPIOB, 6)
#define LINE_ST7735_RESET PAL_LINE(GPIOB, 7)

static int16_t offsetX = 0;
static int16_t offsetY = 0;

//
// SPID1
//   MOSI: PB5
//   MISO: PB4
//   SCK : PB3
static const SPIConfig st7735_spicfg = {
	.circular = false,
	.end_cb = NULL,
	.ssport = GPIOA,
	.sspad = 15, // PA15
	.cr1 = (0b001 << SPI_CR1_BR_Pos),
	.cr2 = 0 // SPI_CR2_SSOE
};

static void st7735_command_mode(void) {
	palClearLine(LINE_ST7735_DC);
}

static void st7735_data_mode(void) {
	palSetLine(LINE_ST7735_DC);
}

static void st7735_send_command(const uint8_t command, const uint32_t len, const uint8_t *data) {
	char tx;

	spiStart(&SPID1, &st7735_spicfg);
	spiSelect(&SPID1);

	st7735_command_mode();
	tx = command;
	spiStartSend(&SPID1, 1, &tx);
	while (SPID1.state != SPI_READY) chThdYield();

	st7735_data_mode();
	if (len) {
		spiStartSend(&SPID1, len, data);
		while (SPID1.state != SPI_READY) chThdYield();
	}

	spiUnselect(&SPID1);
	spiStop(&SPID1);
}

void st7735_init(void) {

	// MOSI
	palSetLineMode(LINE_ST7735_MOSI, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
	// MISO (not used)
	palSetLineMode(LINE_ST7735_MISO, PAL_MODE_INPUT_PULLUP);
	// SCK
	palSetLineMode(LINE_ST7735_SCK, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
	// NSS (Slave Select)
	palSetLineMode(LINE_ST7735_NSS, PAL_MODE_OUTPUT_PUSHPULL);
	// DC Data/Command Select (A0)
	palSetLineMode(LINE_ST7735_DC, PAL_MODE_OUTPUT_PUSHPULL); 
	// Reset
	palSetLineMode(LINE_ST7735_RESET, PAL_MODE_OUTPUT_PUSHPULL); 

	spiObjectInit(&SPID1);

	palSetLine(LINE_ST7735_RESET);
	chThdSleepMilliseconds(100);
	palClearLine(LINE_ST7735_RESET);
	chThdSleepMilliseconds(100);
	palSetLine(LINE_ST7735_RESET);
	chThdSleepMilliseconds(200);

	st7735_send_command(ST7735_SWRESET, 0, NULL);
	chThdSleepMilliseconds(150);

	st7735_send_command(ST7735_SLPOUT, 0, NULL);
	chThdSleepMilliseconds(500);

	st7735_send_command(ST7735_FRMCTR1, 3, (uint8_t*)"\x01\x2c\x2d");
	st7735_send_command(ST7735_FRMCTR2, 3, (uint8_t*)"\x01\x2c\x2d");
	st7735_send_command(ST7735_FRMCTR3, 6, (uint8_t*)"\x01\x2c\x2d" "\x01\x2c\x2d");
	st7735_send_command(ST7735_INVCTR, 1, (uint8_t*)"\x07");
	st7735_send_command(ST7735_PWCTR1, 3, (uint8_t*)"\xa2\x02\x84");
	st7735_send_command(ST7735_PWCTR2, 1, (uint8_t*)"\xc5");
	st7735_send_command(ST7735_PWCTR3, 2, (uint8_t*)"\x0a\x00");
	st7735_send_command(ST7735_PWCTR4, 2, (uint8_t*)"\x8a\x2a");
	st7735_send_command(ST7735_PWCTR5, 2, (uint8_t*)"\x8a\xee");
	st7735_send_command(ST7735_VMCTR1, 1, (uint8_t*)"\x0e");
	st7735_send_command(ST7735_INVOFF, 0, NULL);
	st7735_send_command(ST7735_MADCTL, 1, (uint8_t*)"\x00");
	st7735_send_command(ST7735_COLMOD, 1, (uint8_t*)"\x05");

	// column address 0 - 127
	st7735_send_command(ST7735_CASET, 4, (uint8_t*)"\x00\x00\x00\x7f");
	// row address 0 - 127
	st7735_send_command(ST7735_RASET, 4, (uint8_t*)"\x00\x00\x00\x7f");

	st7735_send_command(ST7735_GMCTRP1, 16, (uint8_t*)"\x02\x1c\x07\x12\x37\x32\x29\x2d\x29\x25\x2B\x39\x00\x01\x03\x10");
	st7735_send_command(ST7735_GMCTRN1, 16, (uint8_t*)"\x03\x1d\x07\x06\x2E\x2C\x29\x2D\x2E\x2E\x37\x3F\x00\x00\x02\x10");

	st7735_send_command(ST7735_NORON, 0, NULL);
	chThdSleepMilliseconds(10);

	st7735_send_command(ST7735_DISPON, 0, NULL);
	chThdSleepMilliseconds(100);
}

void st7735_setAddrWindow(const uint16_t x, const uint16_t y, const uint16_t w, const uint16_t h) {
	uint8_t tx[4];

	tx[0] = x >> 8;
	tx[1] = x & 0xff;
	tx[2] = (x+w-1) >> 8;
	tx[3] = (x+w-1) & 0xff;
	st7735_send_command(ST7735_CASET, 4, tx);

	tx[0] = y >> 8;
	tx[1] = y & 0xff;
	tx[2] = (y+h-1) >> 8;
	tx[3] = (y+h-1) & 0xff;
	st7735_send_command(ST7735_RASET, 4, tx);
}

void st7735_beginDrawing(uint16_t x, uint16_t y, const uint16_t w, const uint16_t h) {
	char tx;
	x += offsetX;
	y += offsetY;

	st7735_setAddrWindow(x, y, w, h);

	spiStart(&SPID1, &st7735_spicfg);
	spiSelect(&SPID1);
	tx = ST7735_RAMWR;
	st7735_command_mode();
	spiStartSend(&SPID1, 1, &tx);
	while (SPID1.state != SPI_READY) chThdYield();

	st7735_data_mode();
}

void st7735_drawPixel(const uint16_t pixel) {
	spiStartSend(&SPID1, 2, &pixel);
	while (SPID1.state != SPI_READY) chThdYield();
}

void st7735_drawPixels(const uint16_t len, const uint16_t* pixels) {
	spiStartSend(&SPID1, len * 2, pixels);
	while (SPID1.state != SPI_READY) chThdYield();
}

void st7735_endDrawing(void) {
	spiUnselect(&SPID1);
	spiStop(&SPID1);
}

void st7735_fillRect(const uint16_t x, const uint16_t y, const uint16_t w, const uint16_t h, const uint16_t color) {
	st7735_beginDrawing(x, y, w, h);
	for (int i = 0; i< w*h; i++) {
		st7735_drawPixel(color);
	}
	st7735_endDrawing();
}

void st7735_setRotate(const uint8_t m) {
	uint8_t madctl;
	switch (m % 4) {
		case 0:
			madctl  = ST7735_MADCTL_MX | ST7735_MADCTL_MY | ST7735_MADCTL_BGR;
			offsetX = 2;
			offsetY = 3;
			break;
		case 1:
			madctl  = ST7735_MADCTL_MY | ST7735_MADCTL_MV | ST7735_MADCTL_BGR;
			offsetX = 0;
			offsetY = 0;
			break;
		case 2:
			madctl  = ST7735_MADCTL_BGR;
			offsetX = 0;
			offsetY = 0;
			break;
		case 3:
			madctl  = ST7735_MADCTL_MX | ST7735_MADCTL_MV | ST7735_MADCTL_BGR;
			offsetX = 0;
			offsetY = 0;
			break;
	}
	st7735_send_command(ST7735_MADCTL, 1, &madctl);
}


void st7735_drawCharScaled(const uint16_t x, const uint16_t y, const uint8_t chr, const uint16_t fg, const uint16_t bg, const uint8_t scaleX, const uint8_t scaleY) {
	uint16_t _buf[font_6x13_height * font_6x13_width * scaleX * scaleY];
	uint16_t* buf = &_buf[0];

	const uint8_t* glyph = font_6x13[chr];
	for (int y = 0; y < font_6x13_height * scaleY; y++) {
		for (int x = 0; x < font_6x13_width * scaleX; x++) {
			*buf++ = glyph[y / scaleY] >> (8 - (x / scaleX)) & 1 ?  fg : bg;
		}
	}

	st7735_beginDrawing(x, y, font_6x13_width * scaleX, font_6x13_height * scaleY);
	st7735_drawPixels(font_6x13_height * font_6x13_width * scaleX * scaleY, _buf);
	st7735_endDrawing();
}

void st7735_drawStringScaled(uint16_t x, uint16_t y, const uint16_t len, const char* string, uint16_t fg, uint16_t bg, const uint8_t scaleX, const uint8_t scaleY) {
	uint16_t x0 = x;
	for (int i = 0; i < len; i++) {
		uint8_t chr = string[i];
		if (chr == '\r') {
			y += font_6x13_height * scaleY;
			continue;
		} else
		if (chr == '\n') {
			y += font_6x13_height * scaleY;
			x = x0;
			continue;
		} else
		if (chr == '\e') {
			uint8_t t = string[++i];
			if (t == 1) {
				uint32_t r = string[++i]<<16;
				uint32_t g = string[++i]<<8;
				uint32_t b = string[++i];
				fg = RGB565(r | g | b);
			} else
			if (t == 2) {
				uint32_t r = string[++i]<<16;
				uint32_t g = string[++i]<<8;
				uint32_t b = string[++i];
				bg = RGB565(r | g | b);
			} else
			if (t == 3) {
				uint16_t tmp = fg;
				fg = bg;
				bg = tmp;
			}
			continue;
		}
		st7735_drawCharScaled(x, y, chr, fg, bg, scaleX, scaleY);
		x += font_6x13_width * scaleX;
	}
}

void st7735_drawString(const uint16_t x, const uint16_t y, const uint16_t len, const char* string, const uint16_t fg, const uint16_t bg) {
	st7735_drawStringScaled(x, y, len, string, fg, bg, 1, 1);
}

void st7735_draw7SegChar(const uint16_t x, const uint16_t y, uint8_t chr, const uint16_t on, const uint16_t off, const uint16_t bg) {
#define W 10
#define H 14
#define POS(y, x) buf[y * W + x]
	uint16_t buf[W * H];
	struct {
		unsigned A: 1;
		unsigned B: 1;
		unsigned C: 1;
		unsigned D: 1;
		unsigned E: 1;
		unsigned F: 1;
		unsigned G: 1;
		unsigned DP: 1;
	} seg = {0, 0, 0, 0, 0, 0, 0, 0};

	if (chr & (1<<7)) {
		seg.DP = 1;
		chr &= 0b01111111;
	}

	switch (chr) {
		case '0':
			seg.A = 1;
			seg.B = 1;
			seg.C = 1;
			seg.D = 1;
			seg.E = 1;
			seg.F = 1;
			break;
		case '1':
			seg.B = 1;
			seg.C = 1;
			break;
		case '2':
			seg.A = 1;
			seg.B = 1;
			seg.D = 1;
			seg.E = 1;
			seg.G = 1;
			break;
		case '3':
			seg.A = 1;
			seg.B = 1;
			seg.C = 1;
			seg.D = 1;
			seg.G = 1;
			break;
		case '4':
			seg.B = 1;
			seg.C = 1;
			seg.F = 1;
			seg.G = 1;
			break;
		case '5':
			seg.A = 1;
			seg.C = 1;
			seg.D = 1;
			seg.F = 1;
			seg.G = 1;
			break;
		case '6':
			seg.A = 1;
			seg.C = 1;
			seg.D = 1;
			seg.E = 1;
			seg.F = 1;
			seg.G = 1;
			break;
		case '7':
			seg.A = 1;
			seg.B = 1;
			seg.C = 1;
			break;
		case '8': 
			seg.A = 1;
			seg.B = 1;
			seg.C = 1;
			seg.D = 1;
			seg.E = 1;
			seg.F = 1;
			seg.G = 1;
			break;
		case '9':
			seg.A = 1;
			seg.B = 1;
			seg.C = 1;
			seg.D = 1;
			seg.F = 1;
			seg.G = 1;
			break;
	}

	for (int i = 0; i < W * H; i++) {
		buf[i] = bg;
	}

	POS(0, 2) = seg.A ? on : off;
	POS(0, 3) = seg.A ? on : off;
	POS(0, 4) = seg.A ? on : off;
	POS(0, 5) = seg.A ? on : off;
	POS(1, 3) = seg.A ? on : off;
	POS(1, 4) = seg.A ? on : off;

	POS(1, 7) = seg.B ? on : off;
	POS(2, 6) = seg.B ? on : off;
	POS(2, 7) = seg.B ? on : off;
	POS(3, 6) = seg.B ? on : off;
	POS(3, 7) = seg.B ? on : off;
	POS(4, 6) = seg.B ? on : off;
	POS(4, 7) = seg.B ? on : off;

	POS(8, 7) = seg.C ? on : off;
	POS(9, 6) = seg.C ? on : off;
	POS(9, 7) = seg.C ? on : off;
	POS(10, 6) = seg.C ? on : off;
	POS(10, 7) = seg.C ? on : off;
	POS(11, 6) = seg.C ? on : off;
	POS(11, 7) = seg.C ? on : off;
	POS(12, 7) = seg.C ? on : off;

	POS(12, 3) = seg.D ? on : off;
	POS(12, 4) = seg.D ? on : off;
	POS(13, 2) = seg.D ? on : off;
	POS(13, 3) = seg.D ? on : off;
	POS(13, 4) = seg.D ? on : off;
	POS(13, 5) = seg.D ? on : off;

	POS(9, 0) = seg.E ? on : off;
	POS(9, 1) = seg.E ? on : off;
	POS(10, 0) = seg.E ? on : off;
	POS(10, 1) = seg.E ? on : off;
	POS(11, 0) = seg.E ? on : off;
	POS(11, 1) = seg.E ? on : off;
	POS(12, 0) = seg.E ? on : off;

	POS(1, 0) = seg.F ? on : off;
	POS(2, 0) = seg.F ? on : off;
	POS(2, 1) = seg.F ? on : off;
	POS(3, 0) = seg.F ? on : off;
	POS(3, 1) = seg.F ? on : off;
	POS(4, 0) = seg.F ? on : off;
	POS(4, 1) = seg.F ? on : off;
	POS(5, 0) = seg.F ? on : off;

	POS(6, 2) = seg.G ? on : off;
	POS(6, 3) = seg.G ? on : off;
	POS(6, 4) = seg.G ? on : off;
	POS(6, 5) = seg.G ? on : off;
	POS(6, 6) = seg.G ? on : off;
	POS(7, 1) = seg.G ? on : off;
	POS(7, 2) = seg.G ? on : off;
	POS(7, 3) = seg.G ? on : off;
	POS(7, 4) = seg.G ? on : off;
	POS(7, 5) = seg.G ? on : off;

	POS(12, 9) = seg.DP ? on : off;
	POS(13, 9) = seg.DP ? on : off;

	st7735_beginDrawing(x, y, W, H);
	st7735_drawPixels(W * H, buf);
	st7735_endDrawing();
#undef POS
}

void st7735_draw7SegString(uint16_t x, const uint16_t y,  const uint16_t len, const char* string, const uint16_t on, const uint16_t off, const uint16_t bg) {
	for (int i = 0; i < len; i++) {
		uint8_t chr = string[i];
		if (i + 1 < len && string[i+1] == '.') {
			chr |= 1<<7;
			i++;
		}
		st7735_draw7SegChar(x, y, chr, on, off, bg);
		x += 11;
	}
}

