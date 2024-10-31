# Nome del programma
TARGET = bin/control_center

# Compilatore
CC = g++
IDIR=../con2redis/src/
# Opzioni di compilazione
CFLAGS = -Wall -std=c++20 -lm -lhiredis -Wall -I. -I$(IDIR) -I/usr/include/hiredis -fpermissive

# Directory dei file sorgente
SRCDIR = src
ODIR = obj
SOURCES = $(wildcard $(SRCDIR)/*.cpp)

# Directory per i file oggetto
OBJDIR = obj
OBJECTS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES))
DEPS = $(wildcard $(SRCDIR)/*.h)
con2redis=$(wildcard ../con2redis/obj/*.o)

# Comando per creare il programma eseguibile
$(TARGET): $(OBJECTS)
	$(CC)  $(con2redis) $(OBJECTS)  -o $(TARGET) $(CFLAGS)

# Compila i file oggetto
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(DEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Target per la pulizia dei file oggetto e del programma eseguibile
.PHONY: clean
clean:
	rm -f $(OBJDIR)/*.o $(TARGET)


all: $(Target)