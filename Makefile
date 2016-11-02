all: extb extb_cut btxe

.PHONY: all clean

extb: extb.c pixel.c lodepng.c
	${CC} ${CFLAGS} -Wno-multichar -std=c99 -Ofast -o $@ $< lodepng.c -lz

btxe: btxe.c
	${CC} ${CFLAGS} -Wno-multichar -std=c99 -o $@ $< lodepng.c -lz

extb_cut: extb_cut.c lodepng.c
	${CC} ${CFLAGS} -isysroot `xcrun --show-sdk-path` \
		`pkg-config --cflags glfw3` `pkg-config --static --libs glfw3` \
		-std=c99 -Ofast -o $@ $< lodepng.c

clean:
	rm -f extb extb_cut btxe
