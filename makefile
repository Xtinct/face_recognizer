CC=gcc
CFLAGS=-lSDL -lm
DEPS = shared_variables.h
PACKAGE_NAME = face_recognizer


obj/%.o: %.c $(DEPS)
	@echo "Compling libraries"
	$(CC) -c -o obj/$@ $< $(CFLAGS)

$(PACKAGE_NAME): camera_adapter.o filter_utils.o
	cc -o $(PACKAGE_NAME) camera_adapter.o filter_utils.o $(CFLAGS)
	rm *.o
clean:
	rm $(PACKAGE_NAME)
