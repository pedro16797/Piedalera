from machine import Pin, UART, SPI, I2C
from time import ticks_us,sleep_us
from ustruct import pack
import _thread
import os
import framebuf

# MIDI GUIDE:
# "bbb" for the structure of 3 bytes
# 0x9X or 0x8X are note on/off commands
# The note number as seen in https://www.inspiredacoustics.com/en/MIDI_note_numbers_and_center_frequencies

def flushThread():
    _thread.exit()
_thread.start_new_thread(flushThread,())
sleep_us(1000)

# Default config
SHINE     = 0
CHANNEL   = 0
VELOCITY  = 127
TRANSPOSE = 0
MIN_OCT   = 0
MAX_OCT   = 7
OFF_OCT   = 3
OCT_DELAY = 100000
OCT_ITERT = 500000
PERIOD    = 10000

def secago():
    file = open("config.txt", "w")
    file.write("Shine: 0 Channel: 0 Velocity: 127 Transpose: 0 Min_Oct: 0 Max_Oct: 7 Oct_Offset: 3 Oct_Delay: 100000 Oct_Iteration: 500000 Period: 10000")
    file.close()

try:
    # Read saved states
    file = open("config.txt")
    configState = file.read().split()
    file.close()
    if (len(configState) < 20):
        secago()
    else:
        SHINE     = int(configState[ 1])
        CHANNEL   = int(configState[ 3])
        VELOCITY  = int(configState[ 5])
        TRANSPOSE = int(configState[ 7])
        MIN_OCT   = int(configState[ 9])
        MAX_OCT   = int(configState[11])
        OFF_OCT   = int(configState[13])
        OCT_DELAY = int(configState[15])
        OCT_ITERT = int(configState[17])
        PERIOD    = int(configState[19])
except:
    secago()

# UART adjusted to the MIDI baudrate, using PIN 21 (GP16) as TX
uart = UART(0, 31250, tx=Pin(16, Pin.OUT))
# Button pins
OCT_UP   = Pin(17)
OCT_DOWN = Pin(18)

# List of GPIOs asigned to the notes [GPID, Octave, Chord Mode, Pressed]
# Octave is -1 if the note wasn't playing, or its octave if it was
# Chord mode is >0 if it is a setter or <0 if it is a getter, 0 when disabled
keyPins   = [[Pin( 0), -1,  0, False], # C
             [Pin( 1), -1,  0, False], # Db
             [Pin( 2), -1,  0, False], # D
             [Pin( 3), -1,  0, False], # Eb
             [Pin( 4), -1,  0, False], # E
             [Pin( 5), -1,  0, False], # F
             [Pin( 6), -1,  0, False], # Gb
             [Pin( 7), -1,  0, False], # G
             [Pin( 8), -1,  0, False], # Ab
             [Pin( 9), -1,  0, False], # A
             [Pin(10), -1,  0, False], # Bb
             [Pin(11), -1,  0, False], # B
             [Pin(12), -1,  3, False], # C'
             [Pin(13), -1,  1, False], # D'b
             [Pin(14), -1,  4, False], # D'
             [Pin(15), -1,  2, False], # E'b
             [Pin(19), -1,  6, False], # E'
             [Pin(20), -1,  7, False], # F'
             [Pin(21), -1,  5, False], # G'b
             [Pin(22), -1, 10, False]] # G'

# List of all possible chords
chordList = [["Major", [4,7]],
             ["Minor", [3,7]],
             ["Major 7th", [4,7,11]],
             ["Minor 7th", [3,7,10]],
             ["Sev (7)", [4,7,10]],
             ["Dim 7th", [3,6, 9]],
             ["H-dim 7th", [3,6,10]]]

selectedChord = 0
chordEnabled = False
holdChord = False
octaveOffset = OFF_OCT

# Variables to track the length of the octave button presses
octUpCount   = 0
octDownCount = 0
programCount = 0

# Temp variable to debug the display thread
runDisplay = True

# Control for the display of config modes
configMode = False
configText = ""

# Display thread
def displayThread():
    from ssd1305 import SSD1306_I2C
    
    global configText
    global octaveOffset
    global selectedChord
    global chordEnabled
    global runDisplay
    
    # Initialize the I2C for the display
    i2c = I2C(1, scl=Pin(27), sda=Pin(26), freq=200000)
    oled = SSD1306_I2C(i2c)
    oled.contrast(SHINE)
    dot = False
    
    # Raspberry Pi logo as 32x32 bytearray
    #buffer = bytearray(b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00|?\x00\x01\x86@\x80\x01\x01\x80\x80\x01\x11\x88\x80\x01\x05\xa0\x80\x00\x83\xc1\x00\x00C\xe3\x00\x00~\xfc\x00\x00L'\x00\x00\x9c\x11\x00\x00\xbf\xfd\x00\x00\xe1\x87\x00\x01\xc1\x83\x80\x02A\x82@\x02A\x82@\x02\xc1\xc2@\x02\xf6>\xc0\x01\xfc=\x80\x01\x18\x18\x80\x01\x88\x10\x80\x00\x8c!\x00\x00\x87\xf1\x00\x00\x7f\xf6\x00\x008\x1c\x00\x00\x0c \x00\x00\x03\xc0\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00")

    # Load the raspberry pi logo into the framebuffer (the image is 32x32)
    #fb = framebuf.FrameBuffer(buffer, 32, 32, framebuf.MONO_HLSB)

    # Blit the image from the framebuffer to the oled display
    #oled.blit(fb, 96, 0)

    while runDisplay:
        # Clear the oled display in case it has junk on it.
        oled.fill(0)
        oled.contrast(SHINE)
        
        oled.text("Octava: " + str(octaveOffset),0,0)
        oled.text(chordList[selectedChord][0] if chordEnabled else "",0,12)
        oled.text(("HOLD: " + ("ON" if holdChord else "OFF")) if chordEnabled else configText,0,24)
        
        # Finally update the oled display so the image & text is displayed
        oled.show()
        dot = not dot

        # Sleep enough to ensure the display doesn't refresh at more than 10fps
        sleep_us(100000)
    oled.fill(0)

# Turn off all the notes iterating through them
def allNotesOff():
    for note,key in enumerate(keyPins):
        if key[1] > -1:
            uart.write(pack("bbb", 0x80, note + TRANSPOSE + 12 * (key[1] + 1), 0))
            if chordEnabled and key[2] < 0:
                # Iterate through all the extra notes in the selected chord
                for index,tone in enumerate(chordList[-1 - key[2]][1]):
                    uart.write(pack("bbb", 0x80, note + TRANSPOSE + tone + 12 * (key[1] + 1), 0))
                # Reset the chord mode
                key[2] = 0
                    
            # Reset the selected octave
            key[1] = -1

_thread.start_new_thread(displayThread,())

try:
    # A loop checking for notes in their low state and octave/program control
    while True:
        # Check the starting time of the loop
        start = ticks_us()

    # -- KEY PRESS DETECTOR -----
        for note,key in enumerate(keyPins):
            # XOR so it only runs if the state has changed
            if key[0].value() ^ key[3]:
                key[3] = key[0].value()
                if not key[0].value(): # Key being pressed
                    if chordEnabled and key[2] > 0:
                        # When in chord mode first check if it's the hold flag, then if it is a chord selector
                        if key[2] > 9:
                            holdChord = not holdChord
                            if (not holdChord):
                                allNotesOff()
                        else:
                            selectedChord = key[2] - 1
                        key[1] = 0
                        break # Break the loop so no extra key presses are processed
                    else:
                        if chordEnabled and holdChord:
                            allNotesOff()
                        uart.write(pack("bbb", 0x90, note + TRANSPOSE + 12 * (octaveOffset + 1), VELOCITY))
                        # When in chord mode play all the necessary notes
                        if chordEnabled:
                            key[2] = -selectedChord - 1
                            for _,tone in enumerate(chordList[selectedChord][1]):
                                uart.write(pack("bbb", 0x90, note + TRANSPOSE + tone + 12 * (octaveOffset + 1), VELOCITY))
                        # Save the octave offset for when the key is released
                        key[1] = octaveOffset
                        
                else: # Key being released
                    if not chordEnabled:
                        uart.write(pack("bbb", 0x80, note + TRANSPOSE + 12 * (key[1] + 1), 0))
                        key[1] = -1
                    elif key[2] < 0:
                        if not holdChord:
                            allNotesOff()
                            break # Break the loop so no extra key presses are processed
                    elif key[2] > 9:
                        if chordEnabled:
                            key[1] = -1
                            break # Break the loop so no extra key presses are processed

    # -- OCTAVE PRESS DETECTOR -----
        if OCT_UP.value() or OCT_DOWN.value():
            # If they are, reset the counter
            programCount = start

            # Change the value of the octave offset if it is within its limits
            if not OCT_UP.value():
                if OCT_DELAY < start - octUpCount and octaveOffset < MAX_OCT:
                    octaveOffset += 1
                    octUpCount = start + OCT_ITERT - OCT_DELAY
            else:
                octUpCount = start
                
                if not OCT_DOWN.value():
                    if OCT_DELAY < start - octDownCount and octaveOffset > MIN_OCT:
                        octaveOffset -= 1
                        octDownCount = start + OCT_ITERT - OCT_DELAY
                else:
                    octDownCount = start
        elif not OCT_UP.value() and not OCT_DOWN.value():
            # If both are low check how long the buttons have been pressed
            octUpCount = start
            octDownCount = start
            
            if OCT_DELAY < start - programCount < 1E6:
                programCount -= 1E6
                allNotesOff()
                chordEnabled = not chordEnabled
            elif start - programCount > 2E6:
    # -- CONFIG MODE -----
                # Reset the mode, since it was changed when trying to enter config
                chordEnabled = not chordEnabled
                configMode = True
                configText = "CONFIG"
                allNotesOff()
                while True:
                    iterating = False
                    for note,key in enumerate(keyPins):
                        if not key[0].value(): # Key being pressed
                            if note == 0: # BRIGHTNESS DOWN
                                while not key[0].value():
                                    SHINE = 255 if SHINE < 16 else (SHINE - 16)
                                    configText = "Brightness " + str(round(SHINE/16))
                                    sleep_us(200000 if iterating else 500000)
                                    iterating = True
                            elif note == 2: # BRIGHTNESS UP
                                while not key[0].value():
                                    SHINE = 0 if SHINE > 239 else (SHINE + 16)
                                    configText = "Brightness " + str(round(SHINE/16))
                                    sleep_us(200000 if iterating else 500000)
                                    iterating = True
                            elif note == 4: # VELOCITY DOWN
                                while not key[0].value():
                                    VELOCITY -= 1
                                    VELOCITY = 127 if VELOCITY < 0 else VELOCITY
                                    configText = "Velocity " + str(VELOCITY)
                                    sleep_us(50000 if iterating else 200000)
                                    iterating = True
                            elif note == 5: # VELOCITY UP
                                while not key[0].value():
                                    VELOCITY += 1
                                    VELOCITY = 0 if VELOCITY > 127 else VELOCITY
                                    configText = "Velocity " + str(VELOCITY)
                                    sleep_us(50000 if iterating else 200000)
                                    iterating = True
                            elif note == 7: # CHAN DOWN
                                while not key[0].value():
                                    CHANNEL -= 1
                                    CHANNEL = 15 if CHANNEL < 0 else CHANNEL
                                    uart.write(pack("bbb", 0xB0, 0x00, 0x79)) #CC0
                                    uart.write(pack("bbb", 0xB0, 0x20, CHANNEL)) #CC32
                                    configText = "Ch" + str(CHANNEL)
                                    sleep_us(200000 if iterating else 500000)
                                    iterating = True
                            elif note == 9: # CHAN UP
                                while not key[0].value():
                                    CHANNEL += 1
                                    CHANNEL = 0 if CHANNEL > 15 else CHANNEL
                                    uart.write(pack("bbb", 0xB0, 0x00, 0x79)) #CC0
                                    uart.write(pack("bbb", 0xB0, 0x20, CHANNEL)) #CC32
                                    configText = "Ch" + str(CHANNEL)
                                    sleep_us(200000 if iterating else 500000)
                                    iterating = True
                            elif note == 11: # TRANSPOSE DOWN
                                while not key[0].value():
                                    TRANSPOSE -= 1
                                    TRANSPOSE = 12 if TRANSPOSE < -12 else TRANSPOSE
                                    configText = "Transpose " + str(TRANSPOSE)
                                    sleep_us(200000 if iterating else 500000)
                                    iterating = True
                            elif note == 12: # TRANSPOSE UP
                                while not key[0].value():
                                    TRANSPOSE += 1
                                    TRANSPOSE = -12 if TRANSPOSE > 12 else TRANSPOSE
                                    configText = "Transpose " + str(TRANSPOSE)
                                    sleep_us(100000 if iterating else 200000)
                                    iterating = True
                            else:
                                while not key[0].value():
                                    sleep_us(PERIOD)
                                break
                    else: # Continue if the inner loop wasn't broken
                        continue
                    break
                # Save the new state to config.txt
                file = open("config.txt", "w")
                file.write("Shine: {} Channel: {} Velocity: {} Transpose: {} Min_Oct: {} Max_Oct: {} Oct_Offset: {} Oct_Delay: {} Oct_Iteration: {} Period: {}"
                           .format(SHINE, CHANNEL, VELOCITY, TRANSPOSE, MIN_OCT, MAX_OCT, OFF_OCT, OCT_DELAY, OCT_ITERT, PERIOD))
                file.close()
                configMode = False
                configText = ""

        # Ensure the loop takes a fixed time
        sleep_us(PERIOD + min(0, start - ticks_us()))
except:
    runDisplay = False
