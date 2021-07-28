# m5paper
Collection of programs, tools, and scripts for the M5Paper E-Ink enabled SBC


## batery_life_tests

A colleciton of Arduino programs that test the affect various features, refresh rates, sleep durations, etc... have on the M5Paper's battery life. The goal of these experiments is to better understand how to produce a long running application for use in various home automation tasks where you may want to put one of these devices on a wall where it will not have access to line power, just its internal battery.

### test01.ino

The goal of this test is to establish a baseline from which we will make other experiements.

The application does the following:

1. When the down key is pressed for ~5 seconds, EEPROM is cleared and the current time is stored as well as a 0 for total cycles and 0 for total screen refreshes. This should only be done at the start of the test.
2. Wakes up from deep sleep
3. Does a full screen refresh
4. writes the test start time, total cycles, total refreshes, current time, and current battery volts to screen in 1 render operation.
3. Runs the main loop 10 times, incrementing total_cycles each time, with a 5 second delay between each cycle. 
4. updates the EEPROM values for total cycles and total refreshes.
5. goes into deep sleep for 60 seconds

The results of this test showed that the M5Paper could do this for ~13.5 hours before exhausting its battery. That consisted of 44500 total loops and 405 screen refreshes + clears.

### test02.ino

The goal is to understand what affect full screen refreshes have on battery life. These refreshes cause the entire e-ink display to flash white, black, white. Presumably this is a power intensive act.


The application is nearly identicate to test01.ino but without the fully screen refresh, it does the following:

1. When the down key is pressed for ~5 seconds, EEPROM is cleared and the current time is stored as well as a 0 for total cycles and 0 for total screen refreshes. This should only be done at the start of the test.
2. Wakes up from deep sleep
3. writes the test start time, total cycles, total refreshes, current time, and current battery volts to screen in 1 render operation.
4. Runs the main loop 10 times, incrementing total_cycles each time, with a 5 second delay between each cycle. 
5. updates the EEPROM values for total cycles and total refreshes.
6. goes into deep sleep for 60 seconds

