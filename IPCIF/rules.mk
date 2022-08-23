CPPSRCS+=$(wildcard *.cpp)
CSRCS+=$(wildcard *.c)
OBJS=$(CPPSRCS:.cpp=.o) $(CSRCS:.c=.o)

CFLAGS_LOCAL=$(CFLAGS)

all: $(OBJS)
	@echo compile complete!

%.o: %.c depend
	@echo "(CC) $<"
	@gcc -c $< -o $@ $(CFLAGS_LOCAL) > /dev/null

%.o: %.cpp depend
	@echo "(CC) $<"
	@g++ -c $< -o $@ $(CFLAGS_LOCAL) > /dev/null

depend: $(CPPSRCS) $(CSRCS)
	@-$(RM) depend; \
	g++ -M $(CFLAGS_LOCAL) $^ > $@
sinclude depend

clean:
	@-$(RM) depend $(OBJS) ./*.a
