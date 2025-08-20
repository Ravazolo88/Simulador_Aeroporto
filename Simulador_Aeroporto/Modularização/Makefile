CC = gcc

CFLAGS = -Wall -Wextra -g -Iheaders

LDFLAGS = -pthread -lncurses

MAIN_DIR = maincode
INC_DIR = headers
OBJ_DIR = obj
BIN_DIR = bin

EXEC_NAME = Airport-Traffic-Control
TARGET = $(BIN_DIR)/$(EXEC_NAME)
MAINS = $(wildcard $(MAIN_DIR)/*.c)
OBJS = $(patsubst $(MAIN_DIR)/%.c,$(OBJ_DIR)/%.o,$(MAINS))


.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "--- Linkando para criar o executável: $(TARGET) ---"
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(MAIN_DIR)/%.c
	@echo "--- Compilando $< em $@ ---"
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: run
run: all
	@echo "--- Executando o Simulador ---"
	@./$(TARGET) 1 3 5 2 120 60 90

.PHONY: clean
clean:
	@echo "--- Limpando arquivos compilados e diretórios de build ---"
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "--- Limpeza finalizada ---"