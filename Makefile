CFLAGS=-g

all: bin/test_config bin/test_data bin/test_fcgi

bin obj:
	mkdir -p $@

clean:
	rm -rf bin obj

obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -o $@ -c $<

bin/%: | bin 
	$(CC) -o $@ $^

bin/test_config: obj/test_config.o obj/config.o
bin/test_data: obj/test_data.o obj/config.o obj/data.o obj/strbuf.o -lmysqlclient -lmemcached
bin/test_fcgi: obj/test_fcgi.o obj/config.o obj/data.o obj/strbuf.o -lmysqlclient -lmemcached -lfcgi -lpthread -luriparser

