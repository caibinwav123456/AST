CPPSRCS:=$(wildcard *.cpp)
CSRCS:=$(wildcard *.c)

OBJS:=$(CPPSRCS:.cpp=.o) $(CSRCS:.c=.o)
all: $(OBJS)
	@echo compile complete!

%.o: %.c depend
	@echo "(CC) $<"
	@gcc -c $< -o $@ $(CFLAGS) > /dev/null

%.o: %.cpp depend
	@echo "(CC) $<"
	@g++ -c $< -o $@ $(CFLAGS) > /dev/null

depend: $(CPPSRCS) $(CSRCS)
	@-$(RM) depend; \
	g++ -M $(CFLAGS) $^ > $@
sinclude depend

clean:
	@-$(RM) depend ./*.o ./*.a
