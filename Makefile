myshell: command.o tokenizer.o
	g++ -o myshell command.o tokenizer.o

command.o: command.cc command.h tokenizer.h
	g++ -c command.cc

tokenizer.o: tokenizer.cc tokenizer.h
	g++ -c tokenizer.cc

clean:
	rm -f myshell command.o tokenizer.o
