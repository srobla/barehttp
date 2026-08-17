EXE = server

SRC_DIR = src
LIB_SRC_DIR = srclib
OBJ_DIR = obj
LIB_DIR = lib
INC_DIR = includes
DOC_DIR = doc

CFLAGS = -Wall -ansi -pedantic -g -std=c11 -I$(INC_DIR)

LIBS = $(LIB_DIR)/libpicohttpparser.a $(LIB_DIR)/libtcp.a $(LIB_DIR)/libhttp.a $(LIB_DIR)/libqueue.a $(LIB_DIR)/liblogs.a $(LIB_DIR)/libqueue.a
OBJS = $(OBJ_DIR)/server.o

COMP_LIBS = -lconfuse -lpthread -lhttp -lpicohttpparser -ltcp -ltcp -lqueue -llogs

all: dirs $(LIBS) $(EXE)

libs: $(LIBS)

dirs:
	mkdir -p $(LIB_DIR) $(OBJ_DIR)

docs:
	doxygen $(DOC_DIR)/Doxyfile 2> /dev/null > /dev/null

# Compilamos server.c 
$(OBJ_DIR)/server.o: $(SRC_DIR)/server.c
	gcc $(CFLAGS) -c $< -o $@

# Picoparser lo compilamos sin flags por que da muchos errores
$(OBJ_DIR)/picohttpparser.o: $(LIB_SRC_DIR)/picohttpparser.c
	gcc -I$(INC_DIR) -c $< -o $@

# Compilamos objetos de librerías
$(OBJ_DIR)/%.o: $(LIB_SRC_DIR)/%.c
	gcc $(CFLAGS) -c $< -o $@

# Creamos las librerías estáticas
$(LIB_DIR)/libpicohttpparser.a: $(OBJ_DIR)/picohttpparser.o
	ar rcs $@ $^

$(LIB_DIR)/libtcp.a: $(OBJ_DIR)/tcp.o
	ar rcs $@ $^

$(LIB_DIR)/libhttp.a: $(OBJ_DIR)/http.o
	ar rcs $@ $^

$(LIB_DIR)/libqueue.a: $(OBJ_DIR)/queue.o
	ar rcs $@ $^

$(LIB_DIR)/liblogs.a: $(OBJ_DIR)/logs.o
	ar rcs $@ $^

# Enlazamos en el ejecutable final
$(EXE): $(OBJS) libs
	gcc $(OBJS) -L$(LIB_DIR) $(COMP_LIBS) -o $@

clean: clean_logs clean_libs
	rm -f $(EXE) $(OBJS) $(OBJ_DIR)/*.o

clean_logs:
	rm -f logs/*.log

clean_libs:
	rm -f lib/*.a

clean_doc:
	rm -r -f doc/html
