
pwindow_manager:
	make -C ./source_code

clean:
	rm -f pwindow_manager
	make -C source_code clean


install:
	pkill pwindow_manager
	sleep 1
	cp pwindow_manager /usr/bin
	pwindow_manager &

uninstall:
	rm -f /usr/bin/pwindow_manager

.PHONY: pwindow_manager
