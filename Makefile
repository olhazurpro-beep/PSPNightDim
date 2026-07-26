TARGET = nightdim
OBJS = main.o

INCDIR = 
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lpspgu -lpspdisplay -lpspctrl -lpspthreadman -lpspkernel -lpsprtc

BUILD_PRX = 1

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = NightDim

# Путь к SDK (в Docker-образе pspdev)
PSPSDK = /usr/local/pspdev/psp/sdk

include $(PSPSDK)/lib/build.mak
