
pwindow_manager:
	make -C ./source_code

clean:
	rm -f pwindow_manager
	make -C source_code clean

install:
	make -C source_code install
