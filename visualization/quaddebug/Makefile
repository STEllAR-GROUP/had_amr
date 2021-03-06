# Makefile
########
#ARCH = `uname -s` . `uname -m`
#ARCH = Linux.i686
#SUNDIALS_DIR = /home/astro/sundials
########
# Some stuff that's the same for all programs and machines 
########

# The commands to call the C and C++ compilers
CC = gcc 
C++ = g++ 
F90 = ifort

# Where to find the include files:
INCLUDE = -I/home/manderson/rnpl/include -I/home/manderson/mpfr/include

# library directory
LIB_DIR = -L/home/manderson/rnpl/lib 

#LDLIBS = -lida.$(ARCH) -lshared.$(ARCH) -lnvecserial.$(ARCH) -lm
#LDLIBS = -lcvodes.$(ARCH) -lshared.$(ARCH) -lnvecserial.$(ARCH) -lm -lhdf5
LDLIBS = -lbbhutil  -lm /home/manderson/mpfr/lib/libmpfr.a /usr/lib64/libgmp.a

CFLAGS = $(INCLUDE) $(LIB_DIR)
########
# Stuff that depends on the specific program we're compiling
# To use this file for other programs, you'll need to change things
# in this section.
########

# The name of the output program 
PROG = debug

# Object files that go into the final executable
OBJS = debug.o mpreal.o

# Header files 
HDRS = mpreal.h

########
# Finally, the commands that actually make stuff
# You shouldn't have to change anything here 
# These commands are of the form:
#   output_thing:  stuff it depends on
#   <TAB> command to make output_thing 
########

# re-link the program when the object files change
$(PROG):  $(OBJS)
	$(C++) $(CFLAGS) $(OBJS) $(LDLIBS) -o $(PROG) 

clean:
	rm -f debug *.o *.sdf *.matlab *.idl core*
dataclean:
	rm -f *.o *.sdf *.matlab *.idl core*

# change a C object file whenever the corresponding source file
# or any of the header files changes
%.o : %.c $(HDRS) 
	$(CC) -c $(CFLAGS) $<
# What this means: 
# The %.o and %.c mean any .o file and the corresponding .c file
# All .c's are recompiled if you change one of the header files
# The $< symbol refers to the FIRST dependancy - so don't try to
# put $(HDRS) before %.c!

# change a C++ object file whenever the corresponding source file
# or any of the header files changes
%.o : %.cpp $(HDRS) 
	$(C++) -c $(CFLAGS) $<
%.o : %.f $(HDRS) 
	$(F90) -c $<
