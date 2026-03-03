SRCDIR ?= .
OBJ_DIR ?= .
LIBRARY ?= libdecFloat$(CROSS).a

SOURCES := $(filter-out $(SRCDIR)/decBasic.c $(SRCDIR)/decCommon.c,$(wildcard $(SRCDIR)/*.c))
HEADERS := $(wildcard $(SRCDIR)/*.h)
OBJECTS := $(addprefix $(OBJ_DIR)/,$(notdir $(SOURCES:.c=.o)))

$(LIBRARY): $(SOURCES) $(HEADERS) Makefile
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(dir $(LIBRARY))
	$(RM) -f $(OBJECTS)
	$(MAKE) $(OBJECTS) SRCDIR=$(SRCDIR) OBJ_DIR=$(OBJ_DIR)
	$(AR) crs $(LIBRARY) $(OBJECTS)

$(OBJ_DIR)/%.o: $(SRCDIR)/%.c Makefile
	$(CC) $(CROSS_FLAGS) -O3 -fPIC -c $< -o $@

.PHONY:	clean
clean:
	$(RM) -f $(OBJ_DIR)/*.o $(LIBRARY)
