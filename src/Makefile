.PHONY:all clean pipe qvk cli gui pipe-clean qvk-clean cli-clean gui-clean
all: pipe qvk cli gui Makefile

pipe:
	make -C pipe
qvk:
	make -C qvk
cli: qvk pipe
	make -C cli
gui: qvk pipe
	make -C gui

clean: pipe-clean qvk-clean cli-clean gui-clean Makefile

pipe-clean:
	make -C pipe clean
qvk-clean:
	make -C qvk clean
cli-clean:
	make -C cli clean
gui-clean:
	make -C gui clean
