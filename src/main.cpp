#include "Camus/CliParser.hpp"
#include "Camus/Core.hpp"
#include <iostream>

int main(int argc, char** argv) {
    // CliParser is responsible for defining and parsing all command-line
    // arguments using the CLI11 library.
    Camus::CliParser parser;
    auto app = parser.setupCli();

    // CLI11_PARSE handles exceptions and prints help messages.
    // We wrap this in our own try-catch to handle CLI11's exit exceptions
    // and other potential setup errors.
    try {
        app->parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app->exit(e);
    } catch (const std::exception& e) {
        std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
        return 1;
    }

    // After parsing, we create the Core application object and run it.
    // The Core class will contain the main application logic, dispatching
    // tasks based on the parsed commands.
    Camus::Core core(parser.getCommands());
    
    try {
        return core.run();
    } catch (const std::exception& e) {
        std::cerr << "Error during execution: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

