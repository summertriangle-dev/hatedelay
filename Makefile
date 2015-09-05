all: extb extb_cut btxe

.PHONY: all extb extb_cut clean btxe

extb:
	${CC} ${CFLAGS} -o extb extb.c lodepng.c -lz

btxe:
	${CC} ${CFLAGS} -o btxe btxe.c lodepng.c -lz

extb_cut:
	${CC} ${CFLAGS} -isysroot `xcrun --show-sdk-path` \
		`pkg-config --cflags glfw3` `pkg-config --static --libs glfw3` \
		-o extb_cut extb_cut.c lodepng.c

clean:
	rm -f extb extb_cut
