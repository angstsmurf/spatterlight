# provisional makefile for babel-get
#
# Note that to compile babel, it is necessary only to compile all the .c
# files in this distribution and link them to the babel and ifiction
# libraries
#
# Note that this is a GNU makefile, and may not work with other makes
#

GETTER_OBJS = get_url$(OBJ) get_dir$(OBJ) get_story$(OBJ) get_ifiction$(OBJ)
# Comment/uncomment the following lines to make the program work

#CC=bcc32
#OBJ=.obj
#BABEL_LIB=..\\babel.lib
#IFICTION_LIB=..\\ifiction.lib
#OUTPUT_BGET=
#GETTER_LIB=getter.lib


CC=gcc -g
OBJ=.o
BABEL_LIB=../babel.a
IFICTION_LIB=../ifiction.a
OUTPUT_BGET=-o babel-get
GETTER_LIB = $(GETTER_OBJS)

babel-get:  babel-get$(OBJ) $(GETTER_LIB) $(BABEL_LIB) $(IFICTION_LIB)
	${CC} ${OUTPUT_BGET} $^

getter.lib: ${foreach dep,${GETTER_OBJS},${dep}.gl}

%${OBJ} : %.c
	${CC} -I.. -c $^

%.obj.gl: %.obj
	tlib getter.lib +-$^
	echo made > $@