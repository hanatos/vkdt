.PHONY:all clean pipe qvk cli pipe-clean qvk-clean cli-clean
all: pipe qvk cli Makefile

pipe:
	make -C pipe
qvk:
	make -C qvk
cli: qvk pipe
	make -C cli

clean: pipe-clean qvk-clean cli-clean Makefile

pipe-clean:
	make -C pipe clean
qvk-clean:
	make -C qvk clean
cli-clean:
	make -C cli clean
