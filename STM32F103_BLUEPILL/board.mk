# List of all the board related files.
BOARDSRC = STM32F103_BLUEPILL/board.c

# Required include directories
BOARDINC = STM32F103_BLUEPILL

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)
