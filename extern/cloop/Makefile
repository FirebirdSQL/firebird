MODULES	:= cloop tests tests/test1

WITH_FPC	:= 1

TARGET	:= release

CC	:= $(CC)
CXX	:= $(CXX)
LD	:= $(CXX)

SRC_DIR		:= src
BUILD_DIR	:= build
OUT_DIR		:= output
SHRLIB_EXT	:= .so
EXE_EXT		:=

OBJ_DIR := $(BUILD_DIR)/$(TARGET)
BIN_DIR := $(OUT_DIR)/$(TARGET)/bin
LIB_DIR := $(OUT_DIR)/$(TARGET)/lib

SRC_DIRS := $(addprefix $(SRC_DIR)/,$(MODULES))
OBJ_DIRS := $(addprefix $(OBJ_DIR)/,$(MODULES))

SRCS_C := $(foreach sdir,$(SRC_DIRS),$(wildcard $(sdir)/*.c))
SRCS_CPP := $(foreach sdir,$(SRC_DIRS),$(wildcard $(sdir)/*.cpp))

OBJS_C := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS_C))
OBJS_CPP := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS_CPP))

C_FLAGS := -ggdb -fPIC -MMD -MP -W -Wall -Wno-unused-parameter
CXX_FLAGS := $(C_FLAGS)
FPC_FLAGS := -Mdelphi

ifeq ($(shell uname),FreeBSD)
	DL_LIB :=
else
	DL_LIB := -ldl
endif

ifeq ($(TARGET),release)
	CXX_FLAGS += -O3
endif

ifeq ($(shell uname),Darwin)
CXX_FLAGS += -stdlib=libc++
endif

ifeq ($(TARGET),debug)
	FPC_FLAGS += -g
endif

vpath %.c $(SRC_DIRS)
vpath %.cpp $(SRC_DIRS)

define compile
$1/%.o: %.c
	$(CC) -c $$(C_FLAGS) $$< -o $$@

$1/%.o: %.cpp
	$(CXX) -c $$(CXX_FLAGS) $$< -o $$@
endef

.PHONY: all mkdirs clean

all: mkdirs \
	$(BIN_DIR)/cloop	\
	$(BIN_DIR)/test1-c$(SHRLIB_EXT)	\
	$(BIN_DIR)/test1-c$(EXE_EXT)	\
	$(BIN_DIR)/test1-cpp$(SHRLIB_EXT)	\
	$(BIN_DIR)/test1-cpp$(EXE_EXT)	\
	$(BIN_DIR)/test1-pascal$(SHRLIB_EXT)	\
	$(BIN_DIR)/test1-pascal$(EXE_EXT)

mkdirs: $(OBJ_DIRS) $(BIN_DIR) $(LIB_DIR)

$(OBJ_DIRS) $(BIN_DIR) $(LIB_DIR):
	@mkdir -p $@

clean:
	@rm -rf $(BUILD_DIR) $(OUT_DIR)

$(foreach bdir,$(OBJ_DIRS),$(eval $(call compile,$(bdir))))

-include $(addsuffix .d,$(basename $(OBJS_C)))
-include $(addsuffix .d,$(basename $(OBJS_CPP)))

$(BIN_DIR)/cloop: \
	$(OBJ_DIR)/cloop/Expr.o \
	$(OBJ_DIR)/cloop/Generator.o \
	$(OBJ_DIR)/cloop/Lexer.o \
	$(OBJ_DIR)/cloop/Parser.o \
	$(OBJ_DIR)/cloop/Main.o \
	| $(BIN_DIR)

	$(LD) $^ -o $@ $(LIBS)

$(SRC_DIR)/tests/test1/CalcCApi.h: $(BIN_DIR)/cloop $(SRC_DIR)/tests/test1/Interface.idl
	$(BIN_DIR)/cloop $(SRC_DIR)/tests/test1/Interface.idl c-header $@ CALC_C_API_H CALC_I

$(SRC_DIR)/tests/test1/CalcCApi.c: $(BIN_DIR)/cloop $(SRC_DIR)/tests/test1/Interface.idl $(SRC_DIR)/tests/test1/CalcCApi.h
	$(BIN_DIR)/cloop $(SRC_DIR)/tests/test1/Interface.idl c-impl $@ CalcCApi.h CALC_I

$(SRC_DIR)/tests/test1/CalcCppApi.h: $(BIN_DIR)/cloop $(SRC_DIR)/tests/test1/Interface.idl
	$(BIN_DIR)/cloop $(SRC_DIR)/tests/test1/Interface.idl c++ $@ CALC_CPP_API_H calc I

$(SRC_DIR)/tests/test1/CalcPascalApi.pas: $(BIN_DIR)/cloop \
	$(SRC_DIR)/tests/test1/Interface.idl \
	$(SRC_DIR)/tests/test1/CalcPascalApi.interface.pas \
	$(SRC_DIR)/tests/test1/CalcPascalApi.implementation.pas
	$(BIN_DIR)/cloop $(SRC_DIR)/tests/test1/Interface.idl pascal $@ CalcPascalApi \
		--uses "SysUtils" \
		--interfaceFile $(SRC_DIR)/tests/test1/CalcPascalApi.interface.pas \
		--implementationFile $(SRC_DIR)/tests/test1/CalcPascalApi.implementation.pas \
		--exceptionClass CalcException

$(SRC_DIR)/tests/test1/CppTest.cpp: $(SRC_DIR)/tests/test1/CalcCppApi.h

$(BIN_DIR)/test1-c$(SHRLIB_EXT): \
	$(OBJ_DIR)/tests/test1/CalcCApi.o \
	$(OBJ_DIR)/tests/test1/CTest.o \

	$(LD) $^ -shared $(DL_LIB) -o $@ $(LIBS)

$(BIN_DIR)/test1-c$(EXE_EXT): \
	$(OBJ_DIR)/tests/test1/CalcCApi.o \
	$(OBJ_DIR)/tests/test1/CTest.o \

	$(LD) $^ $(DL_LIB) -o $@ $(LIBS)

$(BIN_DIR)/test1-cpp$(SHRLIB_EXT): \
	$(OBJ_DIR)/tests/test1/CppTest.o \

	$(LD) $^ -shared $(DL_LIB) -o $@ $(LIBS)

$(BIN_DIR)/test1-cpp$(EXE_EXT): \
	$(OBJ_DIR)/tests/test1/CppTest.o \

	$(LD) $^ $(DL_LIB) -o $@ $(LIBS)

$(BIN_DIR)/test1-pascal$(SHRLIB_EXT): \
	$(SRC_DIR)/tests/test1/PascalClasses.pas \
	$(SRC_DIR)/tests/test1/PascalLibrary.dpr \
	$(SRC_DIR)/tests/test1/CalcPascalApi.pas \

ifeq ($(WITH_FPC),1)
	fpc $(FPC_FLAGS) -fPIC -FU$(OBJ_DIR)/tests/test1 -o$(BIN_DIR)/test1-pascal$(SHRLIB_EXT) $(SRC_DIR)/tests/test1/PascalLibrary.dpr
endif

$(BIN_DIR)/test1-pascal$(EXE_EXT): \
	$(SRC_DIR)/tests/test1/PascalClasses.pas \
	$(SRC_DIR)/tests/test1/PascalTest.dpr \
	$(SRC_DIR)/tests/test1/CalcPascalApi.pas \

ifeq ($(WITH_FPC),1)
	fpc $(FPC_FLAGS) -FU$(OBJ_DIR)/tests/test1 -o$(BIN_DIR)/test1-pascal$(EXE_EXT) $(SRC_DIR)/tests/test1/PascalTest.dpr
endif
