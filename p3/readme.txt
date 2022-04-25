to compile: make
to run: This program can run 4 main function
	1) ./diskinfo test.dmg 
	- This program will output infomation on test.img Super block and FAT

	2) ./disklist test.dmg 		      
	- This program will displays the contents of the root directory

	3) ./diskget test.dmg *file1* *file2* 
	- This program will copies a file1 from the file system to file2 in current directory in Linux
		{note if file2 doesn't exist the program will create one, but if file1 doesn't exist the program will output file not found}

	4) ./diskput test.dmg *file1* 
	- This program will copy file1 from the current Linux directory into the file system
		{file need to exist}

./diskfix will do nothing


	


