# Nome del programma
TARGET = bin/drone_simulation

# Compilatore
CC = g++
IDIR=../con2redis/src
# Flags di compilazione
CFLAGS = -Wall -std=c++20 -I. -I$(IDIR) -I/opt/homebrew/include/ -fpermissive

# Flags di linking
LDFLAGS = -L/opt/homebrew/lib -lhiredis -lm
# Directory dei file sorgente
SRCDIR = src
ODIR = obj
SOURCES = $(wildcard $(SRCDIR)/*.cpp)

# Directory per i file oggetto
OBJDIR = obj
OBJECTS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES))
DEPS = $(wildcard $(SRCDIR)/*.h)
CON2REDIS_OBJECTS = ../con2redis/obj/readreply.o ../con2redis/obj/redisfun.o

# Comando per creare il programma eseguibile
$(TARGET): $(OBJECTS) $(CON2REDIS_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(OBJECTS) $(CON2REDIS_OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compila i file oggetto
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(DEPS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Target per la pulizia dei file oggetto e del programma eseguibile
.PHONY: clean
clean:
	rm -f $(OBJDIR)/*.o $(TARGET)


all: $(Target)