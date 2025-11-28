# Define o nome do executável final
TARGET = controle_aereo

# Define o compilador C
CC = gcc

# Flags do compilador:
# -Wall: Habilita todos os warnings
# -pthread: Necessário para linkar com a biblioteca Pthreads (threads, mutexes, semaforos)
# -g: Inclui informações de debug
CFLAGS = -Wall -pthread -g

# Diretórios que contêm os arquivos de código-fonte (.c) e cabeçalho (.h)
SRCDIRS = aeronave controle rota setor utils
INCDIRS = $(SRCDIRS)

# Encontra todos os arquivos .c em todos os diretórios de código-fonte e na raiz (main.c)
C_SOURCES = $(shell find $(SRCDIRS) -name "*.c") main.c

# Converte a lista de arquivos .c para uma lista de arquivos .o (objetos)
OBJECTS = $(C_SOURCES:.c=.o)

# Lista de flags -I (Include) para o pré-processador
INCLUDES = $(addprefix -I, $(INCDIRS))

# ---------------------------------------------------------------------
# Regras Principais
# ---------------------------------------------------------------------

# Regra default: constrói o TARGET (executável)
.PHONY: all
all: $(TARGET)

# 1. Regra de Linkagem: Cria o executável a partir dos arquivos objeto
$(TARGET): $(OBJECTS)
	@echo "🔗 Linking $@"
	$(CC) $(CFLAGS) $^ -o $@

# 2. Regra de Compilação: Converte cada arquivo .c em .o
# O Makefile usa esta regra genérica para qualquer arquivo .o
# Exemplo: compila aeronave/aeronave.c para aeronave/aeronave.o
%.o: %.c
	@echo "Compiling $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ---------------------------------------------------------------------
# Regras Secundárias
# ---------------------------------------------------------------------

# Regra de limpeza: Remove todos os arquivos objeto e o executável
.PHONY: clean
clean:
	@echo "🧹 Cleaning up..."
	# Remove objetos dos subdiretórios
	rm -f $(OBJECTS)
	# Remove o executável
	rm -f $(TARGET)