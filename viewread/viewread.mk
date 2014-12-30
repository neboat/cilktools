LIBVIEWREAD = $(LIB_DIR)/libviewread.a
VIEWREAD_SRC = viewread.c
VIEWREAD_OBJ = $(VIEWREAD_SRC:.c=.o)

-include $(VIEWREAD_OBJ:.o=.d)

.PHONY : cleanviewread

default : $(LIBVIEWREAD)
clean : cleanviewread

$(LIBVIEWREAD) : $(VIEWREAD_OBJ)

cleanviewread :
	rm -f $(LIBVIEWREAD) $(VIEWREAD_OBJ) $(VIEWREAD_OBJ:.o=.d*) *~
