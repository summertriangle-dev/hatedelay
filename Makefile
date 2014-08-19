extb:
	${CC} -o extb extb.c lodepng.c -lz

hatedelay:
	true

clean:
	rm -f extb hatedelay
