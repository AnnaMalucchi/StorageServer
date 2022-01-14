CC=gcc
CFLAGS= -Wall -pedantic
PTHREADS= -lpthread
TARGETS= obj/FileStorageServer obj/Client

.PHONY: all

all: obj/FileStorageServer obj/Client 

obj/FileStorageServer: src/FileStorageServer.c libs/File.a libs/LogFile.a
	$(CC) -g $(CFLAGS) src/FileStorageServer.c $(PTHREADS) -L  ./libs libs/File.a libs/LogFile.a -o obj/FileStorageServer

obj/Client: src/Client.c libs/FileStorageAPI.a
	$(CC) -g $(CFLAGS) src/Client.c -L ./libs libs/FileStorageAPI.a -o obj/Client

#Creazione delle librerie
libs/FileStorageAPI.a: obj/FileStorageAPI.o
	ar rvs $@ $<

libs/File.a: obj/File.o
	ar rvs $@ $<

libs/LogFile.a: obj/LogFile.o
	ar rvs $@ $<

#Eseguibili Server
obj/File.o: src/File.c include/File.h
	$(CC) -g -c $(CFLAGS) $< -o $@

obj/LogFile.o: src/LogFile.c include/LogFile.h
	$(CC) -g -c $(CFLAGS) $< -o $@

#Eseguibili Client
obj/FileStorageAPI.o: src/FileStorageAPI.c include/FileStorageAPI.h
	$(CC) -g -c $(CFLAGS) $< -o $@

#Eliminazione eseguibili
clean :
	-rm -f $(TARGETS) 

#Elimina i file oggetto e eseguibili 
cleanall :
	-rm -f $(TARGETS) objs/*.o lib/*.a



test1 :$(TARGETS) 
	valgrind --leak-check=full obj/FileStorageServer test1/Config.txt &
	chmod +x test1/Test1.sh 
	test1/Test1.sh 



test2 :$(TARGETS) 
	valgrind --leak-check=full obj/FileStorageServer test2/Config.txt &
	chmod +x test2/Test2.sh 
	test2/Test2.sh 



test3 :$(TARGETS) 
#valgrind --leak-check=full obj/FileStorageServer test3/Config.txt &
	chmod +x test3/Test3.sh 
	test3/Test3.sh 
	
#@echo "ARRIVI QUIII\n"
#./script/statisticsTest3.sh


cleanTest1:
	@echo "------------Cleaning test1----------------"
	@rm -rf test1/Client1/ReadFile
	@rm -rf test1/Client2/ReadFile
	@rm -rf test1/Client3/ReadFile
	@rm -rf test1/Client1/FileReplacement
	@rm -rf test1/Client2/FileReplacement
	@rm -rf test1/Client3/FileReplacement
	@rm -rf test1/LogFile
#@-rm -f $(TARGETS) 
	@-rm -f obj/* libs/*
	@echo "-------Cleaning test1 complete!------"


cleanTest2:
	@echo "------------Cleaning test2----------------"
	@rm -rf test2/Client1/ReadFile
	@rm -rf test2/Client2/ReadFile
	@rm -rf test2/Client1/FileReplacement
	@rm -rf test2/Client2/FileReplacement
	@rm -rf test2/LogFile
#@-rm -f $(TARGETS) 
	@-rm -f obj/* libs/*
	@echo "-------Cleaning test2 complete!------"


cleanTest3:
	@echo "------------Cleaning test3----------------"
	@rm -rf test3/Client1/ReadFile
	@rm -rf test3/Client2/ReadFile
	@rm -rf test3/Client1/FileReplacement
	@rm -rf test3/Client2/FileReplacement
	@rm -rf test3/LogFile
#@-rm -f $(TARGETS) 
	@-rm -f obj/* libs/*
	@echo "-------Cleaning test3 complete!------"