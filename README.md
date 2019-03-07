# GNU_bc_with_GMP

I rewrote the math in bc number to use GMP rather than Dr. Nelson's home-grown number routines. The number.c file contains a rant by me as to the answer to FAQ question one. Also, with number.h is numbers.txt, which explains the algorithms used.
This was a quick and dirty merge. It doesn't appear broken: all of the tests in the timetest produce the same output as a distributed build of bc that I have.

One should be able to just type ./configure and then make to build it. Configure does not check to ensure that GMP is on the system, and the makefile.in's have been hand altered to statically link in GMP. Don't rebuild them with automake.

The result is faster and slower, depending on use. To show this, there is the file Test/mul2.b. One may find, as I did, that mul.b runs faster for the original code than the new code. I rewrote mul.b as mul2.b, which is faster in the new and old code. The problem is that the loop test is stored in the byte-code in decimal, and is converted from decimal to binary at every test of the condition, thus making the code slower. So, the new code is much faster when one is doing CPU intensive activities, but slower when doing IO intensive activities, and sometimes it is hard to determine that one is doing an IO intensive activity. Remember: all constants are stored in decimal in the intermediate byte-code, and have to be converted to binary every time they are encountered.

As an aside, the code for handling output bases other than 10 is rather inefficient.


Final note: my conversation with the GMP mailing list.  
Me (12/15/2011): So, I rewrote GNU bc's number system to use GMP to store and manipulate the numeric data, and I'm not sure what I should do with it. I sent it to Phil Nelson [in August], but I probably insulted him with the condition it is in (you need to manually change the make files to include libgmp). On this vein, why doesn't GMP include a fixed-point data type (either decimal or binary)? Is it too trivial for y'all to bother with? It would be handy for currency calculations, like the cost of a loaf of bread in drachmas.  
Christ Schlacta: Fixed point is what people use because their platform doesn't support floating point because it sucks.  You can approximate fixed point using mpq and always using a denominator that's a power of ten.