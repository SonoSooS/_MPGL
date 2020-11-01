#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

WOW64	?=
export WOW64

TOPDIR ?= $(CURDIR)

PLPREFIX?=
export PLPREFIX

LD				:= $(PLPREFIX)gcc
CC				:= $(PLPREFIX)gcc
CXX				:= $(PLPREFIX)g++
AS				:= $(PLPREFIX)as
AR				:= $(PLPREFIX)gcc-ar
OBJCOPY			:= $(PLPREFIX)objcopy
STRIP			:= $(PLPREFIX)strip
NM				:= $(PLPREFIX)gcc-nm
RANLIB			:= $(PLPREFIX)gcc-ranlib
LZZ				?= lzz

#---------------------------------------------------------------------------------
TARGET			:= MPGL#$(notdir $(TOPDIR))
BUILD			:= build
SOURCES			:= soos
#DATA			:= data
INCLUDES		:= inc
#ICON			:= assets/logo.png

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------

export DT := $(shell date +"%Y/%m/%d")

ifeq ($(strip $(WOW64)),)
#ARCH	:= -m64 -march=core2 -mtune=core2
ARCH	:= -m64 -march=corei7 -mtune=corei7
#ARCH	:= -m64 -march=core2 -mtune=corei7
#ARCH	:= -m64 -march=k8 -mtune=k8
else
ARCH	:= -m32 -march=atom -mtune=atom
endif

CFLAGS	:=	-g -Wall -Ofast -ffast-math -ffunction-sections -fdata-sections \
			$(ARCH) \
			-Wno-format -Wno-write-strings -Wno-unused-variable -Wno-unused-value \
			-Wno-deprecated-declarations -Wno-pointer-arith -Wno-sign-compare \
			-Wno-unused-but-set-variable -Wno-comment -Wno-psabi

POSTFIX	:= exe

CFLAGS	+=	$(INCLUDE) -DDATETIME=\"$(DT)\"

CXXFLAGS:=	$(CFLAGS) -Wno-reorder -fno-rtti -std=gnu++11

CFLAGS	+=	-std=gnu11
OBJCFLAGS:=	$(CFLAGS)

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	:=	-g $(ARCH) -Wl,--gc-sections

LIBS	:=	-lopengl32 -lgdi32 -lm -lmingw32 -mconsole -mwindows -static-libgcc

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

export VPATH	:=	$(foreach dir,$(SOURCES) $(BUILD)/_lzz_temp,$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(shell find -L $(SOURCES) -name '*.c' -printf "%P\n")
MFILES		:=  $(shell find -L $(SOURCES) -name '*.m' -printf "%P\n")
CPPFILES	:=	$(shell find -L $(SOURCES) -name '*.cpp' -printf "%P\n")
LPPFILES	:=	$(shell find -L $(SOURCES) -name '*.lpp' -printf "%P\n")
SFILES		:=	$(shell find -L $(SOURCES) -name '*.s' -printf "%P\n")
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*)))

LPPSOOS		:=	$(foreach fil,$(LPPFILES),$(patsubst %.lpp,%.cpp,$(fil)))
CPPFILES	+=	$(LPPSOOS)
LPPTARGET	:=	$(foreach fil,$(LPPFILES),$(patsubst %.lpp,$(TOPDIR)/$(BUILD)/_lzz_temp/%.cpp,$(fil)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(MFILES:.m=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)/include) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					$(foreach dir,$(SOURCES) $(BUILD)/_lzz_temp,-I$(CURDIR)/$(dir)) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					$(foreach dir,$(INCLUDES),-L$(CURDIR)/$(dir)/lib)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
endif


.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(TOPDIR)/$(BUILD)/_lzz_temp/%.cpp : %.lpp
	@mkdir -p $(shell dirname $@)
	@echo [LZZ] $(patsubst $(TOPDIR)/$(BUILD)/_lzz_temp/%.cpp,%.lpp,$@)
	@lzz -hx hpp -hd -sd -c -o $(shell dirname $@) $<

$(BUILD): $(LPPTARGET)
	@[ -d $@ ] || mkdir -p $@
	@[ -d out ] || mkdir -p out
	@find -L $(SOURCES) -type d -printf "%P\0" | xargs -0 -I {} mkdir -p $(BUILD)/{}
	@[ ! -d $(BUILD)/_lzz_temp ] || find $(BUILD)/_lzz_temp -type d -printf "%P\0" | xargs -0 -I {} mkdir -p $(BUILD)/{}
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -rf $(BUILD) $(TARGET).elf out/


#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
.PHONY: all
all: $(OUTPUT).$(POSTFIX)

%.o: %.cpp
	@echo [CX] $(notdir $<)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	@echo [CC] $(notdir $<)
	@$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT).$(POSTFIX): $(OFILES)
	@echo [LD] $(notdir $@)
	@$(LD) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@


#---------------------------------------------------------------------------------------
-include $(DEPENDS)
#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
