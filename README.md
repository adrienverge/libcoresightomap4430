libcoresightomap4430
====================

**libcoresightomap4430** is a set of libraries and examples of code to use
STM and ETB hardware tracing modules on a OMAP4430 system on chip (for
instance, on a Pandaboard).

There is no need for any driver.  This code is made for the Texas Instruments
version of STM (System Trace Module), not the ARM one; and the Embedded Trace
Buffer (ETB).  It provides user-space libraries, but you can modify it to make
kernel modules.

Usage
-----

Simply run `make` to compile all files, it will produce the `stmwrite`,
`etbread` and `stpdecode` binaries (among others).

Then send data to a STM channel:
```
# echo "tracing myevent #1" | ./stmwrite -c 10
# sleep 1
# echo "tracing myevent #2" | ./stmwrite -c 11
```

You can either export trace data off-chip via a JTAG interface, or fetch it
from the ETB hardware buffer:
```
# ./etbread > mytrace
```

Finally, decode the trace:
```
# ./stpdecode mytrace
[12.01793921] [10] tracing myevent #1
[13.34567810] [11] tracing myevent #2
```

You can also use the libraries separately from using in your own program.

Libraries
---------

- **libomap4430**

  Used to enable the EMU clock on the Pandaboard.  This clock is needed to use
  some debug facilities such as STM.

- **libstm**

  Used to send messages through the STM.

- **libetb**

  Used to read messages from the ETB buffer.

- **libstp**

  Used to decode messages read from the ETB.  The messages are encoded
  according to the System Trace Protocol (STP) version 1 format.  Since there
  is no public documentation for STP, the decoding might not work well.

Example programs
----------------

- **stmwrite**

  Program to write data to the STM.

- **etbread**

  Program to read messages collected in the ETB.

- **stpdecode**

  Program to decode a STP-formatted file (extracted with etbread, for
  instance).

- **etbdecode**

  Reads from the ETB and decode the STP stream at the same time.
