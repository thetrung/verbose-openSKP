VERBOSE ?= /home/thetrung/Documents/verbose-lang/verbose
CXX ?= clang++
CC ?= clang
AR ?= ar

BUILD_DIR := build
LIB := $(BUILD_DIR)/libverbose_openskp.a

OPENSKP_INC := -Ithird_party/openskp/include -Ithird_party/openskp/src -Ithird_party/miniz -Icsrc
CXXFLAGS := -std=c++17 -O2 -DOPENSKP_STATIC_DEFINE $(OPENSKP_INC)
CFLAGS := -O2 -Ithird_party/miniz

OPENSKP_CPPS := \
	third_party/openskp/src/core.cpp \
	third_party/openskp/src/create.cpp \
	third_party/openskp/src/curves.cpp \
	third_party/openskp/src/earcut.cpp \
	third_party/openskp/src/errors.cpp \
	third_party/openskp/src/face_groups.cpp \
	third_party/openskp/src/geometry.cpp \
	third_party/openskp/src/instanced_scene.cpp \
	third_party/openskp/src/legacy.cpp \
	third_party/openskp/src/model.cpp \
	third_party/openskp/src/observability.cpp \
	third_party/openskp/src/pages_dimensions.cpp \
	third_party/openskp/src/parser.cpp \
	third_party/openskp/src/scene.cpp \
	third_party/openskp/src/tlv.cpp \
	third_party/openskp/src/transforms.cpp \
	third_party/openskp/src/triangulator.cpp \
	third_party/openskp/src/vff.cpp \
	csrc/verbose_openskp.cpp

MINIZ_CS := \
	third_party/miniz/miniz.c \
	third_party/miniz/miniz_tdef.c \
	third_party/miniz/miniz_tinfl.c \
	third_party/miniz/miniz_zip.c

CPP_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(OPENSKP_CPPS))
C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(MINIZ_CS))

.PHONY: all lib test clean

all: lib

lib: $(LIB)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(CPP_OBJS) $(C_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(AR) rcs $@ $^
	@echo "[READY] Built static library $@"

test: $(LIB)
	$(VERBOSE) test tests/test_openskp.vb -I src $(LIB) -lstdc++ -lm

clean:
	rm -rf $(BUILD_DIR) tests/*.ll
