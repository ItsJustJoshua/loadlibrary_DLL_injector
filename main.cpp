#include "injector.hpp"


int main(int argc, char* argv[]) {

    // check if there is the right number of arguments being passed into the program
    if (argc != 3) {
        std::cout << "Usage: this.exe <process_name> <dll_path>" << std::endl;
        return 1;
    }

	// store the arguments into variables for easier access
    std::string process_name = argv[1];
    std::string dll_path = argv[2];

	// print out the variables to the console so can check input is correct
    std::cout << "process name: " << process_name << std::endl;
    std::cout << "DLL path: " << dll_path << std::endl;

    // created instance of the injector class and pass variables into it
    loadlibrary_injector injector(process_name, dll_path);

    // run the injector and make sure its sucessful 
	if (!injector.run()) {
		std::cout << "DLL injection failed." << std::endl;
		std::cin.get(); // wait for user input before closing the program
		return 1;
	}

	// if we get here then the injection was successful
	std::cout << "DLL injection successful." << std::endl;

	// have this at the end of the program so doesnt close instantly giving time to read the output of the program
	std::cout << "Press any key to close the program..." << std::endl;
	std::cin.get();

    return 0;
}