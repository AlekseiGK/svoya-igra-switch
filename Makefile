#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Установите переменную окружения DEVKITPRO")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
TARGET      :=  svoya-igra
BUILD       :=  build
SOURCES     :=  source
DATA        :=  data
INCLUDES    :=  source
ROMFS       :=  romfs

APP_TITLE   :=  Своя игра
APP_AUTHOR  :=  Своими руками
APP_VERSION :=  1.0.0
export APP_TITLE
export APP_AUTHOR
export APP_VERSION

#---------------------------------------------------------------------------------
ARCH    :=  -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

SDL_CFLAGS  :=  $(shell PKG_CONFIG_PATH=$(PORTLIBS)/lib/pkgconfig:$(LIBNX)/lib/pkgconfig pkg-config --cflags sdl2 SDL2_ttf)
SDL_LIBS    :=  $(shell PKG_CONFIG_PATH=$(PORTLIBS)/lib/pkgconfig:$(LIBNX)/lib/pkgconfig pkg-config --static --libs sdl2 SDL2_ttf)

CFLAGS  :=  -g -Wall -O2 -ffunction-sections $(ARCH) $(SDL_CFLAGS) -D__SWITCH__
CFLAGS  +=  $(INCLUDE)

CXXFLAGS    :=  $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS :=  -g $(ARCH)
LDFLAGS  =  -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS    :=  $(SDL_LIBS) -lnx -lm

#---------------------------------------------------------------------------------
LIBDIRS :=  $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   :=  $(CURDIR)/$(TARGET)
export TOPDIR   :=  $(CURDIR)

export VPATH    :=  $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                     $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  :=  $(CURDIR)/$(BUILD)

CFILES      :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES    :=  $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
    export LD   :=  $(CC)
else
    export LD   :=  $(CXX)
endif

export OFILES_BIN    :=  $(addsuffix .o,$(BINFILES))
export OFILES_SRC    :=  $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES        :=  $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN    :=  $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE  :=  $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                     $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                     -I$(CURDIR)/$(BUILD)

export LIBPATHS :=  $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(ROMFS)),)
    export NROFLAGS :=
else
    export NROFLAGS :=  --romfsdir=$(CURDIR)/$(ROMFS)
endif

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf

#---------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS :=  $(OFILES:.o=.d)

all   :   $(OUTPUT).nro

$(OUTPUT).nro : $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf : $(OFILES)

$(OFILES_SRC) : $(HFILES_BIN)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
