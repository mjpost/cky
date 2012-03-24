all:
	make -C src/ llncky
	mkdir bin
	cp src/llncky bin/cky

clean:
	make -C src/ clean
	rm -rf bin
