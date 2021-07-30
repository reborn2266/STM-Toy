CPPFLAGS := -DTM_DEBUG
SRCS = stm.c tls.c

all:
	gcc -o stm -g $(CPPFLAGS) $(SRCS) -lpthread
	g++ -o x64_rtm -mrtm -std=c++17 -g x64_x.cpp -lpthread 
clean:
	rm -f *.o a.out
	rm -f -r a.out.dSYM
