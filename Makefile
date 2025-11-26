# === Configuración ===
TARGET    := p2
SRC       := p2.c comandos.c lista.c ficheros.c memoria.c procesos.c
OBJ       := $(SRC:.c=.o)
DEP       := $(OBJ:.o=.d)

CC        := gcc
STD       := -std=c11
WARN      := -Wall -Wextra -Wpedantic -Wshadow -Wconversion
OPT       := -O2
DBG       :=
SAN       :=
CPPFLAGS  :=
CFLAGS    := $(STD) $(WARN) $(OPT) $(DBG) -MMD -MP
LDFLAGS   := $(SAN)
LDLIBS    :=

# Activa modo debug con: `make debug` o `make DEBUG=1`
ifeq ($(DEBUG),1)
	OPT     := -O0
	DBG     := -g
	SAN     := -fsanitize=address,undefined
	CFLAGS  := $(STD) $(WARN) $(OPT) $(DBG) -MMD -MP $(SAN)
	LDFLAGS := $(SAN)
endif

RM ?= rm -f

.PHONY: all clean run debug

# === Reglas ===
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

# Regla genérica de compilación; las dependencias de headers las genera -MMD -MP
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Incluir dependencias auto-generadas (.d)
-include $(DEP)

# Ejecutar el binario
run: $(TARGET)
	./$(TARGET)

# Construcción en modo debug (equivalente a make DEBUG=1)
debug:
	$(MAKE) DEBUG=1

# Limpieza
clean:
	$(RM) $(OBJ) $(DEP) $(TARGET)
