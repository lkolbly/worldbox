build:
	make -C src/
	mv src/a.out .

clean:
	make -C src/ clean
