#---------------------------------------------------------------------------------
.SUFFIXES:
MAKEFLAGS += --no-builtin-rules
MAKEFLAGS += --no-builtin-variables
#---------------------------------------------------------------------------------

WOW64	?=
export WOW64

TOPDIR ?= $(CURDIR)

PLPREFIX?=
export PLPREFIX

LD				?= $(PLPREFIX)gcc
CC				?= $(PLPREFIX)gcc
AS				?= $(PLPREFIX)as
AR				?= $(PLPREFIX)gcc-ar
OBJCOPY			?= $(PLPREFIX)objcopy
STRIP			?= $(PLPREFIX)strip
NM				?= $(PLPREFIX)gcc-nm
RANLIB			?= $(PLPREFIX)gcc-ranlib

export LD		?= $(CC)


#---------------------------------------------------------------------------------
TARGET			:= MPGL
BUILD			:= build
SOURCES			:= src
#DATA			:= data
INCLUDES		:= inc
#ICON			:= assets/logo.png

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------

export DT := $(shell date +"%Y/%m/%d")

ifeq ($(strip $(OVERARCH)),)
ifeq ($(strip $(WOW64)),)
ARCH	:= -m64 -march=corei7 -mtune=corei7
else
ARCH	:= -m32 -march=atom -mtune=atom
endif
else
ARCH	:= $(OVERARCH)
endif

CFLAGS	:=	-g -Wall -O2 -ffast-math -ffunction-sections -fdata-sections \
			-fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions \
			-Wno-unknown-pragmas \
			$(ARCH) \
			-Wno-format

POSTFIX	:= exe

CFLAGS	+=	$(INCLUDE) -DDATETIME=\"$(DT)\"

CXXFLAGS:=	$(CFLAGS) -Wno-reorder -fno-rtti -std=gnu++11

CFLAGS	+=	-std=gnu11
OBJCFLAGS:=	$(CFLAGS)

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	:=	-g $(ARCH) -Wl,--gc-sections -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions

LIBS	:=	-lopengl32 -lgdi32 -lm -lmingw32 -lwinmm -mconsole -mwindows

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=	


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

ifeq ($(findstring -m32,$(ARCH)),)
export OUTPUT	:=	$(CURDIR)/out/$(TARGET)
else
export OUTPUT	:=	$(CURDIR)/out/$(TARGET)x86
endif
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)


CFILES		:=	$(shell find -L $(SOURCES) -name '*.c' -printf "%P\n")
MFILES		:=  $(shell find -L $(SOURCES) -name '*.m' -printf "%P\n")
SFILES		:=	$(shell find -L $(SOURCES) -name '*.s' -printf "%P\n")
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*)))

#---------------------------------------------------------------------------------

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CFILES:.c=.o) $(MFILES:.m=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)/include) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					$(foreach dir,$(SOURCES),-I$(CURDIR)/$(dir)) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					$(foreach dir,$(INCLUDES),-L$(CURDIR)/$(dir)/lib)

#---------------------------------------------------------------------------------

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@[ -d out ] || mkdir -p out
	@find -L $(SOURCES) -type d -printf "%P\0" | xargs -0 -I {} mkdir -p $(BUILD)/{}
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -rf $(BUILD) $(OUTPUT).$(POSTFIX)

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
.PHONY: all
all: $(OUTPUT).$(POSTFIX)

%.o: %.c
	@echo [CC] $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@

$(OUTPUT).$(POSTFIX): $(OFILES)
	@echo [LD] $(notdir $@)
	@$(LD) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@


#---------------------------------------------------------------------------------------
-include $(DEPENDS)
#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
